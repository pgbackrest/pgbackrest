/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/macro.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Contains information about a memory allocation. This header is placed at the beginning of every memory allocation returned to the
user by memNew(), etc. The advantage is that when an allocation is passed back by the user we know the location of the allocation
header by doing some pointer arithmetic. This is much faster than searching through a list.
***********************************************************************************************************************************/
typedef struct MemContextAlloc
{
    unsigned int allocIdx : 32;                                     // Index in the allocation list
    unsigned int size : 32;                                         // Allocation size (4GB max)
} MemContextAlloc;

// Get the allocation buffer pointer given the allocation header pointer
#define MEM_CONTEXT_ALLOC_BUFFER(header)                            ((MemContextAlloc *)header + 1)

// Get the allocation header pointer given the allocation buffer pointer
#define MEM_CONTEXT_ALLOC_HEADER(buffer)                            ((MemContextAlloc *)buffer - 1)

// Make sure the allocation is valid for the current memory context. This check only works correctly if the allocation is valid and
// allocated as one of many but belongs to another context. Otherwise, there is likely to be a segfault.
#define ASSERT_ALLOC_MANY_VALID(alloc)                                                                                             \
    ASSERT(                                                                                                                        \
        alloc != NULL && (uintptr_t)alloc != (uintptr_t)-sizeof(MemContextAlloc) &&                                                \
        alloc->allocIdx < memContextAllocMany(memContextStack[memContextCurrentStackIdx].memContext)->listSize &&                  \
        memContextAllocMany(memContextStack[memContextCurrentStackIdx].memContext)->list[alloc->allocIdx]);

/***********************************************************************************************************************************
Contains information about the memory context
***********************************************************************************************************************************/
// Quantity of child contexts, allocations, or callbacks
typedef enum
{
    memQtyNone = 0,                                                 // None for this type
    memQtyOne = 1,                                                  // One for this type
    memQtyMany = 2,                                                 // Many for this type
} MemQty;

// Main structure required by every mem context
struct MemContext
{
#ifdef DEBUG
    const char *name;                                               // Indicates what the context is being used for
    uint64_t sequenceNew;                                           // Sequence when this context was created (used for audit)
    bool active : 1;                                                // Is the context currently active?
#endif
    MemQty childQty : 2;                                            // How many child contexts can this context have?
    bool childInitialized : 1;                                      // Has the child context list been initialized?
    MemQty allocQty : 2;                                            // How many allocations can this context have?
    bool allocInitialized : 1;                                      // Has the allocation list been initialized?
    MemQty callbackQty : 2;                                         // How many callbacks can this context have?
    bool callbackInitialized : 1;                                   // Has the callback been initialized?
    size_t allocExtra : 16;                                         // Size of extra allocation (1kB max)

    unsigned int contextParentIdx;                                  // Index in the parent context list
    MemContext *contextParent;                                      // All contexts have a parent except top
};

// Mem context with one allocation
typedef struct MemContextAllocOne
{
    MemContextAlloc *alloc;                                         // Memory allocation created in this context
} MemContextAllocOne;

// Mem context with many allocations
typedef struct MemContextAllocMany
{
    MemContextAlloc **list;                                         // List of memory allocations created in this context
    unsigned int listSize;                                          // Size of alloc list (not the actual count of allocations)
    unsigned int freeIdx;                                           // Index of first free space in the alloc list
} MemContextAllocMany;

// Mem context with one child context
typedef struct MemContextChildOne
{
    MemContext *context;                                            // Context created in this context
} MemContextChildOne;

// Mem context with many child contexts
typedef struct MemContextChildMany
{
    MemContext **list;                                              // List of contexts created in this context
    unsigned int listSize;                                          // Size of child context list (not the actual count of contexts)
    unsigned int freeIdx;                                           // Index of first free space in the context list
} MemContextChildMany;

