/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Memory context states
***********************************************************************************************************************************/
typedef enum {memContextStateFree = 0, memContextStateFreeing, memContextStateActive} MemContextState;

/***********************************************************************************************************************************
Contains information about a memory allocation
***********************************************************************************************************************************/
typedef struct MemContextAlloc
{
    bool active:1;                                                  // Is the allocation active?
    unsigned int size:32;                                           // Allocation size (4GB max)
    void *buffer;                                                   // Allocated buffer
} MemContextAlloc;

/***********************************************************************************************************************************
Contains information about the memory context
***********************************************************************************************************************************/
struct MemContext
{
    MemContextState state;                                          // Current state of the context
    const char name[MEM_CONTEXT_NAME_SIZE + 1];                     // Indicates what the context is being used for

    MemContext *contextParent;                                      // All contexts have a parent except top

    MemContext **contextChildList;                                  // List of contexts created in this context
    unsigned int contextChildListSize;                              // Size of child context list (not the actual count of contexts)

    MemContextAlloc *allocList;                                     // List of memory allocations created in this context
    unsigned int allocListSize;                                     // Size of alloc list (not the actual count of allocations)

    MemContextCallback callbackFunction;                            // Function to call before the context is freed
    void *callbackArgument;                                         // Argument to pass to callback function
};

/***********************************************************************************************************************************
Top context

The top context always exists and can never be freed.  All other contexts are children of the top context. The top context is
generally used to allocate memory that exists for the life of the program.
***********************************************************************************************************************************/
MemContext contextTop = {.state = memContextStateActive, .name = "TOP"};

/***********************************************************************************************************************************
Current context

All memory allocations will be done from the current context.  Initialized to top context at execution start.
***********************************************************************************************************************************/
MemContext *contextCurrent = &contextTop;

/***********************************************************************************************************************************
Wrapper around malloc()
***********************************************************************************************************************************/
static void *
memAllocInternal(size_t size, bool zero)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(BOOL, zero);
    FUNCTION_TEST_END();

    // Allocate memory
    void *buffer = malloc(size);

    // Error when malloc fails
    if (buffer == NULL)
        THROW_FMT(MemoryError, "unable to allocate %zu bytes", size);

    // Zero the memory when requested
    if (zero)
        memset(buffer, 0, size);

    // Return the buffer
    FUNCTION_TEST_RETURN(buffer);
}

/***********************************************************************************************************************************
Wrapper around realloc()
***********************************************************************************************************************************/
static void *
memReAllocInternal(void *bufferOld, size_t sizeOld, size_t sizeNew, bool zeroNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, bufferOld);
        FUNCTION_TEST_PARAM(SIZE, sizeOld);
        FUNCTION_TEST_PARAM(SIZE, sizeNew);
        FUNCTION_TEST_PARAM(BOOL, zeroNew);
    FUNCTION_TEST_END();

    ASSERT(bufferOld != NULL);

    // Allocate memory
    void *bufferNew = realloc(bufferOld, sizeNew);

    // Error when realloc fails
    if (bufferNew == NULL)
        THROW_FMT(MemoryError, "unable to reallocate %zu bytes", sizeNew);

    // Zero the new memory when requested - old memory is left untouched else why bother with a realloc?
    if (zeroNew)
        memset((unsigned char *)bufferNew + sizeOld, 0, sizeNew - sizeOld);

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
        FUNCTION_TEST_PARAM(VOIDP, buffer);
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
    unsigned int contextIdx;

    for (contextIdx = 0; contextIdx < memContext->contextChildListSize; contextIdx++)
    {
        if (!memContext->contextChildList[contextIdx] ||
            (allowFree && memContext->contextChildList[contextIdx]->state == memContextStateFree))
        {
            break;
        }
    }

    // If no space was found then allocate more
    if (contextIdx == memContext->contextChildListSize)
    {
        // If no space has been allocated to the list
        if (memContext->contextChildListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memAllocInternal(sizeof(MemContext *) * MEM_CONTEXT_INITIAL_SIZE, true);

            // Set new list size
            memContext->contextChildListSize = MEM_CONTEXT_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int contextChildListSizeNew = memContext->contextChildListSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            memContext->contextChildList = memReAllocInternal(
                memContext->contextChildList, sizeof(MemContext *) * memContext->contextChildListSize,
                sizeof(MemContext *) * contextChildListSizeNew, true);

            // Set new list size
            memContext->contextChildListSize = contextChildListSizeNew;
        }
    }

    FUNCTION_TEST_RETURN(contextIdx);
}

