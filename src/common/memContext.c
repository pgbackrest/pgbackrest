/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include "build.auto.h"

//#include <stdio.h> // !!! REMOVE
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Memory context states
***********************************************************************************************************************************/
typedef enum
{
    memContextStateFreeing = 0,
    memContextStateActive
} MemContextState;

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
Contains information about the memory context
***********************************************************************************************************************************/
struct MemContext
{
    const char *name;                                               // Indicates what the context is being used for
    MemContextState state:1;                                        // Current state of the context
    MemContextAllocType allocType:2;                                // How many allocations can this context have?
    bool allocInitialized:1;                                        // Has the allocation list been initialized?
    MemContextChildType childType:2;                                // How many child contexts can this context have?
    bool childInitialized:1;                                        // Has the child contest list been initialized?
    bool callback:1;                                                // Is a callback allowed?
    bool callbackInitialized:1;                                     // Has the callback been initialized?
    size_t allocExtra:16;                                           // Size of extra allocation (1kB max)

    unsigned int contextParentIdx;                                  // Index in the parent context list
    MemContext *contextParent;                                      // All contexts have a parent except top
};

typedef struct MemContextAllocMany
{
    MemContextAlloc **list;                                         // List of memory allocations created in this context
    unsigned int listSize;                                          // Size of alloc list (not the actual count of allocations)
    unsigned int freeIdx;                                           // Index of first free space in the alloc list
} MemContextAllocMany;

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

static size_t
memContextAllocSize(const MemContextAllocType type)
{
    switch (type)
    {
        case memContextAllocTypeNone: // {uncovered}
            return 0; // {uncovered}

        case memContextAllocTypeOne: // {uncovered}
            return sizeof(MemContextAlloc); // {uncovered}

        default:
            ASSERT(type == memContextAllocTypeMany);
            return sizeof(MemContextAllocMany);
    }
}

static size_t
memContextChildSize(const MemContextChildType type)
{
    switch (type)
    {
        case memContextAllocTypeNone: // {uncovered}
            return 0; // {uncovered}

        case memContextAllocTypeOne: // {uncovered}
            return sizeof(MemContext); // {uncovered}

        default:
            ASSERT(type == memContextChildTypeMany);
            return sizeof(MemContextChildMany);
    }
}

static size_t
memContextCallbackSize(bool callback)
{
    if (callback) // {uncovered}
        return sizeof(MemContextCallback);

    return 0; // {uncovered}
}

static MemContextAllocMany *
memContextAllocMany(MemContext *const memContext)
{
    return (MemContextAllocMany *)((unsigned char *)(memContext + 1) + memContext->allocExtra);
}

static MemContextChildMany *
memContextChildMany(MemContext *const memContext)
{
    switch (memContext->allocType)
    {
        case memContextAllocTypeNone: // {uncovered}
            return (MemContextChildMany *)memContextAllocMany(memContext); // {uncovered}

        case memContextAllocTypeOne: // {uncovered}
            return (MemContextChildMany *)((MemContextAlloc *)memContextAllocMany(memContext) + 1); // {uncovered}

        default:
            ASSERT(memContext->allocType == memContextAllocTypeMany);
            return (MemContextChildMany *)((MemContextAllocMany *)memContextAllocMany(memContext) + 1);
    }
}