// Mem context with one callback
typedef struct MemContextCallbackOne
{
    void (*function)(void *);                                       // Function to call before the context is freed
    void *argument;                                                 // Argument to pass to callback function
} MemContextCallbackOne;

/***********************************************************************************************************************************
Possible sizes for the manifest based on options
***********************************************************************************************************************************/
// {uncrustify_off - formatting compressed to save space}
static const uint8_t memContextSizePossible[memQtyMany + 1][memQtyMany + 1][memQtyOne + 1] =
{
    // child none
    {// alloc none
     {/* callback none */ 0, /* callback one */ sizeof(MemContextCallbackOne)},
     // alloc one
     {/* callback none */ sizeof(MemContextAllocOne),
      /* callback one */ sizeof(MemContextAllocOne) + sizeof(MemContextCallbackOne)},
     // alloc many
     {/* callback none */ sizeof(MemContextAllocMany),
      /* callback one */ sizeof(MemContextAllocMany) + sizeof(MemContextCallbackOne)}},
    // child one
    {// alloc none
     {/* callback none */ sizeof(MemContextChildOne),
      /* callback one */ sizeof(MemContextChildOne) + sizeof(MemContextCallbackOne)},
     // alloc one
     {/* callback none */ sizeof(MemContextChildOne) + sizeof(MemContextAllocOne),
      /* callback one */ sizeof(MemContextChildOne) + sizeof(MemContextAllocOne) + sizeof(MemContextCallbackOne)},
     // alloc many
     {/* callback none */ sizeof(MemContextChildOne) + sizeof(MemContextAllocMany),
      /* callback one */ sizeof(MemContextChildOne) + sizeof(MemContextAllocMany) + sizeof(MemContextCallbackOne)}},
    // child many
    {// alloc none
     {/* callback none */ sizeof(MemContextChildMany),
      /* callback one */ sizeof(MemContextChildMany) + sizeof(MemContextCallbackOne)},
     // alloc one
     {/* callback none */ sizeof(MemContextChildMany) + sizeof(MemContextAllocOne),
      /* callback one */ sizeof(MemContextChildMany) + sizeof(MemContextAllocOne) + sizeof(MemContextCallbackOne)},
     // alloc many
     {/* callback none */ sizeof(MemContextChildMany) + sizeof(MemContextAllocMany),
      /* callback one */ sizeof(MemContextChildMany) + sizeof(MemContextAllocMany) + sizeof(MemContextCallbackOne)}},
};
// {uncrustify_on}

/***********************************************************************************************************************************
Get pointers to optional parts of the manifest
***********************************************************************************************************************************/
// Get pointer to child part
#define MEM_CONTEXT_CHILD_OFFSET(memContext)                        ((unsigned char *)(memContext + 1) + memContext->allocExtra)

static MemContextChildOne *
memContextChildOne(MemContext *const memContext)
{
    return (MemContextChildOne *)MEM_CONTEXT_CHILD_OFFSET(memContext);
}

static MemContextChildMany *
memContextChildMany(MemContext *const memContext)
{
    return (MemContextChildMany *)MEM_CONTEXT_CHILD_OFFSET(memContext);
}

// Get pointer to allocation part
#define MEM_CONTEXT_ALLOC_OFFSET(memContext)                                                                                       \
    ((unsigned char *)(memContext + 1) + memContextSizePossible[memContext->childQty][0][0] + memContext->allocExtra)

static MemContextAllocOne *
memContextAllocOne(MemContext *const memContext)
{
    return (MemContextAllocOne *)MEM_CONTEXT_ALLOC_OFFSET(memContext);
}

static MemContextAllocMany *
memContextAllocMany(MemContext *const memContext)
{
    return (MemContextAllocMany *)MEM_CONTEXT_ALLOC_OFFSET(memContext);
}