/***********************************************************************************************************************************
Create a new memory context
***********************************************************************************************************************************/
MemContext *
memContextNew(const char *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, name);
    FUNCTION_TEST_END();

    ASSERT(name != NULL);

    // Check context name length
    if (strlen(name) == 0 || strlen(name) > MEM_CONTEXT_NAME_SIZE)
        THROW_FMT(AssertError, "context name length must be > 0 and <= %d", MEM_CONTEXT_NAME_SIZE);

    // Find space for the new context
    unsigned int contextIdx = memContextNewIndex(memContextCurrent(), true);

    // If the context has not been allocated yet
    if (!memContextCurrent()->contextChildList[contextIdx])
        memContextCurrent()->contextChildList[contextIdx] = memAllocInternal(sizeof(MemContext), true);

    // Get the context
    MemContext *this = memContextCurrent()->contextChildList[contextIdx];

    // Create initial space for allocations
    this->allocList = memAllocInternal(sizeof(MemContextAlloc) * MEM_CONTEXT_ALLOC_INITIAL_SIZE, true);
    this->allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE;

    // Set the context name
    strcpy((char *)this->name, name);

    // Set new context active
    this->state = memContextStateActive;

    // Set current context as the parent
    this->contextParent = memContextCurrent();

    // Return context
    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Register a callback to be called just before the context is freed
***********************************************************************************************************************************/
void
memContextCallback(MemContext *this, void (*callbackFunction)(void *), void *callbackArgument)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
        FUNCTION_TEST_PARAM(FUNCTIONP, callbackFunction);
        FUNCTION_TEST_PARAM(VOIDP, callbackArgument);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(callbackFunction != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot assign callback to inactive context");

    // Top context cannot have a callback
    if (this == memContextTop())
        THROW(AssertError, "top context may not have a callback");

    // Error if callback has already been set - there may be valid use cases for this but error until one is found
    if (this->callbackFunction)
        THROW_FMT(AssertError, "callback is already set for context '%s'", this->name);

    // Set callback function and argument
    this->callbackFunction = callbackFunction;
    this->callbackArgument = callbackArgument;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Clear the mem context callback.  This is usually done in the object free method after resources have been freed but before
memContextFree() is called.  The goal is to prevent the object free method from being called more than once.
***********************************************************************************************************************************/
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
    ASSERT(this != memContextTop());

    // Clear callback function and argument
    this->callbackFunction = NULL;
    this->callbackArgument = NULL;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Allocate memory in the memory context and optionally zero it.
***********************************************************************************************************************************/
static void *
memContextAlloc(size_t size, bool zero)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
        FUNCTION_TEST_PARAM(BOOL, zero);
    FUNCTION_TEST_END();

    // Find space for the new allocation
    unsigned int allocIdx;

    for (allocIdx = 0; allocIdx < memContextCurrent()->allocListSize; allocIdx++)
        if (!memContextCurrent()->allocList[allocIdx].active)
            break;

    // If no space was found then allocate more
    if (allocIdx == memContextCurrent()->allocListSize)
    {
        // Only the top context will not have initial space for allocations
        if (memContextCurrent()->allocListSize == 0)
        {
            // Allocate memory before modifying anything else in case there is an error
            memContextCurrent()->allocList = memAllocInternal(sizeof(MemContextAlloc) * MEM_CONTEXT_ALLOC_INITIAL_SIZE, true);

            // Set new size
            memContextCurrent()->allocListSize = MEM_CONTEXT_ALLOC_INITIAL_SIZE;
        }
        // Else grow the list
        else
        {
            // Calculate new list size
            unsigned int allocListSizeNew = memContextCurrent()->allocListSize * 2;

            // ReAllocate memory before modifying anything else in case there is an error
            memContextCurrent()->allocList = memReAllocInternal(
                memContextCurrent()->allocList, sizeof(MemContextAlloc) * memContextCurrent()->allocListSize,
                sizeof(MemContextAlloc) * allocListSizeNew, true);

            // Set new size
            memContextCurrent()->allocListSize = allocListSizeNew;
        }
    }

    // Allocate the memory
    memContextCurrent()->allocList[allocIdx].active = true;
    memContextCurrent()->allocList[allocIdx].size = (unsigned int)size;
    memContextCurrent()->allocList[allocIdx].buffer = memAllocInternal(size, zero);

    // Return buffer
    FUNCTION_TEST_RETURN(memContextCurrent()->allocList[allocIdx].buffer);
}

/***********************************************************************************************************************************
Find a memory allocation
***********************************************************************************************************************************/
static unsigned int
memFind(const void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find memory allocation
    unsigned int allocIdx;

    for (allocIdx = 0; allocIdx < memContextCurrent()->allocListSize; allocIdx++)
        if (memContextCurrent()->allocList[allocIdx].buffer == buffer && memContextCurrent()->allocList[allocIdx].active)
            break;

    // Error if the buffer was not found
    if (allocIdx == memContextCurrent()->allocListSize)
        THROW(AssertError, "unable to find allocation");

    FUNCTION_TEST_RETURN(allocIdx);
}

