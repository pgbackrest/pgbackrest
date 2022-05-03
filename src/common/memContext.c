/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <stdio.h> // !!! REMOVE
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Contains information about a memory allocation. This header is placed at the beginning of every memory allocation returned to the
user by memNew(), etc. The advantage is that when an allocation is passed back by the user we know the location of the allocation
header by doing some pointer arithmetic. This is much faster than searching through a list.
***********************************************************************************************************************************/
typedef struct MemContextAlloc
{
    unsigned int allocIdx:32;                                       // Index in the allocation list
    unsigned int size:32;                                           // Allocation size (4GB max)
} MemContextAlloc;

// Get the allocation buffer pointer given the allocation header pointer
#define MEM_CONTEXT_ALLOC_BUFFER(header)                            ((MemContextAlloc *)header + 1)

// Get the allocation header pointer given the allocation buffer pointer
#define MEM_CONTEXT_ALLOC_HEADER(buffer)                            ((MemContextAlloc *)buffer - 1)

// Make sure the allocation is valid for the current memory context.  This check only works correctly if the allocation is valid but
// belongs to another context.  Otherwise, there is likely to be a segfault.
#define ASSERT_ALLOC_VALID(alloc)                                                                                                  \
    ASSERT(                                                                                                                        \
        alloc != NULL && (uintptr_t)alloc != (uintptr_t)-sizeof(MemContextAlloc) &&                                                \
        alloc->allocIdx < memContextStack[memContextCurrentStackIdx].memContext->allocListSize &&                                  \
        memContextStack[memContextCurrentStackIdx].memContext->allocList[alloc->allocIdx]);

/***********************************************************************************************************************************
Contains information about the memory context !!! MORE COMMENTS ON STRUCTURES
***********************************************************************************************************************************/
struct MemContext
{
#ifdef DEBUG
    const char *name;                                               // Indicates what the context is being used for
#endif
    bool active:1;                                                  // Is the context currently active?
    MemType allocType:2;                                            // How many allocations can this context have?
    bool allocInitialized:1;                                        // Has the allocation list been initialized?
    MemType childType:2;                                            // How many child contexts can this context have?
    bool childInitialized:1;                                        // Has the child contest list been initialized?
    MemType callbackType:2;                                         // How many callbacks can this context have?
    bool callbackInitialized:1;                                     // Has the callback been initialized?
    size_t allocExtra:16;                                           // Size of extra allocation (1kB max)

    unsigned int contextParentIdx;                                  // Index in the parent context list
    MemContext *contextParent;                                      // All contexts have a parent except top
};

typedef struct MemContextAllocOne
{
    MemContextAlloc *alloc;                                         // Memory allocation created in this context
} MemContextAllocOne;

typedef struct MemContextAllocMany
{
    MemContextAlloc **list;                                         // List of memory allocations created in this context
    unsigned int listSize;                                          // Size of alloc list (not the actual count of allocations)
    unsigned int freeIdx;                                           // Index of first free space in the alloc list
} MemContextAllocMany;

typedef struct MemContextChildOne
{
    MemContext *context;                                            // Context created in this context
} MemContextChildOne;

typedef struct MemContextChildMany
{
    MemContext **list;                                              // List of contexts created in this context
    unsigned int listSize;                                          // Size of child context list (not the actual count of contexts)
    unsigned int freeIdx;                                           // Index of first free space in the context list
} MemContextChildMany;

typedef struct MemContextCallback
{
    void (*function)(void *);                                       // Function to call before the context is freed
    void *argument;                                                 // Argument to pass to callback function
} MemContextCallback;

