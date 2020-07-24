/***********************************************************************************************************************************
Memory Context Manager

Memory is allocated inside contexts and all allocations (and child memory contexts) are freed when the context is freed.  The goal
is to make memory management both easier and more performant.

Memory context management is encapsulated in macros so there is rarely any need to call the functions directly.  Memory allocations
are mostly performed in the constructors of objects and reallocated as needed.

See the sections on memory context management and memory allocations below for more details.
***********************************************************************************************************************************/
#ifndef COMMON_MEMCONTEXT_H
#define COMMON_MEMCONTEXT_H

#include <stddef.h>

/***********************************************************************************************************************************
Memory context object
***********************************************************************************************************************************/
typedef struct MemContext MemContext;

#include "common/error.h"

/***********************************************************************************************************************************
Define initial number of memory contexts

No space is reserved for child contexts when a new context is created because most contexts will be leaves.  When a child context is
requested then space will be reserved for this many child contexts initially.  When more space is needed the size will be doubled.
***********************************************************************************************************************************/
#define MEM_CONTEXT_INITIAL_SIZE                                    4

/***********************************************************************************************************************************
Define initial number of memory allocations

Space is reserved for this many allocations when a context is created.  When more space is needed the size will be doubled.
***********************************************************************************************************************************/
#define MEM_CONTEXT_ALLOC_INITIAL_SIZE                              4

/***********************************************************************************************************************************
Memory management functions

All these functions operate in the current memory context, including memResize() and memFree().
***********************************************************************************************************************************/
// Allocate memory in the current memory context
void *memNew(size_t size);

// Allocate requested number of pointers and initialize them to NULL
void *memNewPtrArray(size_t size);

// Reallocate to the new size.  Original buffer pointer is undefined on return.
void *memResize(const void *buffer, size_t size);

// Free memory allocation
void memFree(void *buffer);