// Get pointer to callback part
static MemContextCallbackOne *
memContextCallbackOne(MemContext *const memContext)
{
    return
        (MemContextCallbackOne *)
        ((unsigned char *)(memContext + 1) +
         memContextSizePossible[memContext->childQty][memContext->allocQty][0] + memContext->allocExtra);
}

/***********************************************************************************************************************************
Top context

The top context always exists and can never be freed. All other contexts are children of the top context. The top context is
generally used to allocate memory that exists for the life of the program.
***********************************************************************************************************************************/
static struct MemContextTop
{
    MemContext memContext;
    MemContextChildMany memContextChildMany;
    MemContextAllocMany memContextAllocMany;
} contextTop =
{
    .memContext =
    {
#ifdef DEBUG
        .name = "TOP",
        .active = true,
#endif
        .childQty = memQtyMany,
        .allocQty = memQtyMany,
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
***********************************************************************************************************************************/
#ifdef DEBUG

static uint64_t memContextSequence = 0;

FN_EXTERN void
memContextAuditBegin(MemContextAuditState *const state)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, state);
    FUNCTION_TEST_END();

    ASSERT(state != NULL);
    ASSERT(state->memContext != NULL);
    ASSERT(state->memContext == memContextTop() || state->memContext->sequenceNew != 0);

    if (state->memContext->childInitialized)
    {
        ASSERT(state->memContext->childQty != memQtyNone);

        if (state->memContext->childQty == memQtyOne)
        {
            MemContextChildOne *const memContextChild = memContextChildOne(state->memContext);

            if (memContextChild->context != NULL)
                state->sequenceContextNew = memContextChild->context->sequenceNew;
        }
        else
        {
            ASSERT(state->memContext->childQty == memQtyMany);
            MemContextChildMany *const memContextChild = memContextChildMany(state->memContext);

            for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
            {
                if (memContextChild->list[contextIdx] != NULL &&
                    memContextChild->list[contextIdx]->sequenceNew > state->sequenceContextNew)
                {
                    state->sequenceContextNew = memContextChild->list[contextIdx]->sequenceNew;
                }
            }
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
memContextAuditNameMatch(const char *const actual, const char *const expected)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, actual);
        FUNCTION_TEST_PARAM(STRINGZ, expected);
    FUNCTION_TEST_END();

    ASSERT(actual != NULL);
    ASSERT(expected != NULL);

    unsigned int actualIdx = 0;

    while (actual[actualIdx] != '\0' && actual[actualIdx] == expected[actualIdx])
        actualIdx++;

    FUNCTION_TEST_RETURN(
        BOOL,
        (actual[actualIdx] == '\0' || strncmp(actual + actualIdx, "::", 2) == 0) &&
        (expected[actualIdx] == '\0' || strcmp(expected + actualIdx, " *") == 0));
}

FN_EXTERN void
memContextAuditEnd(const MemContextAuditState *const state, const char *const returnType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, state);
        FUNCTION_TEST_PARAM(STRINGZ, returnType);
    FUNCTION_TEST_END();

    if (state->returnTypeAny)
        FUNCTION_TEST_RETURN_VOID();

    if (state->memContext->childInitialized)
    {
        ASSERT(state->memContext->childQty != memQtyNone);

        const char *returnTypeInvalid = NULL;
        const char *returnTypeFound = NULL;

        if (state->memContext->childQty == memQtyOne)
        {
            MemContextChildOne *const memContextChild = memContextChildOne(state->memContext);

            if (memContextChild->context != NULL && memContextChild->context->sequenceNew > state->sequenceContextNew &&
                !memContextAuditNameMatch(memContextChild->context->name, returnType))
            {
                returnTypeInvalid = memContextChild->context->name;
            }
        }
        else
        {
            ASSERT(state->memContext->childQty == memQtyMany);
            MemContextChildMany *const memContextChild = memContextChildMany(state->memContext);

            for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
            {
                if (memContextChild->list[contextIdx] != NULL &&
                    memContextChild->list[contextIdx]->sequenceNew > state->sequenceContextNew)
                {
                    if (memContextAuditNameMatch(memContextChild->list[contextIdx]->name, returnType))
                    {
                        if (returnTypeFound != NULL)
                        {
                            returnTypeInvalid = memContextChild->list[contextIdx]->name;
                            break;
                        }

                        returnTypeFound = memContextChild->list[contextIdx]->name;
                    }
                    else
                    {
                        returnTypeInvalid = memContextChild->list[contextIdx]->name;
                        break;
                    }
                }
            }
        }

        if (returnTypeInvalid != NULL)
        {
            if (returnTypeFound != NULL)
            {
                THROW_FMT(
                    AssertError, "expected return type '%s' already found but also found '%s'", returnTypeFound, returnTypeInvalid);
            }
            else
            {
                THROW_FMT(
                    AssertError, "expected return type '%s' but found '%s'", returnType, returnTypeInvalid);
            }
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void *
memContextAuditAllocExtraName(void *const allocExtra, const char *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, allocExtra);
        FUNCTION_TEST_PARAM(STRINGZ, name);
    FUNCTION_TEST_END();

    memContextFromAllocExtra(allocExtra)->name = name;

    FUNCTION_TEST_RETURN_P(VOID, allocExtra);
}

#endif

/***********************************************************************************************************************************
Wrapper around malloc() with error handling
***********************************************************************************************************************************/
static void *
memAllocInternal(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate memory
    void *const buffer = malloc(size);

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
memAllocPtrArrayInternal(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate memory
    void **const buffer = memAllocInternal(size * sizeof(void *));

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
memReAllocInternal(void *const bufferOld, const size_t sizeNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
    FUNCTION_TEST_END();

    ASSERT(bufferOld != NULL);

    // Allocate memory
    void *const bufferNew = realloc(bufferOld, sizeNew);

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
memReAllocPtrArrayInternal(void *const bufferOld, const size_t sizeOld, const size_t sizeNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
    FUNCTION_TEST_END();

    // Allocate memory
    void **const bufferNew = memReAllocInternal(bufferOld, sizeNew * sizeof(void *));

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
memFreeInternal(void *const buffer)
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
memContextNewIndex(MemContext *const memContext, MemContextChildMany *const memContextChild)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, memContext);
        FUNCTION_TEST_PARAM_P(VOID, memContextChild);
    FUNCTION_TEST_END();

    ASSERT(memContext != NULL);
    ASSERT(memContextChild != NULL);

    // Initialize (free space will always be index 0)
    if (!memContext->childInitialized)
    {
        *memContextChild = (MemContextChildMany)
        {
            .list = memAllocPtrArrayInternal(MEM_CONTEXT_INITIAL_SIZE),
            .listSize = MEM_CONTEXT_INITIAL_SIZE,
        };

        memContext->childInitialized = true;
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
FN_EXTERN MemContext *
memContextNew(
#ifdef DEBUG
    const char *const name,
#endif
    const MemContextNewParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
        FUNCTION_TEST_PARAM(UINT, param.childQty);
        FUNCTION_TEST_PARAM(UINT, param.allocQty);
        FUNCTION_TEST_PARAM(UINT, param.callbackQty);
        FUNCTION_TEST_PARAM(SIZE, param.allocExtra);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);
    ASSERT(param.childQty <= 1 || param.childQty == UINT8_MAX);
    ASSERT(param.allocQty <= 1 || param.allocQty == UINT8_MAX);
    ASSERT(param.callbackQty <= 1);
    // Check context name length
    ASSERT(name[0] != '\0');

    // Pad allocExtra so that optional structures will be aligned. There may not be any optional structures but it doesn't seem
    // worth the cost of checking since memory allocations are aligned so the extra bytes would be wasted anyway.
    size_t allocExtra = param.allocExtra;

    if (allocExtra % ALIGN_OF(void *) != 0)
        allocExtra += ALIGN_OFFSET(void *, allocExtra);

    // Create the new context
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->childQty != memQtyNone);

    const MemQty childQty = param.childQty > 1 ? memQtyMany : (MemQty)param.childQty;
    const MemQty allocQty = param.allocQty > 1 ? memQtyMany : (MemQty)param.allocQty;
    const MemQty callbackQty = (MemQty)param.callbackQty;

    MemContext *const this = memAllocInternal(
        sizeof(MemContext) + allocExtra + memContextSizePossible[childQty][allocQty][callbackQty]);

    *this = (MemContext)
    {
#ifdef DEBUG
        // Set the context name
        .name = name,

        // Set audit sequence
        .sequenceNew = ++memContextSequence,

        // Set new context active
        .active = true,
#endif
        // Set flags
        .childQty = childQty,
        .allocQty = allocQty,
        .callbackQty = callbackQty,

        // Set extra allocation
        .allocExtra = (uint16_t)allocExtra,

        // Set current context as the parent
        .contextParent = contextCurrent,
    };

    // Find space for the new context
    if (contextCurrent->childQty == memQtyOne)
    {
        ASSERT(!contextCurrent->childInitialized || memContextChildOne(contextCurrent)->context == NULL);

        memContextChildOne(contextCurrent)->context = this;
        contextCurrent->childInitialized = true;
    }
    else
    {
        ASSERT(contextCurrent->childQty == memQtyMany);
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
FN_EXTERN void *
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
FN_EXTERN MemContext *
memContextFromAllocExtra(void *const allocExtra)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, allocExtra);
    FUNCTION_TEST_END();

    ASSERT(allocExtra != NULL);
    ASSERT(((MemContext *)allocExtra - 1)->allocExtra != 0);

    FUNCTION_TEST_RETURN(MEM_CONTEXT, (MemContext *)allocExtra - 1);
}

/**********************************************************************************************************************************/
FN_EXTERN void
memContextCallbackSet(MemContext *const this, void (*const callbackFunction)(void *), void *const callbackArgument)
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
    ASSERT(this->callbackQty != memQtyNone);

#ifdef DEBUG
    // Error if callback has already been set - there may be valid use cases for this in the future but error until one is found
    if (this->callbackInitialized)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);
#endif

    // Set callback function and argument
    *(memContextCallbackOne(this)) = (MemContextCallbackOne){.function = callbackFunction, .argument = callbackArgument};
    this->callbackInitialized = true;

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
memContextCallbackClear(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->callbackQty != memQtyNone);
    ASSERT(this->active);

    // Clear callback function and argument
    *(memContextCallbackOne(this)) = (MemContextCallbackOne){0};
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
    ASSERT(contextCurrent->allocQty != memQtyNone);

    if (contextCurrent->allocQty == memQtyOne)
    {
        MemContextAllocOne *const contextAlloc = memContextAllocOne(contextCurrent);
        ASSERT(!contextCurrent->allocInitialized || contextAlloc->alloc == NULL);

        // Initialize allocation header
        *result = (MemContextAlloc){.size = (unsigned int)(sizeof(MemContextAlloc) + size)};

        // Set pointer in allocation
        contextAlloc->alloc = result;
        contextCurrent->allocInitialized = true;
    }
    else
    {
        ASSERT(contextCurrent->allocQty == memQtyMany);

        MemContextAllocMany *const contextAlloc = memContextAllocMany(contextCurrent);

        // Initialize (free space will always be index 0)
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
                const unsigned int listSizeNew = contextAlloc->listSize * 2;

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
memContextAllocResize(MemContextAlloc *alloc, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, alloc);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Resize the allocation
    alloc = memReAllocInternal(alloc, sizeof(MemContextAlloc) + size);
    alloc->size = (unsigned int)(sizeof(MemContextAlloc) + size);

    // Update pointer in allocation list in case the realloc moved the allocation
    MemContext *const currentContext = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(currentContext->allocQty != memQtyNone);
    ASSERT(currentContext->allocInitialized);

    if (currentContext->allocQty == memQtyOne)
    {
        ASSERT(memContextAllocOne(currentContext)->alloc != NULL);
        memContextAllocOne(currentContext)->alloc = alloc;
    }
    else
    {
        ASSERT(currentContext->allocQty == memQtyMany);
        ASSERT_ALLOC_MANY_VALID(alloc);

        memContextAllocMany(currentContext)->list[alloc->allocIdx] = alloc;
    }

    FUNCTION_TEST_RETURN_TYPE_P(MemContextAlloc, alloc);
}

/**********************************************************************************************************************************/
FN_EXTERN void *
memNew(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_P(VOID, MEM_CONTEXT_ALLOC_BUFFER(memContextAllocNew(size)));
}

/**********************************************************************************************************************************/
FN_EXTERN void *
memNewPtrArray(const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Allocate pointer array
    void **const buffer = (void **const)MEM_CONTEXT_ALLOC_BUFFER(memContextAllocNew(size * sizeof(void *)));

    // Set pointers to NULL
    for (size_t ptrIdx = 0; ptrIdx < size; ptrIdx++)
        buffer[ptrIdx] = NULL;

    FUNCTION_TEST_RETURN_P(VOID, buffer);
}

/**********************************************************************************************************************************/
FN_EXTERN void *
memResize(void *const buffer, const size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_P(VOID, MEM_CONTEXT_ALLOC_BUFFER(memContextAllocResize(MEM_CONTEXT_ALLOC_HEADER(buffer), size)));
}

/**********************************************************************************************************************************/
FN_EXTERN void
memFree(void *const buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    // Get the allocation
    MemContext *const contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    ASSERT(contextCurrent->allocQty != memQtyNone);
    ASSERT(contextCurrent->allocInitialized);
    MemContextAlloc *const alloc = MEM_CONTEXT_ALLOC_HEADER(buffer);

    // Remove allocation from the context
    if (contextCurrent->allocQty == memQtyOne)
    {
        ASSERT(memContextAllocOne(contextCurrent)->alloc == alloc);
        memContextAllocOne(contextCurrent)->alloc = NULL;
    }
    else
    {
        ASSERT(contextCurrent->allocQty == memQtyMany);
        ASSERT_ALLOC_MANY_VALID(alloc);

        // If this allocation is before the current free allocation then make it the current free allocation
        MemContextAllocMany *const contextAlloc = memContextAllocMany(contextCurrent);

        if (alloc->allocIdx < contextAlloc->freeIdx)
            contextAlloc->freeIdx = alloc->allocIdx;

        // Null the allocation
        contextAlloc->list[alloc->allocIdx] = NULL;
    }

    // Free the allocation
    memFreeInternal(alloc);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
memContextMove(MemContext *const this, MemContext *const parentNew)
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
        ASSERT(this->contextParent->childQty != memQtyNone);
        ASSERT(this->contextParent->childInitialized);

        // Null out the context in the old parent
        if (this->contextParent->childQty == memQtyOne)
        {
            ASSERT(memContextChildOne(this->contextParent)->context != NULL);
            memContextChildOne(this->contextParent)->context = NULL;
        }
        else
        {
            ASSERT(this->contextParent->childQty == memQtyMany);
            ASSERT(memContextChildMany(this->contextParent)->list[this->contextParentIdx] == this);

            memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;
        }

        // Find a place in the new parent context and assign it
        ASSERT(parentNew->active);
        ASSERT(parentNew->childQty != memQtyNone);

        if (parentNew->childQty == memQtyOne)
        {
            ASSERT(!parentNew->childInitialized || memContextChildOne(parentNew)->context == NULL);

            memContextChildOne(parentNew)->context = this;
            parentNew->childInitialized = true;
        }
        else
        {
            ASSERT(parentNew->childQty == memQtyMany);
            MemContextChildMany *const memContextChild = memContextChildMany(parentNew);

            // The child list may move while finding a new index so store the index and use it with (what might be) the new pointer
            this->contextParentIdx = memContextNewIndex(parentNew, memContextChild);
            memContextChild->list[this->contextParentIdx] = this;
        }

        // Set new parent
        this->contextParent = parentNew;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
memContextSwitch(MemContext *const this)
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
FN_EXTERN void
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
FN_EXTERN void
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
FN_EXTERN void
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
FN_EXTERN MemContext *
memContextTop(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(MEM_CONTEXT, (MemContext *)&contextTop);
}

/**********************************************************************************************************************************/
FN_EXTERN MemContext *
memContextCurrent(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(MEM_CONTEXT, memContextStack[memContextCurrentStackIdx].memContext);
}

/**********************************************************************************************************************************/
FN_EXTERN MemContext *
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
#ifdef DEBUG

FN_EXTERN size_t
memContextSize(const MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->active);

    // Size of struct and extra
    size_t total = 0;
    const unsigned char *offset = (const unsigned char *)(this + 1) + this->allocExtra;

    // Size of child contexts
    if (this->childQty == memQtyOne)
    {
        if (this->childInitialized)
        {
            const MemContextChildOne *const contextChild = (const MemContextChildOne *const)offset;

            if (contextChild->context != NULL)
                total += memContextSize(contextChild->context);
        }

        offset += sizeof(MemContextChildOne);
    }
    else if (this->childQty == memQtyMany)
    {
        if (this->childInitialized)
        {
            const MemContextChildMany *const contextChild = (const MemContextChildMany *const)offset;

            for (unsigned int contextIdx = 0; contextIdx < contextChild->listSize; contextIdx++)
            {
                if (contextChild->list[contextIdx] != NULL)
                    total += memContextSize(contextChild->list[contextIdx]);
            }

            total += contextChild->listSize * sizeof(MemContextChildMany *);
        }

        offset += sizeof(MemContextChildMany);
    }

    // Size of allocations
    if (this->allocQty == memQtyOne)
    {
        if (this->allocInitialized)
        {
            const MemContextAllocOne *const contextAlloc = (const MemContextAllocOne *const)offset;

            if (contextAlloc->alloc != NULL)
                total += contextAlloc->alloc->size;
        }

        offset += sizeof(MemContextAllocOne);
    }
    else if (this->allocQty == memQtyMany)
    {
        if (this->allocInitialized)
        {
            const MemContextAllocMany *const contextAlloc = (const MemContextAllocMany *const)offset;

            for (unsigned int allocIdx = 0; allocIdx < contextAlloc->listSize; allocIdx++)
            {
                if (contextAlloc->list[allocIdx] != NULL)
                    total += contextAlloc->list[allocIdx]->size;
            }

            total += contextAlloc->listSize * sizeof(MemContextAllocMany *);
        }

        offset += sizeof(MemContextAllocMany);
    }

    // Size of callback
    if (this->callbackQty != memQtyNone)
        offset += sizeof(MemContextCallbackOne);

    FUNCTION_TEST_RETURN(SIZE, (size_t)(offset - (const unsigned char *)this) + total);
}

#endif // DEBUG

/**********************************************************************************************************************************/
FN_EXTERN void
memContextClean(const unsigned int tryDepth, const bool fatal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, tryDepth);
        FUNCTION_TEST_PARAM(BOOL, false);
    FUNCTION_TEST_END();

    ASSERT(tryDepth > 0);

    // Iterate through everything pushed to the stack since the last try
    while (memContextStack[memContextMaxStackIdx].tryDepth >= tryDepth)
    {
        // Free memory contexts that were not kept. Skip this for fatal errors to avoid calling destructors that could error and
        // mask the original error.
        if (memContextStack[memContextMaxStackIdx].type == memContextStackTypeNew)
        {
            if (!fatal)
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
// Helper to execute callbacks for the context and all its children
static void
memContextCallbackRecurse(MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

#ifdef DEBUG
    // Certain actions against the context are no longer allowed
    this->active = false;
#endif

    // Callback
    if (this->callbackInitialized)
    {
        MemContextCallbackOne *const callback = memContextCallbackOne(this);
        callback->function(callback->argument);
        this->callbackInitialized = false;
    }

    // Child callbacks
    if (this->childInitialized)
    {
        if (this->childQty == memQtyOne)
        {
            MemContextChildOne *const memContextChild = memContextChildOne(this);

            if (memContextChild->context != NULL)
                memContextCallbackRecurse(memContextChild->context);
        }
        else
        {
            ASSERT(this->childQty == memQtyMany);
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
    if (this == memContextStack[memContextCurrentStackIdx].memContext && this != memContextTop())
        THROW_FMT(AssertError, "cannot free current context '%s'", this->name);
#endif

    // Free child contexts
    if (this->childInitialized)
    {
        if (this->childQty == memQtyOne)
        {
            MemContextChildOne *const memContextChild = memContextChildOne(this);

            if (memContextChild->context != NULL)
                memContextFreeRecurse(memContextChild->context);
        }
        else
        {
            ASSERT(this->childQty == memQtyMany);
            MemContextChildMany *const memContextChild = memContextChildMany(this);

            for (unsigned int contextIdx = 0; contextIdx < memContextChild->listSize; contextIdx++)
            {
                if (memContextChild->list[contextIdx] != NULL)
                    memContextFreeRecurse(memContextChild->list[contextIdx]);
            }

            // Free child context allocation list
            memFreeInternal(memContextChildMany(this)->list);
        }
    }

    // Free memory allocations and list
    if (this->allocInitialized)
    {
        ASSERT(this->allocQty != memQtyNone);

        if (this->allocQty == memQtyOne)
        {
            MemContextAllocOne *const contextAlloc = memContextAllocOne(this);

            if (contextAlloc->alloc != NULL)
                memFreeInternal(contextAlloc->alloc);
        }
        else
        {
            ASSERT(this->allocQty == memQtyMany);

            MemContextAllocMany *const contextAlloc = memContextAllocMany(this);

            for (unsigned int allocIdx = 0; allocIdx < contextAlloc->listSize; allocIdx++)
                if (contextAlloc->list[allocIdx] != NULL)
                    memFreeInternal(contextAlloc->list[allocIdx]);

            memFreeInternal(contextAlloc->list);
        }
    }

    // Free the memory context so the slot can be reused (if not the top mem context)
    if (this != memContextTop())
    {
        ASSERT(this->contextParent != NULL);

        if (this->contextParent->childQty == memQtyOne)
            memContextChildOne(this->contextParent)->context = NULL;
        else
        {
            ASSERT(this->contextParent->childQty == memQtyMany);

            MemContextChildMany *const memContextChild = memContextChildMany(this->contextParent);

            // If the context index is lower than the current free index in the parent then replace it
            if (this->contextParentIdx < memContextChild->freeIdx)
                memContextChild->freeIdx = this->contextParentIdx;

            memContextChildMany(this->contextParent)->list[this->contextParentIdx] = NULL;
        }

        memFreeInternal(this);
    }
    // Else reset top context. In practice it is uncommon for the top mem context to be freed and then used again.
    else
    {
        this->childInitialized = false;
        this->allocInitialized = false;

#ifdef DEBUG
        // Make the top mem context active again
        this->active = true;
#endif
    }

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
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
    // Free context even if a callback fails
    FINALLY()
    {
        memContextFreeRecurse(this);
    }
    TRY_END();

    FUNCTION_TEST_RETURN_VOID();
}