static MemContextCallback *
memContextCallback(MemContext *const memContext)
{
    switch (memContext->childType)
    {
        case memContextChildTypeNone: // {uncovered}
            return (MemContextCallback *)memContextChildMany(memContext); // {uncovered}

        case memContextAllocTypeOne: // {uncovered}
            return (MemContextCallback *)((MemContext *)memContextChildMany(memContext) + 1); // {uncovered}

        default:
            ASSERT(memContext->childType == memContextChildTypeMany);
            return (MemContextCallback *)((MemContextChildMany *)memContextChildMany(memContext) + 1);
    }
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
        .name = "TOP",
        .state = memContextStateActive,
        .allocType = memContextAllocTypeMany,
        .childType = memContextChildTypeMany,
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
memContextNew(const char *const name, MemContextNewParam param /* !!! MAKE THIS CONST AGAIN */)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
        FUNCTION_TEST_PARAM(ENUM, param.allocType);
        FUNCTION_TEST_PARAM(ENUM, param.childType);
        FUNCTION_TEST_PARAM(BOOL, param.callback);
        FUNCTION_TEST_PARAM(SIZE, param.allocExtra);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);
    ASSERT(param.allocType <= memContextAllocTypeMany);
    ASSERT(param.childType <= memContextChildTypeMany);
    // Check context name length
    ASSERT(name[0] != '\0');

    // !!! FUDGE THIS FOR NOW
    param.callback = true;
    param.allocType = memContextAllocTypeMany;
    param.childType = memContextChildTypeMany;

    // Find space for the new context
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->childType != memContextChildTypeNone);

    MemContext *const this = memAllocInternal(
        sizeof(MemContext) +
        (param.allocExtra % sizeof(void *) == 0 ? param.allocExtra : param.allocExtra / sizeof(void *) + sizeof(void *)) +
        memContextAllocSize(param.allocType) + memContextChildSize(param.childType) + memContextCallbackSize(param.callback));
    unsigned int contextIdx;

    if (contextCurrent->childType == memContextChildTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->childType == memContextChildTypeMany);
        MemContextChildMany *const memContextChild = memContextChildMany(contextCurrent);

        contextIdx = memContextNewIndex(contextCurrent, memContextChild);
        memContextChild->list[contextIdx] = this;

        // Possible free context must be in the next position
        memContextChild->freeIdx++;
    }

    *this = (MemContext)
    {
        // Set the context name
        .name = name,

        // Set flags
        .allocType = param.allocType,
        .childType = param.childType,
        .callback = param.callback,

        // Set extra allocation
        .allocExtra = param.allocExtra,

        // Set new context active
        .state = memContextStateActive,

        // Set current context as the parent
        .contextParent = contextCurrent,
        .contextParentIdx = contextIdx,
    };

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
    ASSERT(callbackFunction != NULL);
    ASSERT(this->callback);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot assign callback to inactive context");

    // Error if callback has already been set - there may be valid use cases for this in the future but error until one is found
    if (this->callbackInitialized)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);

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
    ASSERT(this->callback);

    // Error if context is not active or freeing
    ASSERT(this->state == memContextStateActive || this->state == memContextStateFreeing);

    // Top context cannot have a callback
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

    MemContextAlloc *result;

    // Find space for the new allocation
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->allocType != memContextAllocTypeNone);

    // fprintf(stdout, "!!!HERE\n"); fflush(stdout);

    if (contextCurrent->allocType == memContextAllocTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->allocType == memContextAllocTypeMany);

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

        // Create new allocation
        result = memAllocInternal(sizeof(MemContextAlloc) + size);

        *result = (MemContextAlloc)
        {
            .allocIdx = contextAlloc->freeIdx,
            .size = (unsigned int)(sizeof(MemContextAlloc) + size),
        };

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
    ASSERT(currentContext->allocType != memContextAllocTypeNone);

    if (currentContext->allocType == memContextAllocTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
    }
    else
    {
        ASSERT(currentContext->allocType == memContextAllocTypeMany);
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
memFree(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    // ASSERT_ALLOC_VALID(MEM_CONTEXT_ALLOC_HEADER(buffer)); // FIX THIS?

    // Get the allocation
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->allocType != memContextAllocTypeNone);
    MemContextAlloc *const alloc = MEM_CONTEXT_ALLOC_HEADER(buffer);

    if (contextCurrent->allocType == memContextAllocTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
    }
    else
    {
        ASSERT(contextCurrent->allocType == memContextAllocTypeMany);
        MemContextAllocMany *const contextAlloc = memContextAllocMany(contextCurrent);

        // If this allocation is before the current free allocation then make it the current free allocation
        if (alloc->allocIdx < contextAlloc->freeIdx)
            contextAlloc->freeIdx = alloc->allocIdx;

        // Free the allocation
        contextAlloc->list[alloc->allocIdx] = NULL;
        memFreeInternal(alloc);
    }

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
        ASSERT(this->contextParent->childType != memContextChildTypeNone);

        // Null out the context in the old parent
        if (this->contextParent->childType == memContextChildTypeOne) // {uncovered}
        {
            THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
        }
        else
        {
            ASSERT(this->contextParent->childType == memContextChildTypeMany);
            ASSERT(memContextChildMany(this->contextParent)->list[this->contextParentIdx] == this);

            memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;
        }

        // Find a place in the new parent context and assign it. The child list may be moved while finding a new index so store the
        // index and use it with (what might be) the new pointer.
        ASSERT(parentNew->childType != memContextChildTypeNone);

        if (parentNew->childType == memContextChildTypeOne) // {uncovered}
        {
            THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
        }
        else
        {
            ASSERT(parentNew->childType == memContextChildTypeMany);
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
    ASSERT(memContextCurrentStackIdx < MEM_CONTEXT_STACK_MAX - 1);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot switch to inactive context");

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
            memContextName(memContextStack[memContextMaxStackIdx].memContext));
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
            memContextName(memContextStack[memContextMaxStackIdx].memContext));
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
            memContextName(memContextStack[memContextMaxStackIdx].memContext));
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
bool
memContextFreeing(const MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->state == memContextStateFreeing);
}