/***********************************************************************************************************************************
Ensure that the prior memory context is restored after the block executes (even on error)

MEM_CONTEXT_BEGIN(memContext)
{
    <The mem context specified is now the current context>
    <Prior context can be accessed with the memContextPrior() function>
}
MEM_CONTEXT_END();

<Prior memory context is restored>
***********************************************************************************************************************************/
#define MEM_CONTEXT_BEGIN(memContext)                                                                                              \
    do                                                                                                                             \
    {                                                                                                                              \
        /* Switch to the new memory context */                                                                                     \
        memContextSwitch(memContext);

#define MEM_CONTEXT_END()                                                                                                          \
        /* Switch back to the prior context */                                                                                     \
        memContextSwitchBack();                                                                                                    \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Switch to prior context and ensure that the previous prior memory context is restored after the block executes (even on error)

MEM_CONTEXT_PRIOR_BEGIN(memContext)
{
    <The mem context specified is now the prior context>
}
MEM_CONTEXT_PRIOR_END();

<Previous prior memory context is restored>
***********************************************************************************************************************************/
#define MEM_CONTEXT_PRIOR_BEGIN()                                                                                                  \
    MEM_CONTEXT_BEGIN(memContextPrior())

#define MEM_CONTEXT_PRIOR_END()                                                                                                    \
    MEM_CONTEXT_END()

/***********************************************************************************************************************************
Create a new context and make sure it is freed on error and prior context is restored in all cases

MEM_CONTEXT_NEW_BEGIN(memContextName)
{
    <The mem context created is now the current context and can be accessed with the MEM_CONTEXT_NEW() macro>

    ObjectType *object = memNew(sizeof(ObjectType));
    object->memContext = MEM_CONTEXT_NEW();

    <Prior context can be accessed with the memContextPrior() function>
    <On error the newly created context will be freed and the error rethrown>
}
MEM_CONTEXT_NEW_END();

<Prior memory context is restored>

Note that memory context names are expected to live for the lifetime of the context -- no copy is made.
***********************************************************************************************************************************/
#define MEM_CONTEXT_NEW()                                                                                                          \
    MEM_CONTEXT_NEW_memContext

#define MEM_CONTEXT_NEW_BEGIN(memContextName)                                                                                      \
    do                                                                                                                             \
    {                                                                                                                              \
        MemContext *MEM_CONTEXT_NEW() = memContextNew(memContextName);                                                             \
        memContextSwitch(MEM_CONTEXT_NEW());

#define MEM_CONTEXT_NEW_END()                                                                                                      \
        memContextSwitchBack();                                                                                                    \
        memContextKeep();                                                                                                          \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Create a temporary memory context and make sure it is freed when done (even on error)

MEM_CONTEXT_TEMP_BEGIN()
{
    <A temp memory context is now the current context>
    <Temp context can be accessed with the MEM_CONTEXT_TEMP() macro>
    <Prior context can be accessed with the memContextPrior() function>
}
MEM_CONTEXT_TEMP_END();

<Prior memory context is restored>
<Temp memory context is freed>
***********************************************************************************************************************************/
#define MEM_CONTEXT_TEMP()                                                                                                         \
    MEM_CONTEXT_TEMP_memContext

#define MEM_CONTEXT_TEMP_BEGIN()                                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        MemContext *MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                               \
        memContextSwitch(MEM_CONTEXT_TEMP());

#define MEM_CONTEXT_TEMP_RESET_BEGIN()                                                                                             \
    MEM_CONTEXT_TEMP_BEGIN()                                                                                                       \
    unsigned int MEM_CONTEXT_TEMP_loopTotal = 0;

#define MEM_CONTEXT_TEMP_RESET(resetTotal)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        MEM_CONTEXT_TEMP_loopTotal++;                                                                                              \
                                                                                                                                   \
        if (MEM_CONTEXT_TEMP_loopTotal >= resetTotal)                                                                              \
        {                                                                                                                          \
            memContextSwitchBack();                                                                                                \
            memContextDiscard();                                                                                                   \
            MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                       \
            memContextSwitch(MEM_CONTEXT_TEMP());                                                                                  \
            MEM_CONTEXT_TEMP_loopTotal = 0;                                                                                        \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

#define MEM_CONTEXT_TEMP_END()                                                                                                     \
        memContextSwitchBack();                                                                                                    \
        memContextDiscard();                                                                                                       \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Memory context management functions

memContextSwitch(memContextNew());

<Do something with the memory context, e.g. allocation memory with memNew()>
<Current memory context can be accessed with memContextCurrent()>
<Prior memory context can be accessed with memContextPrior()>

memContextSwitchBack();

<The memory context must now be kept or discarded>
memContextKeep()/memContextDiscard();

There is no need to implement any error handling.  The mem context system will automatically clean up any mem contexts that were
created but not marked as keep when an error occurs and reset the current mem context to whatever it was at the beginning of the
nearest try block.

Use the MEM_CONTEXT*() macros when possible rather than reimplement the boilerplate for every memory context block.
***********************************************************************************************************************************/
// Create a new mem context in the current mem context. The new context must be either kept with memContextKeep() or discarded with
// memContextDisard() before switching back from the parent context.
MemContext *memContextNew(const char *name);

// Switch to a context making it the current mem context
void memContextSwitch(MemContext *this);

// Switch back to the context that was current before the last switch. If the last function called was memContextNew(), then
// memContextKeep()/memContextDiscard() must be called before calling memContextSwitchBack(), otherwise an error will occur in
// debug builds and the behavior is undefined in production builds.
void memContextSwitchBack(void);

// Keep a context created by memContextNew() so that it will not be automatically freed if an error occurs. If the context was
// switched after the call to memContextNew(), then it must be switched back before calling memContextKeep() or an error will occur
// in debug builds and the behavior is undefined in production builds.
void memContextKeep(void);

// Discard a context created by memContextNew(). If the context was switched after the call to memContextNew(), then it must be
// switched back before calling memContextDiscard() or an error will occur in debug builds and the behavior is undefined in
// production builds.
void memContextDiscard(void);

// Move mem context to a new parent. This is generally used to move objects to a new context once they have been successfully
// created.
void memContextMove(MemContext *this, MemContext *parentNew);

// Set a function that will be called when this mem context is freed
void memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *);

// Clear the callback function so it won't be called when the mem context is freed.  This is usually done in the object free method
// after resources have been freed but before memContextFree() is called.  The goal is to prevent the object free method from being
// called more than once.
void memContextCallbackClear(MemContext *this);

// Free a memory context
void memContextFree(MemContext *this);

// #ifdef DEBUG

// Get total size of mem context and all children
size_t memContextSize(const MemContext *this);

// #endif // DEBUG

/***********************************************************************************************************************************
Memory context getters
***********************************************************************************************************************************/
// Current memory context
MemContext *memContextCurrent(void);

// Prior context, i.e. the context that was current before the last memContextSwitch()
MemContext *memContextPrior(void);

// "top" context.  This context is created at initialization and is always present, i.e. it is never freed.  The top context is a
// good place to put long-lived mem contexts since they won't be automatically freed until the program exits.
MemContext *memContextTop(void);

// Mem context name
const char *memContextName(MemContext *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MEM_CONTEXT_TYPE                                                                                              \
    MemContext *
#define FUNCTION_LOG_MEM_CONTEXT_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "MemContext", buffer, bufferSize)

/***********************************************************************************************************************************
Internal functions
***********************************************************************************************************************************/
// Clean up mem contexts after an error.  Should only be called from error handling routines.
void memContextClean(unsigned int tryDepth);

#endif
