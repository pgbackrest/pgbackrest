/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include "build.auto.h"

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
    memContextStateFree = 0,
    memContextStateFreeing,
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
    MemContextState state;                                          // Current state of the context

    unsigned int contextParentIdx;                                  // Index in the parent context list
    MemContext *contextParent;                                      // All contexts have a parent except top

    MemContext **contextChildList;                                  // List of contexts created in this context
    unsigned int contextChildListSize;                              // Size of child context list (not the actual count of contexts)
    unsigned int contextChildFreeIdx;                               // Index of first free space in the context list

    MemContextAlloc **allocList;                                    // List of memory allocations created in this context
    unsigned int allocListSize;                                     // Size of alloc list (not the actual count of allocations)
    unsigned int allocFreeIdx;                                      // Index of first free space in the alloc list

    void (*callbackFunction)(void *);                               // Function to call before the context is freed
    void *callbackArgument;                                         // Argument to pass to callback function
};

/***********************************************************************************************************************************
Top context

The top context always exists and can never be freed.  All other contexts are children of the top context. The top context is
generally used to allocate memory that exists for the life of the program.
***********************************************************************************************************************************/
static MemContext contextTop = {.state = memContextStateActive, .name = "TOP"};

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
} memContextStack[MEM_CONTEXT_STACK_MAX] = {{.memContext = &contextTop}};

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
    FUNCTION_TEST_RETURN(buffer);
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
    FUNCTION_TEST_RETURN(buffer);
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
    FUNCTION_TEST_RETURN(bufferNew);
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
    FUNCTION_TEST_RETURN(bufferNew);
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
memContextNewIndex(MemContext *memContext, bool allowFree)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, memContext);
        FUNCTION_TEST_PARAM(BOOL, allowFree);
    FUNCTION_TEST_END();

    ASSERT(memContext != NULL);

    // Try to find space for the new context
    for (; memContext->contextChildFreeIdx < memContext->contextChildListSize; memContext->contextChildFreeIdx++)
    {
        if (memContext->contextChildList[memContext->contextChildFreeIdx] == NULL ||
            (allowFree && memContext->contextChildList[memContext->contextChildFreeIdx]->state == memContextStateFree))
        {
            break;
        }
    }

    // If no space was found then allocate more
    if (memContext->contextChildFreeIdx == memContext->contextChildListSize)
    {
        // If no space has been allocated to the list
        if (memContext->contextChildListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memAllocPtrArrayInternal(MEM_CONTEXT_INITIAL_SIZE);

            // Set new list size
            memContext->contextChildListSize = MEM_CONTEXT_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int contextChildListSizeNew = memContext->contextChildListSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memReAllocPtrArrayInternal(
                memContext->contextChildList, memContext->contextChildListSize, contextChildListSizeNew);

            // Set new list size
            memContext->contextChildListSize = contextChildListSizeNew;
        }
    }

    FUNCTION_TEST_RETURN(memContext->contextChildFreeIdx);
}

/**********************************************************************************************************************************/
MemContext *
memContextNew(const char *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);

    // Check context name length
    ASSERT(name[0] != '\0');

    // Find space for the new context
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    unsigned int contextIdx = memContextNewIndex(contextCurrent, true);

    // If the context has not been allocated yet
    if (contextCurrent->contextChildList[contextIdx] == NULL)
        contextCurrent->contextChildList[contextIdx] = memAllocInternal(sizeof(MemContext));

    // Get the context
    MemContext *this = contextCurrent->contextChildList[contextIdx];

    *this = (MemContext)
    {
        // Create initial space for allocations
        .allocList = memAllocPtrArrayInternal(MEM_CONTEXT_ALLOC_INITIAL_SIZE),
        .allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE,

        // Set the context name
        .name = name,

        // Set new context active
        .state = memContextStateActive,

        // Set current context as the parent
        .contextParent = contextCurrent,
        .contextParentIdx = contextIdx,
    };

    // Possible free context must be in the next position
    contextCurrent->contextChildFreeIdx++;

    // Add to the mem context stack so it will be automatically freed on error if memContextKeep() has not been called
    memContextMaxStackIdx++;

    memContextStack[memContextMaxStackIdx] = (struct MemContextStack)
    {
        .memContext = this,
        .type = memContextStackTypeNew,
        .tryDepth = errorTryDepth(),
    };

    // Return context
    FUNCTION_TEST_RETURN(this);
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

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot assign callback to inactive context");

    // Top context cannot have a callback
    if (this == &contextTop)
        THROW(AssertError, "top context may not have a callback");

    // Error if callback has already been set - there may be valid use cases for this but error until one is found
    if (this->callbackFunction)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);

    // Set callback function and argument
    this->callbackFunction = callbackFunction;
    this->callbackArgument = callbackArgument;

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

    // Error if context is not active or freeing
    ASSERT(this->state == memContextStateActive || this->state == memContextStateFreeing);

    // Top context cannot have a callback
    ASSERT(this != &contextTop);

    // Clear callback function and argument
    this->callbackFunction = NULL;
    this->callbackArgument = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Find an available slot in the memory context's allocation list and allocate memory