/***********************************************************************************************************************************
Possible sizes for the manifest based on options. Formatting has been compressed to save space.
***********************************************************************************************************************************/
static const uint8_t memContextSizePossible[memTypeMany + 1][memTypeMany + 1][memTypeOne + 1] =
{
    // alloc none
    {// child none
     {/* callback none */ 0, /* callback one */ sizeof(MemContextCallback)},
     // child one
     {/* callback none */ sizeof(MemContextChildOne), /* callback one */ sizeof(MemContextChildOne) + sizeof(MemContextCallback)},
     // child many
     {/* callback none */ sizeof(MemContextChildMany),
      /* callback one */ sizeof(MemContextChildMany) + sizeof(MemContextCallback)}},
    // alloc one
    {// child none
     {/* callback none */ sizeof(MemContextAllocOne), /* callback one */ sizeof(MemContextAllocOne) + sizeof(MemContextCallback)},
     // child one
     {/* callback none */ sizeof(MemContextAllocOne) + sizeof(MemContextChildOne),
      /* callback one */ sizeof(MemContextAllocOne) + sizeof(MemContextChildOne) + sizeof(MemContextCallback)},
     // child many
     {/* callback none */ sizeof(MemContextAllocOne) + sizeof(MemContextChildMany),
      /* callback one */ sizeof(MemContextAllocOne) + sizeof(MemContextChildMany) + sizeof(MemContextCallback)}},
    // alloc many
    {// child none
     {/* callback none */ sizeof(MemContextAllocMany), /* callback one */ sizeof(MemContextAllocMany) + sizeof(MemContextCallback)},
     // child one
     {/* callback none */ sizeof(MemContextAllocMany) + sizeof(MemContextChildOne),
      /* callback one */ sizeof(MemContextAllocMany) + sizeof(MemContextChildOne) + sizeof(MemContextCallback)},
     // child many
     {/* callback none */ sizeof(MemContextAllocMany) + sizeof(MemContextChildMany),
      /* callback one */ sizeof(MemContextAllocMany) + sizeof(MemContextChildMany) + sizeof(MemContextCallback)}},
};

/***********************************************************************************************************************************
Get pointers to optional parts of the manifest
***********************************************************************************************************************************/
// Allocation parts
#define MEM_CONTEXT_ALLOC_OFFSET(memContext)                        ((unsigned char *)(memContext + 1) + memContext->allocExtra)

static MemContextAllocOne *
memContextAllocOne(MemContext *const memContext) // {uncovered}
{
    return (MemContextAllocOne *)MEM_CONTEXT_ALLOC_OFFSET(memContext); // {uncovered}
}

static MemContextAllocMany *
memContextAllocMany(MemContext *const memContext)
{
    return (MemContextAllocMany *)MEM_CONTEXT_ALLOC_OFFSET(memContext);
}

// Child parts
#define MEM_CONTEXT_CHILD_OFFSET(memContext)                                                                                       \
    ((unsigned char *)(memContext + 1) + memContextSizePossible[memContext->allocType][0][0] + memContext->allocExtra)

static MemContextChildOne *
memContextChildOne(MemContext *const memContext) // {uncovered}
{
    return (MemContextChildOne *)MEM_CONTEXT_CHILD_OFFSET(memContext); // {uncovered}
}

static MemContextChildMany *
memContextChildMany(MemContext *const memContext)
{
    return (MemContextChildMany *)MEM_CONTEXT_CHILD_OFFSET(memContext);
}

// Callback part
static MemContextCallback *
memContextCallback(MemContext *const memContext)
{
    return
        (MemContextCallback *)((unsigned char *)(memContext + 1) +
            memContextSizePossible[memContext->allocType][memContext->childType][0] + memContext->allocExtra);
}

/***********************************************************************************************************************************
Top context

The top context always exists and can never be freed.  All other contexts are children of the top context. The top context is
generally used to allocate memory that exists for the life of the program.
***********************************************************************************************************************************/
static struct MemContextTop
{
    MemContext memContext;
    MemContextAllocMany memContextAllocMany;
    MemContextChildMany memContextChildMany;
} contextTop =
{
    .memContext =
    {
#ifdef DEBUG
        .name = "TOP",
#endif
        .active = true,
        .allocType = memTypeMany,
        .childType = memTypeMany,
    },
};

/***********************************************************************************************************************************
Memory context stack types
***********************************************************************************************************************************/
typedef enum
{
    memContextStackTypeSwitch = 0,                                  // Context can be switched to allocate mem for new variables
    memContextStackTypeNew,                                         // Context to be tracked for error handling - cannot switch to
} MemContextStackType;

/***********************************************************************************************************************************
Mem context stack used to pop mem contexts and cleanup after an error
***********************************************************************************************************************************/
#define MEM_CONTEXT_STACK_MAX                                       128

static struct MemContextStack
{
    MemContext *memContext;
    MemContextStackType type;
    unsigned int tryDepth;
} memContextStack[MEM_CONTEXT_STACK_MAX] = {{.memContext = (MemContext *)&contextTop}};

static unsigned int memContextCurrentStackIdx = 0;
static unsigned int memContextMaxStackIdx = 0;

