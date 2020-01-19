/***********************************************************************************************************************************
Memory Context Manager
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
Memory context management functions

MemContext *context = memContextNew();
memContextPush(context);

TRY_BEGIN()
{
    <Do something with the memory context>
}
CATCH_ANY()
{
    <only needed if the error renders the memory context useless - for instance in a constructor>

    memContextFree(context);
    RETHROW();
}
FINALLY
{
    memContextPop(context);
}
TRY_END();

Use the MEM_CONTEXT*() macros when possible rather than implement error-handling for every memory context block.
***********************************************************************************************************************************/
// Create a new mem context in the current mem context. The new context must be either kept with memContextKeep() or discarded with
// memContextDisard before the parent context can be popped off the stack.
MemContext *memContextNew(const char *name);

// Push a mem context onto the stack which will then become the current mem context
void memContextPush(MemContext *this);

// Pop a mem context off the stack and make the prior mem context the current mem context. If the top of the stack is a new mem
// context that has not been kept/discarded then an error will be thrown in debug builds.
void memContextPop(void);

// Keep a context created by memContextNew() so that it will not be automatically freed if an error occurs. If the top of the stack
// is not a new context then an error will be thrown in debug builds.
void memContextKeep(void);

// Discard a context created by memContextNew(). If the top of the stack is not a new context then an error will be thrown in debug
// builds.
void memContextDiscard(void);

// Move mem context to a new parent
void memContextMove(MemContext *this, MemContext *parentNew);

void memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *);
void memContextCallbackClear(MemContext *this);

// Free a memory context
void memContextFree(MemContext *this);

/***********************************************************************************************************************************
Memory context accessors
***********************************************************************************************************************************/
MemContext *memContextCurrent(void);
MemContext *memContextPrior(void);
MemContext *memContextTop(void);
const char *memContextName(MemContext *this);

/***********************************************************************************************************************************
Memory management

These functions always new/free within the current memory context.
***********************************************************************************************************************************/
void *memNew(size_t size);
void *memNewRaw(size_t size);
void *memGrowRaw(const void *buffer, size_t size);
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
        memContextPush(memContext);

#define MEM_CONTEXT_END()                                                                                                          \
        /* Switch back to the prior context */                                                                                     \
        memContextPop();                                                                                                           \
    }                                                                                                                              \
    while(0)

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
        memContextPush(MEM_CONTEXT_NEW());

#define MEM_CONTEXT_NEW_END()                                                                                                      \
        memContextPop();                                                                                                           \
        memContextKeep();                                                                                                          \
    }                                                                                                                              \
    while(0)

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
        memContextPush(MEM_CONTEXT_TEMP());

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
            memContextPop();                                                                                                       \
            memContextDiscard();                                                                                                   \
            MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                       \
            memContextPush(MEM_CONTEXT_TEMP());                                                                                    \
            MEM_CONTEXT_TEMP_loopTotal = 0;                                                                                        \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

#define MEM_CONTEXT_TEMP_END()                                                                                                     \
        memContextPop();                                                                                                           \
        memContextDiscard();                                                                                                       \
    }                                                                                                                              \
    while (0)

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