***********************************************************************************************************************************/
static MemContextAlloc *
memContextAllocNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    // Find space for the new allocation
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;

    for (; contextCurrent->allocFreeIdx < contextCurrent->allocListSize; contextCurrent->allocFreeIdx++)
        if (contextCurrent->allocList[contextCurrent->allocFreeIdx] == NULL)
            break;

    // If no space was found then allocate more
    if (contextCurrent->allocFreeIdx == contextCurrent->allocListSize)
    {
        // Only the top context will not have initial space for allocations
        if (contextCurrent->allocListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            contextCurrent->allocList = memAllocPtrArrayInternal(MEM_CONTEXT_ALLOC_INITIAL_SIZE);

            // Set new size
            contextCurrent->allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int allocListSizeNew = contextCurrent->allocListSize * 2;

            // Reallocate memory before modifying anything else in case there is an error
            contextCurrent->allocList = memReAllocPtrArrayInternal(
                contextCurrent->allocList, contextCurrent->allocListSize, allocListSizeNew);

            // Set new size
            contextCurrent->allocListSize = allocListSizeNew;
        }
    }

    // Create new allocation
    MemContextAlloc *result = memAllocInternal(sizeof(MemContextAlloc) + size);

    *result = (MemContextAlloc)
    {
        .allocIdx = contextCurrent->allocFreeIdx,
        .size = (unsigned int)(sizeof(MemContextAlloc) + size),
    };

    // Set pointer in allocation list
    contextCurrent->allocList[contextCurrent->allocFreeIdx] = result;

    // Update free index to next location. This location may not actually be free but it is where the search should start next time.
    contextCurrent->allocFreeIdx++;

    FUNCTION_TEST_RETURN(result);
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

    ASSERT_ALLOC_VALID(alloc);

    // Resize the allocation
    alloc = memReAllocInternal(alloc, sizeof(MemContextAlloc) + size);
    alloc->size = (unsigned int)(sizeof(MemContextAlloc) + size);

    // Update pointer in allocation list in case the realloc moved the allocation
    memContextStack[memContextCurrentStackIdx].memContext->allocList[alloc->allocIdx] = alloc;

    FUNCTION_TEST_RETURN(alloc);
}

/**********************************************************************************************************************************/
void *
memNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocNew(size)));
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

    FUNCTION_TEST_RETURN(buffer);
}

/**********************************************************************************************************************************/
void *
memResize(const void *buffer, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(MEM_CONTEXT_ALLOC_BUFFER(memContextAllocResize(MEM_CONTEXT_ALLOC_HEADER(buffer), size)));
}