/**********************************************************************************************************************************/
const char *
memContextName(const MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot get name for inactive context");

    FUNCTION_TEST_RETURN_CONST(STRINGZ, this->name);
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

    // Size of struct and extra
    size_t total = 0;
    const unsigned char *offset = (unsigned char *)(this + 1) + this->allocExtra;

    // Size of allocations
    if (this->allocType == memContextAllocTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
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
    if (this->childType == memContextChildTypeOne) // {uncovered}
    {
        THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
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
    if (this->callback)
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

/**********************************************************************************************************************************/
void
memContextFree(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If context is already freeing then return if memContextFree() is called again - this can happen in callbacks
    if (this->state != memContextStateFreeing)
    {
        // Current context cannot be freed unless it is top (top is never really freed, just the stuff under it)
        if (this == memContextStack[memContextCurrentStackIdx].memContext && this != (MemContext *)&contextTop)
            THROW_FMT(AssertError, "cannot free current context '%s'", this->name);

        // Free child contexts
        if (this->childInitialized)
        {
            if (this->childType == memContextChildTypeOne) // {uncovered}
            {
                THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
            }
            else
            {
                ASSERT(this->childType == memContextChildTypeMany);
                MemContextChildMany *const memContextChild = memContextChildMany(this);

                for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
                {
                    if (memContextChild->list[contextIdx] != NULL)
                        memContextFree(memContextChild->list[contextIdx]);
                }
            }
        }

        // Set state to freeing now that there are no child contexts. Child contexts might need to interact with their parent while
        // freeing so the parent needs to remain active until they are all gone.
        this->state = memContextStateFreeing;

        // Execute callback if defined
        TRY_BEGIN()
        {
            if (this->callbackInitialized)
            {
                MemContextCallback *const callback = memContextCallback(this);
                callback->function(callback->argument);
            }
        }
        // Finish cleanup even if the callback fails
        FINALLY()
        {
            // Free child context allocation list
            if (this->childInitialized)
            {
                if (this->childType == memContextChildTypeMany) // {uncovered}
                    memFreeInternal(memContextChildMany(this)->list);

                this->childInitialized = false;
            }

            // Free memory allocations and list
            if (this->allocInitialized)
            {
                ASSERT(this->allocType != memContextAllocTypeOne);

                if (this->allocType == memContextAllocTypeOne) // {uncovered}
                {
                    THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
                }
                else
                {
                    ASSERT(this->allocType == memContextAllocTypeMany);

                    MemContextAllocMany *const contextAlloc = memContextAllocMany(this);

                    for (unsigned int allocIdx = 0; allocIdx < contextAlloc->listSize; allocIdx++)
                        if (contextAlloc->list[allocIdx] != NULL)
                            memFreeInternal(contextAlloc->list[allocIdx]);

                    memFreeInternal(contextAlloc->list);
                }

                this->allocInitialized = false;
            }

            // If the context index is lower than the current free index in the parent then replace it
            if (this->contextParent != NULL)
            {
                if (this->contextParent->childType == memContextChildTypeOne) // {uncovered}
                {
                    THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
                }
                else
                {
                    MemContextChildMany *const memContextChild = memContextChildMany(this->contextParent);

                    if (this->contextParentIdx < memContextChild->freeIdx)
                        memContextChild->freeIdx = this->contextParentIdx;
                }
            }

            // Make top context active again
            if (this == (MemContext *)&contextTop)
            {
                this->state = memContextStateActive;
            }
            // Else free the memory context so the slot can be reused
            else
            {
                ASSERT(this->contextParent != NULL);

                if (this->contextParent->childType == memContextChildTypeOne) // {uncovered}
                {
                    THROW(AssertError, "!!!NOT YET IMPLEMENTED"); // {uncovered}
                }
                else
                {
                    memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;
                    memFreeInternal(this);
                }
            }
        }
        TRY_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