/***********************************************************************************************************************************
Wrapper around malloc() with error handling
***********************************************************************************************************************************/
static void *
memAllocInternal(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate memory
    void *buffer = malloc(size);

    // Error when malloc fails
    if (buffer == NULL)
        THROW_FMT(MemoryError, "unable to allocate %zu bytes", size);

    // Return the buffer
    FUNCTION_TEST_RETURN_P(VOID, buffer);
}

/***********************************************************************************************************************************
Allocate an array of pointers and set all entries to NULL
***********************************************************************************************************************************/
static void *
memAllocPtrArrayInternal(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate memory
    void **buffer = memAllocInternal(size * sizeof(void *));

    // Set all pointers to NULL
    for (size_t ptrIdx = 0; ptrIdx < size; ptrIdx++)
        buffer[ptrIdx] = NULL;

    // Return the buffer
    FUNCTION_TEST_RETURN_P(VOID, buffer);
}

/***********************************************************************************************************************************
Wrapper around realloc() with error handling
***********************************************************************************************************************************/
static void *
memReAllocInternal(void *bufferOld, size_t sizeNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
    FUNCTION_TEST_END();

    ASSERT(bufferOld != NULL);

    // Allocate memory
    void *bufferNew = realloc(bufferOld, sizeNew);

    // Error when realloc fails
    if (bufferNew == NULL)
        THROW_FMT(MemoryError, "unable to reallocate %zu bytes", sizeNew);

    // Return the buffer
    FUNCTION_TEST_RETURN_P(VOID, bufferNew);
}

/***********************************************************************************************************************************
Wrapper around realloc() with error handling
***********************************************************************************************************************************/
static void *
memReAllocPtrArrayInternal(void *bufferOld, size_t sizeOld, size_t sizeNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
    FUNCTION_TEST_END();

    // Allocate memory
    void **bufferNew = memReAllocInternal(bufferOld, sizeNew * sizeof(void *));

    // Set all new pointers to NULL
    for (size_t ptrIdx = sizeOld; ptrIdx < sizeNew; ptrIdx++)
        bufferNew[ptrIdx] = NULL;

    // Return the buffer
    FUNCTION_TEST_RETURN_P(VOID, bufferNew);
}

/***********************************************************************************************************************************
Wrapper around free()
***********************************************************************************************************************************/
static void
memFreeInternal(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    free(buffer);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Find space for a new mem context
***********************************************************************************************************************************/
static unsigned int
memContextNewIndex(MemContext *const context, MemContextChildMany *memContextChild)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, context);
        FUNCTION_TEST_PARAM_P(VOID, memContextChild);
    FUNCTION_TEST_END();

    ASSERT(memContextChild != NULL);

    // Initialize
    if (!context->childInitialized)
    {
        *memContextChild = (MemContextChildMany)
        {
            .list = memAllocPtrArrayInternal(MEM_CONTEXT_INITIAL_SIZE),
            .listSize = MEM_CONTEXT_INITIAL_SIZE,
        };

        context->childInitialized = true;
    }
    else
    {
        // Try to find space for the new context
        for (; memContextChild->freeIdx < memContextChild->listSize; memContextChild->freeIdx++)
        {
            if (memContextChild->list[memContextChild->freeIdx] == NULL)
                break;
        }

        // If no space was found then allocate more
        if (memContextChild->freeIdx == memContextChild->listSize)
        {
            // Calculate new list size
            const unsigned int listSizeNew = memContextChild->listSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            memContextChild->list = memReAllocPtrArrayInternal(memContextChild->list, memContextChild->listSize, listSizeNew);

            // Set new list size
            memContextChild->listSize = listSizeNew;
        }
    }

    FUNCTION_TEST_RETURN(UINT, memContextChild->freeIdx);
}