/**********************************************************************************************************************************/
void
memFree(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buffer);
    FUNCTION_TEST_END();

    ASSERT_ALLOC_VALID(MEM_CONTEXT_ALLOC_HEADER(buffer));

    // Get the allocation
    MemContext *contextCurrent = memContextStack[memContextCurrentStackIdx].memContext;
    MemContextAlloc *alloc = MEM_CONTEXT_ALLOC_HEADER(buffer);

    // If this allocation is before the current free allocation then make it the current free allocation
    if (alloc->allocIdx < contextCurrent->allocFreeIdx)
        contextCurrent->allocFreeIdx = alloc->allocIdx;

    // Free the allocation
    contextCurrent->allocList[alloc->allocIdx] = NULL;
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
        // Null out the context in the old parent
        ASSERT(this->contextParent->contextChildList[this->contextParentIdx] == this);
        this->contextParent->contextChildList[this->contextParentIdx] = NULL;

        // Find a place in the new parent context and assign it. The child list may be moved while finding a new index so store the
        // index and use it with (what might be) the new pointer.
        this->contextParent = parentNew;
        this->contextParentIdx = memContextNewIndex(parentNew, false);
        ASSERT(parentNew->contextChildList[this->contextParentIdx] == NULL);
        parentNew->contextChildList[this->contextParentIdx] = this;
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
    FUNCTION_TEST_RETURN(&contextTop);
}

/**********************************************************************************************************************************/
MemContext *
memContextCurrent(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(memContextStack[memContextCurrentStackIdx].memContext);
}

/**********************************************************************************************************************************/
bool
memContextFreeing(const MemContext *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->state == memContextStateFreeing);
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

    FUNCTION_TEST_RETURN(this->name);
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

    FUNCTION_TEST_RETURN(memContextStack[memContextCurrentStackIdx - priorIdx].memContext);
}

/**********************************************************************************************************************************/
size_t
memContextSize(const MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    // Size of struct and child context/alloc arrays
    size_t result =
        sizeof(MemContext) + (this->contextChildListSize * sizeof(MemContext *)) +
        (this->allocListSize * sizeof(MemContextAlloc *));

    // Add child contexts
    for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
    {
        if (this->contextChildList[contextIdx])
            result += memContextSize(this->contextChildList[contextIdx]);
    }

    // Add allocations
    for (unsigned int allocIdx = 0; allocIdx < this->allocListSize; allocIdx++)
    {
        if (this->allocList[allocIdx] != NULL)
            result += this->allocList[allocIdx]->size;
    }

    FUNCTION_TEST_RETURN(result);
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
        if (this == memContextStack[memContextCurrentStackIdx].memContext && this != &contextTop)
            THROW_FMT(AssertError, "cannot free current context '%s'", this->name);

        // Error if context is not active
        if (this->state != memContextStateActive)
            THROW(AssertError, "cannot free inactive context");

        // Free child contexts
        for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
            if (this->contextChildList[contextIdx] && this->contextChildList[contextIdx]->state == memContextStateActive)
                memContextFree(this->contextChildList[contextIdx]);

        // Set state to freeing now that there are no child contexts.  Child contexts might need to interact with their parent while
        // freeing so the parent needs to remain active until they are all gone.
        this->state = memContextStateFreeing;

        // Execute callback if defined
        TRY_BEGIN()
        {
            if (this->callbackFunction)
                this->callbackFunction(this->callbackArgument);
        }
        // Finish cleanup even if the callback fails
        FINALLY()
        {
            // Free child context allocations
            if (this->contextChildListSize > 0)
            {
                for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
                    if (this->contextChildList[contextIdx])
                        memFreeInternal(this->contextChildList[contextIdx]);

                memFreeInternal(this->contextChildList);
                this->contextChildListSize = 0;
            }

            // Free memory allocations
            if (this->allocListSize > 0)
            {
                for (unsigned int allocIdx = 0; allocIdx < this->allocListSize; allocIdx++)
                    if (this->allocList[allocIdx] != NULL)
                        memFreeInternal(this->allocList[allocIdx]);

                memFreeInternal(this->allocList);
                this->allocListSize = 0;
            }

            // If the context index is lower than the current free index in the parent then replace it
            if (this->contextParent != NULL && this->contextParentIdx < this->contextParent->contextChildFreeIdx)
                this->contextParent->contextChildFreeIdx = this->contextParentIdx;

            // Make top context active again
            if (this == &contextTop)
                this->state = memContextStateActive;
            // Else reset the memory context so it can be reused
            else
                *this = (MemContext){.state = memContextStateFree};
        }
        TRY_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