/***********************************************************************************************************************************
Allocate zeroed memory in the memory context
***********************************************************************************************************************************/
void *
memNew(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(memContextAlloc(size, true));
}

/***********************************************************************************************************************************
Grow allocated memory without initializing the new portion
***********************************************************************************************************************************/
void *
memGrowRaw(const void *buffer, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, buffer);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find the allocation
    MemContextAlloc *alloc = &(memContextCurrent()->allocList[memFind(buffer)]);

    // Grow the buffer
    alloc->buffer = memReAllocInternal(alloc->buffer, alloc->size, size, false);
    alloc->size = (unsigned int)size;

    FUNCTION_TEST_RETURN(alloc->buffer);
}

/***********************************************************************************************************************************
Allocate memory in the memory context without initializing it
***********************************************************************************************************************************/
void *
memNewRaw(size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(memContextAlloc(size, false));
}

/***********************************************************************************************************************************
Free a memory allocation in the context
***********************************************************************************************************************************/
void
memFree(void *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Find the allocation
    MemContextAlloc *alloc = &(memContextCurrent()->allocList[memFind(buffer)]);

    // Free the buffer
    memFreeInternal(alloc->buffer);
    alloc->active = false;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Move a context to a new parent context

This is generally used to move objects to a new context once they have been successfully created.
***********************************************************************************************************************************/
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
        // Find context in the old parent and NULL it out
        MemContext *parentOld = this->contextParent;
        unsigned int contextIdx;

        for (contextIdx = 0; contextIdx < parentOld->contextChildListSize; contextIdx++)
        {
            if (parentOld->contextChildList[contextIdx] == this)
            {
                parentOld->contextChildList[contextIdx] = NULL;
                break;
            }
        }

        // The memory must be found
        if (contextIdx == parentOld->contextChildListSize)
            THROW(AssertError, "unable to find mem context in old parent");

        // Find a place in the new parent context and assign it. The child list may be moved while finding a new index so store the
        // index and use it with (what might be) the new pointer.
        contextIdx = memContextNewIndex(parentNew, false);
        ASSERT(parentNew->contextChildList[contextIdx] == NULL);
        parentNew->contextChildList[contextIdx] = this;

        // Assign new parent
        this->contextParent = parentNew;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Switch to the specified context and return the old context
***********************************************************************************************************************************/
MemContext *
memContextSwitch(MemContext *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MEM_CONTEXT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Error if context is not active
    if (this->state != memContextStateActive)
        THROW(AssertError, "cannot switch to inactive context");

    MemContext *memContextOld = memContextCurrent();
    contextCurrent = this;

    FUNCTION_TEST_RETURN(memContextOld);
}

/***********************************************************************************************************************************
Return the top context
***********************************************************************************************************************************/
MemContext *
memContextTop(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(&contextTop);
}

/***********************************************************************************************************************************
Return the current context
***********************************************************************************************************************************/
MemContext *
memContextCurrent(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN(contextCurrent);
}

/***********************************************************************************************************************************
Return the context name
***********************************************************************************************************************************/
const char *
memContextName(MemContext *this)
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

/***********************************************************************************************************************************
memContextFree - free all memory used by the context and all child contexts
***********************************************************************************************************************************/
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
        if (this == memContextCurrent() && this != memContextTop())
            THROW_FMT(AssertError, "cannot free current context '%s'", this->name);

        // Error if context is not active
        if (this->state != memContextStateActive)
            THROW(AssertError, "cannot free inactive context");

        // Free child contexts
        if (this->contextChildListSize > 0)
            for (unsigned int contextIdx = 0; contextIdx < this->contextChildListSize; contextIdx++)
                if (this->contextChildList[contextIdx] && this->contextChildList[contextIdx]->state == memContextStateActive)
                    memContextFree(this->contextChildList[contextIdx]);

        // Set state to freeing now that there are no child contexts.  Child contexts might need to interact with their parent while
        // freeing so the parent needs to remain active until they are all gone.
        this->state = memContextStateFreeing;

        // Execute callback if defined
        if (this->callbackFunction)
            this->callbackFunction(this->callbackArgument);

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
            {
                MemContextAlloc *alloc = &(this->allocList[allocIdx]);

                if (alloc->active)
                    memFreeInternal(alloc->buffer);
            }

            memFreeInternal(this->allocList);
            this->allocListSize = 0;
        }

        // Make top context active again
        if (this == memContextTop())
            this->state = memContextStateActive;
        // Else reset the memory context so it can be reused
        else
            memset(this, 0, sizeof(MemContext));
    }

    FUNCTION_TEST_RETURN_VOID();
}