/**********************************************************************************************************************************/
MemContext *
memContextNew(
#ifdef DEBUG
    const char *const name,
#endif
    const MemContextNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
        FUNCTION_TEST_PARAM(ENUM, param.allocType);
        FUNCTION_TEST_PARAM(ENUM, param.childType);
        FUNCTION_TEST_PARAM(BOOL, param.callbackType);
        FUNCTION_TEST_PARAM(SIZE, param.allocExtra);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);
    ASSERT(param.allocType <= memTypeMany);
    ASSERT(param.childType <= memTypeMany);
    ASSERT(param.callbackType <= memTypeOne);
    // Check context name length
    ASSERT(name[0] != '\0');

    // Fix alignment !!! WORTH MAKING THIS RIGHT? THE SYSTEM WILL PROBABLY ALIGN ANYWAY !!! ALSO FIND ALIGN MACRO
    size_t allocExtra = param.allocExtra;

    if (allocExtra % sizeof(void *) != 0 && //{uncovered}
        (param.allocType != memTypeNone || param.childType != memTypeNone || param.callbackType != memTypeNone)) //{uncovered}
    {
        allocExtra += sizeof(void *) - allocExtra % sizeof(void *); //{uncovered}
    }

    // Create the new context
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->childType != memTypeNone);

    MemContext *const this = memAllocInternal(
        sizeof(MemContext) + allocExtra + memContextSizePossible[param.allocType][param.childType][param.callbackType]);

    *this = (MemContext)
    {
#ifdef DEBUG
        // Set the context name
        .name = name,
#endif
        // Set new context active
        .active = true,

        // Set flags
        .allocType = param.allocType,
        .childType = param.childType,
        .callbackType = param.callbackType,

        // Set extra allocation
        .allocExtra = (uint16_t)allocExtra,

        // Set current context as the parent
        .contextParent = contextCurrent,
    };

    // Find space for the new context
    if (contextCurrent->childType == memTypeOne) // {uncovered}
    {
        ASSERT(!contextCurrent->childInitialized || memContextChildOne(contextCurrent)->context == NULL); // {uncovered}

        memContextChildOne(contextCurrent)->context = this; // {uncovered}
        contextCurrent->childInitialized = true; // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->childType == memTypeMany);
        MemContextChildMany *const memContextChild = memContextChildMany(contextCurrent);

        this->contextParentIdx = memContextNewIndex(contextCurrent, memContextChild);
        memContextChild->list[this->contextParentIdx] = this;

        // Possible free context must be in the next position
        memContextChild->freeIdx++;
    }

    // Add to the mem context stack so it will be automatically freed on error if memContextKeep() has not been called
    memContextMaxStackIdx++;

    memContextStack[memContextMaxStackIdx] = (struct MemContextStack)
    {
        .memContext = this,
        .type = memContextStackTypeNew,
        .tryDepth = errorTryDepth(),
    };

    // Return context
    FUNCTION_TEST_RETURN(MEM_CONTEXT, this);
}

/**********************************************************************************************************************************/
void *
memContextAllocExtra(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->allocExtra != 0);

    FUNCTION_TEST_RETURN_P(VOID, this + 1);
}

/**********************************************************************************************************************************/
MemContext *
memContextFromAllocExtra(void *const allocExtra)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, allocExtra);
    FUNCTION_TEST_END();

    ASSERT(allocExtra != NULL);
    ASSERT(((MemContext *)allocExtra - 1)->allocExtra != 0);

    FUNCTION_TEST_RETURN(MEM_CONTEXT, (MemContext *)allocExtra - 1);
}

const MemContext *
memContextConstFromAllocExtra(const void *const allocExtra)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, allocExtra);
    FUNCTION_TEST_END();

    ASSERT(allocExtra != NULL);
    ASSERT(((MemContext *)allocExtra - 1)->allocExtra != 0);

    FUNCTION_TEST_RETURN(MEM_CONTEXT, (MemContext *)allocExtra - 1);
}

/**********************************************************************************************************************************/
void
memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *callbackArgument)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_TEST_PARAM_P(VOID, callbackArgument);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->active);
    ASSERT(callbackFunction != NULL);
    ASSERT(this->active);
    ASSERT(this->callbackType != memTypeNone);

#ifdef DEBUG
    // Error if callback has already been set - there may be valid use cases for this in the future but error until one is found
    if (this->callbackInitialized)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);
#endif

    // Set callback function and argument
    *(memContextCallback(this)) = (MemContextCallback){.function = callbackFunction, .argument = callbackArgument};
    this->callbackInitialized = true;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextCallbackClear(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->callbackType != memTypeNone);
    ASSERT(this->active);

    // Top context cannot have a callback !!! IS THIS NEEDED? this->callbackType != memTypeNone SHOULD BE ENOUGH
    ASSERT(this != (MemContext *)&contextTop);

    // Clear callback function and argument
    *(memContextCallback(this)) = (MemContextCallback){0};
    this->callbackInitialized = false;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Find an available slot in the memory context's allocation list and allocate memory
***********************************************************************************************************************************/
static MemContextAlloc *
memContextAllocNew(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate memory
    MemContextAlloc *const result = memAllocInternal(sizeof(MemContextAlloc) + size);

    // Find space for the new allocation
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->allocType != memTypeNone);

    // fprintf(stdout, "!!!HERE\n"); fflush(stdout);

    if (contextCurrent->allocType == memTypeOne) // {uncovered}
    {
        MemContextAllocOne *const contextAlloc = memContextAllocOne(contextCurrent); // {uncovered}
        ASSERT(!contextCurrent->allocInitialized || contextAlloc->alloc == NULL); // {uncovered}

        // Initialize allocation header
        *result = (MemContextAlloc){.size = (unsigned int)(sizeof(MemContextAlloc) + size)}; // {uncovered}

        // Set pointer in allocation
        contextAlloc->alloc = result; // {uncovered}
        contextCurrent->allocInitialized = true; // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->allocType == memTypeMany);

        MemContextAllocMany *const contextAlloc = memContextAllocMany(contextCurrent);

        // Initialize
        if (!contextCurrent->allocInitialized)
        {
            *contextAlloc = (MemContextAllocMany)
            {
                .list = memAllocPtrArrayInternal(MEM_CONTEXT_ALLOC_INITIAL_SIZE),
                .listSize = contextAlloc->listSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE,
            };

            contextCurrent->allocInitialized = true;
        }
        else
        {
            for (; contextAlloc->freeIdx < contextAlloc->listSize; contextAlloc->freeIdx++)
                if (contextAlloc->list[contextAlloc->freeIdx] == NULL)
                    break;

            // If no space was found then allocate more
            if (contextAlloc->freeIdx == contextAlloc->listSize)
            {
                // Calculate new list size
                unsigned int listSizeNew = contextAlloc->listSize * 2;

                // Reallocate memory before modifying anything else in case there is an error
                contextAlloc->list = memReAllocPtrArrayInternal(contextAlloc->list, contextAlloc->listSize, listSizeNew);

                // Set new size
                contextAlloc->listSize = listSizeNew;
            }
        }

        // Initialize allocation header
        *result = (MemContextAlloc){.allocIdx = contextAlloc->freeIdx, .size = (unsigned int)(sizeof(MemContextAlloc) + size)};

        // Set pointer in allocation list
        contextAlloc->list[contextAlloc->freeIdx] = result;

        // Update free index to next location. This location may not be free but it is where the search should start next time.
        contextAlloc->freeIdx++;
    }

    FUNCTION_TEST_RETURN_TYPE_P(MemContextAlloc, result);
}

/***********************************************************************************************************************************
Resize memory that has already been allocated
***********************************************************************************************************************************/
static MemContextAlloc *
memContextAllocResize(MemContextAlloc *alloc, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, alloc);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // ASSERT_ALLOC_VALID(alloc); // !!! FIX THIS?

    // Resize the allocation
    alloc = memReAllocInternal(alloc, sizeof(MemContextAlloc) + size);
    alloc->size = (unsigned int)(sizeof(MemContextAlloc) + size);

    // Update pointer in allocation list in case the realloc moved the allocation
    MemContext *const currentContext = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(currentContext->allocType != memTypeNone);
    ASSERT(currentContext->allocInitialized);

    if (currentContext->allocType == memTypeOne) // {uncovered}
    {
        ASSERT(memContextAllocOne(currentContext)->alloc != NULL); // {uncovered}
        memContextAllocOne(currentContext)->alloc = alloc; // {uncovered}
    }
    else
    {
        ASSERT(currentContext->allocType == memTypeMany);
        memContextAllocMany(currentContext)->list[alloc->allocIdx] = alloc;
    }

    FUNCTION_TEST_RETURN_TYPE_P(MemContextAlloc, alloc);
}

/**********************************************************************************************************************************/
void *
memNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    void *result = MEM_CONTEXT_ALLOC_BUFFER(memContextAllocNew(size));

    FUNCTION_TEST_RETURN_P(VOID, result);
}

/**********************************************************************************************************************************/
void *
memNewPtrArray(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate pointer array
    void **buffer = (void **)MEM_CONTEXT_ALLOC_BUFFER(memContextAllocNew(size * sizeof(void *)));

    // Set pointers to NULL
    for (size_t ptrIdx = 0; ptrIdx < size; ptrIdx++)
        buffer[ptrIdx] = NULL;

    FUNCTION_TEST_RETURN_P(VOID, buffer);
}

/**********************************************************************************************************************************/
void *
memResize(const void *buffer, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_P(VOID, MEM_CONTEXT_ALLOC_BUFFER(memContextAllocResize(MEM_CONTEXT_ALLOC_HEADER(buffer), size)));
}

/**********************************************************************************************************************************/
void
memFree(void *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    // ASSERT_ALLOC_VALID(MEM_CONTEXT_ALLOC_HEADER(buffer)); // FIX THIS?

    // Get the allocation
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->allocType != memTypeNone);
    ASSERT(contextCurrent->allocInitialized);
    MemContextAlloc *const alloc = MEM_CONTEXT_ALLOC_HEADER(buffer);

    if (contextCurrent->allocType == memTypeOne) // {uncovered}
    {
        // ASSERT(memContextAllocOne(currentContext)->alloc != NULL); // {uncovered}
        memContextAllocOne(contextCurrent)->alloc = NULL; // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->allocType == memTypeMany);
        MemContextAllocMany *const contextAlloc = memContextAllocMany(contextCurrent);

        // If this allocation is before the current free allocation then make it the current free allocation
        if (alloc->allocIdx < contextAlloc->freeIdx)
            contextAlloc->freeIdx = alloc->allocIdx;

        // Free the allocation
        contextAlloc->list[alloc->allocIdx] = NULL;
    }

    memFreeInternal(alloc);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextMove(MemContext *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    // Only move if a valid mem context is provided and the old and new parents are not the same
    if (this != NULL && this->contextParent != parentNew)
    {
        ASSERT(this->active);
        ASSERT(this->contextParent->active);
        ASSERT(this->contextParent->childType != memTypeNone);
        ASSERT(this->contextParent->childInitialized);

        // Null out the context in the old parent
        if (this->contextParent->childType == memTypeOne) // {uncovered}
        {
            ASSERT(memContextChildOne(this->contextParent)->context != NULL); // {uncovered}
            memContextChildOne(this->contextParent)->context = NULL; // {uncovered}
        }
        else
        {
            ASSERT(this->contextParent->childType == memTypeMany);
            ASSERT(memContextChildMany(this->contextParent)->list[this->contextParentIdx] == this);

            memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;
        }

        // Find a place in the new parent context and assign it. The child list may be moved while finding a new index so store the
        // index and use it with (what might be) the new pointer.
        ASSERT(parentNew->active);
        ASSERT(parentNew->childType != memTypeNone);

        if (parentNew->childType == memTypeOne) // {uncovered}
        {
            ASSERT(!parentNew->childInitialized || memContextChildOne(parentNew)->context == NULL); // {uncovered}

            memContextChildOne(parentNew)->context = this; // {uncovered}
            parentNew->childInitialized = true; // {uncovered}
        }
        else
        {
            ASSERT(parentNew->childType == memTypeMany);
            MemContextChildMany *const memContextChild = memContextChildMany(parentNew);

            this->contextParent = parentNew;
            this->contextParentIdx = memContextNewIndex(parentNew, memContextChild);
            memContextChild->list[this->contextParentIdx] = this;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextSwitch(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->active);
    ASSERT(memContextCurrentStackIdx < MEM_CONTEXT_STACK_MAX - 1);

    memContextMaxStackIdx++;
    memContextCurrentStackIdx = memContextMaxStackIdx;

    // Add memContext to the stack as a context that can be used for memory allocation
    memContextStack[memContextCurrentStackIdx] = (struct MemContextStack)
    {
        .memContext = this,
        .type = memContextStackTypeSwitch,
        .tryDepth = errorTryDepth(),
    };

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextSwitchBack(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextCurrentStackIdx > 0);

    // Generate a detailed error to help with debugging
#ifdef DEBUG
    if (memContextStack[memContextMaxStackIdx].type == memContextStackTypeNew)
    {
        THROW_FMT(
            AssertError, "current context expected but new context '%s' found",
            memContextStack[memContextMaxStackIdx].memContext->name);
    }
#endif

    ASSERT(memContextCurrentStackIdx == memContextMaxStackIdx);

    memContextMaxStackIdx--;
    memContextCurrentStackIdx--;

    // memContext of type New cannot be the current context so keep going until we find a memContext we can switch to as the current
    // context
    while (memContextStack[memContextCurrentStackIdx].type == memContextStackTypeNew)
        memContextCurrentStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextKeep(void)
{
    FUNCTION_TEST_VOID();

    // Generate a detailed error to help with debugging
#ifdef DEBUG
    if (memContextStack[memContextMaxStackIdx].type != memContextStackTypeNew)
    {
        THROW_FMT(
            AssertError, "new context expected but current context '%s' found",
            memContextStack[memContextMaxStackIdx].memContext->name);
    }
#endif

    memContextMaxStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
memContextDiscard(void)
{
    FUNCTION_TEST_VOID();

    // Generate a detailed error to help with debugging
#ifdef DEBUG
    if (memContextStack[memContextMaxStackIdx].type != memContextStackTypeNew)
    {
        THROW_FMT(
            AssertError, "new context expected but current context '%s' found",
            memContextStack[memContextMaxStackIdx].memContext->name);
    }
#endif

    memContextFree(memContextStack[memContextMaxStackIdx].memContext);
    memContextMaxStackIdx--;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
MemContext *
memContextTop(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(MEM_CONTEXT, (MemContext *)&contextTop);
}

/**********************************************************************************************************************************/
MemContext *
memContextCurrent(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(MEM_CONTEXT, memContextStack[memContextCurrentStackIdx].memContext);
}

/**********************************************************************************************************************************/
MemContext *
memContextPrior(void)
{
    FUNCTION_TEST_VOID();

    ASSERT(memContextCurrentStackIdx > 0);

    unsigned int priorIdx = 1;

    while (memContextStack[memContextCurrentStackIdx - priorIdx].type == memContextStackTypeNew)
        priorIdx++;

    FUNCTION_TEST_RETURN(MEM_CONTEXT, memContextStack[memContextCurrentStackIdx - priorIdx].memContext);
}

/**********************************************************************************************************************************/
size_t
memContextSize(const MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->active);

    // Size of struct and extra
    size_t total = 0;
    const unsigned char *offset = (unsigned char *)(this + 1) + this->allocExtra;

    // Size of allocations
    if (this->allocType == memTypeOne) // {uncovered}
    {
        const MemContextAllocOne *const contextAlloc = (const MemContextAllocOne *const)offset; // {uncovered}

        if (contextAlloc->alloc != NULL) // {uncovered}
            total += contextAlloc->alloc->size; // {uncovered}

        offset += sizeof(MemContextAllocOne); // {uncovered}
    }
    else
    {
        if (this->allocInitialized) // {uncovered}
        {
            const MemContextAllocMany *const contextAlloc = (const MemContextAllocMany *const)offset; // {uncovered}

            for (unsigned int allocIdx = 0; allocIdx < contextAlloc->listSize; allocIdx++) // {uncovered}
            {
                if (contextAlloc->list[allocIdx] != NULL) // {uncovered}
                    total += contextAlloc->list[allocIdx]->size; // {uncovered}
            }

            total += contextAlloc->listSize * sizeof(MemContextAllocMany *); // {uncovered}
        }

        offset += sizeof(MemContextAllocMany); // {uncovered}
    }

    // Size of child contexts
    if (this->childType == memTypeOne) // {uncovered}
    {
        const MemContextChildOne *const contextChild = (const MemContextChildOne *const)offset; // {uncovered}

        if (contextChild->context != NULL) // {uncovered}
            total += memContextSize(contextChild->context); // {uncovered}

        offset += sizeof(MemContextChildOne); // {uncovered}
    }
    else
    {
        if (this->childInitialized)
        {
            const MemContextChildMany *const contextChild = (const MemContextChildMany *const)offset;

            // Add child contexts
            for (unsigned int contextIdx = 0; contextIdx < contextChild->listSize; contextIdx++)
            {
                if (contextChild->list[contextIdx] != NULL)
                    total += memContextSize(contextChild->list[contextIdx]);
            }

            total += contextChild->listSize * sizeof(MemContextChildMany *);
        }

        offset += sizeof(MemContextChildMany);
    }

    // Size of callback accounting
    if (this->callbackType != memTypeNone)
        offset += sizeof(MemContextCallback);

    FUNCTION_TEST_RETURN(SIZE, (size_t)(offset - (unsigned char*)this) + total);
}

/**********************************************************************************************************************************/
void
memContextClean(unsigned int tryDepth)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, tryDepth);
    FUNCTION_TEST_END();

    ASSERT(tryDepth > 0);

    // Iterate through everything pushed to the stack since the last try
    while (memContextStack[memContextMaxStackIdx].tryDepth >= tryDepth)
    {
        // Free memory contexts that were not kept
        if (memContextStack[memContextMaxStackIdx].type == memContextStackTypeNew)
        {
            memContextFree(memContextStack[memContextMaxStackIdx].memContext);
        }
        // Else find the prior context and make it the current context
        else
        {
            memContextCurrentStackIdx--;

            while (memContextStack[memContextCurrentStackIdx].type == memContextStackTypeNew)
                memContextCurrentStackIdx--;
        }

        memContextMaxStackIdx--;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Execute callbacks for the context and all its childrens
***********************************************************************************************************************************/
static void
memContextCallbackRecurse(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Certain actions against the context are no longer allowed
    this->active = false;

    // Callback
    if (this->callbackInitialized)
    {
        MemContextCallback *const callback = memContextCallback(this);
        callback->function(callback->argument);
        this->callbackInitialized = false;
    }

    // Child callbacks
    if (this->childInitialized)
    {
        if (this->childType == memTypeOne) // {uncovered}
        {
            MemContextChildOne *const memContextChild = memContextChildOne(this); // {uncovered}

            if (memContextChild->context != NULL) // {uncovered}
                memContextCallbackRecurse(memContextChild->context); // {uncovered}
        }
        else
        {
            ASSERT(this->childType == memTypeMany);
            MemContextChildMany *const memContextChild = memContextChildMany(this);

            for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
            {
                if (memContextChild->list[contextIdx] != NULL)
                    memContextCallbackRecurse(memContextChild->list[contextIdx]);
            }
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
memContextFreeRecurse(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

#ifdef DEBUG
    // Current context cannot be freed unless it is top (top is never really freed, just the stuff under it)
    if (this == memContextStack[memContextCurrentStackIdx].memContext && this != (MemContext *)&contextTop)
        THROW_FMT(AssertError, "cannot free current context '%s'", this->name);
#endif

    // Free child contexts
    if (this->childInitialized)
    {
        if (this->childType == memTypeOne) // {uncovered}
        {
            MemContextChildOne *const memContextChild = memContextChildOne(this); // {uncovered}

            if (memContextChild->context != NULL) // {uncovered}
                memContextFreeRecurse(memContextChild->context); // {uncovered}
        }
        else
        {
            ASSERT(this->childType == memTypeMany);
            MemContextChildMany *const memContextChild = memContextChildMany(this);

            for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
            {
                if (memContextChild->list[contextIdx] != NULL)
                    memContextFreeRecurse(memContextChild->list[contextIdx]);
            }
        }
    }

    // Free child context allocation list
    if (this->childInitialized)
    {
        if (this->childType == memTypeMany) // {uncovered}
            memFreeInternal(memContextChildMany(this)->list);

        this->childInitialized = false;
    }

    // Free memory allocations and list
    if (this->allocInitialized)
    {
        ASSERT(this->allocType != memTypeNone);

        if (this->allocType == memTypeOne) // {uncovered}
        {
            MemContextAllocOne *const contextAlloc = memContextAllocOne(this); // {uncovered}

            if (contextAlloc->alloc != NULL) // {uncovered}
                memFreeInternal(contextAlloc->alloc); // {uncovered}
        }
        else
        {
            ASSERT(this->allocType == memTypeMany);

            MemContextAllocMany *const contextAlloc = memContextAllocMany(this);

            for (unsigned int allocIdx = 0; allocIdx < contextAlloc->listSize; allocIdx++)
                if (contextAlloc->list[allocIdx] != NULL)
                    memFreeInternal(contextAlloc->list[allocIdx]);

            memFreeInternal(contextAlloc->list);
        }

        this->allocInitialized = false;
    }

    // If the context index is lower than the current free index in the parent then replace it
    if (this->contextParent != NULL && this->contextParent->childType == memTypeMany) // {uncovered}
    {
        MemContextChildMany *const memContextChild = memContextChildMany(this->contextParent);

        if (this->contextParentIdx < memContextChild->freeIdx)
            memContextChild->freeIdx = this->contextParentIdx;
    }

    // Make top context active again
    if (this == (MemContext *)&contextTop)
    {
        this->active = true;
    }
    // Else free the memory context so the slot can be reused
    else
    {
        ASSERT(this->contextParent != NULL);

        if (this->contextParent->childType == memTypeOne) // {uncovered}
            memContextChildOne(this->contextParent)->context = NULL; // {uncovered}
        else
            memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;

        memFreeInternal(this);
    }

    FUNCTION_TEST_RETURN_VOID();
}

void
memContextFree(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->active);

    // Execute callbacks
    TRY_BEGIN()
    {
        memContextCallbackRecurse(this);
    }
    // Finish cleanup even if a callback fails
    FINALLY()
    {
        memContextFreeRecurse(this);
    }
    TRY_END();

    FUNCTION_TEST_RETURN_VOID();
}
