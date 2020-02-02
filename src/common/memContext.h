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
MemContext *contextPrior = memContextSwitch(context);

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
    memContextSwitch(contextPrior);
}
TRY_END();

Use the MEM_CONTEXT*() macros when possible rather than implement error-handling for every memory context block.
***********************************************************************************************************************************/
MemContext *memContextNew(const char *name);
void memContextMove(MemContext *this, MemContext *parentNew);
void memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *);
void memContextCallbackClear(MemContext *this);
MemContext *memContextSwitch(MemContext *this);
void memContextFree(MemContext *this);

/***********************************************************************************************************************************
Memory context accessors
***********************************************************************************************************************************/
MemContext *memContextCurrent(void);
MemContext *memContextTop(void);
const char *memContextName(MemContext *this);

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
    <Prior context can be accessed with the memContextPrior() macro>
}
MEM_CONTEXT_END();

<Prior memory context is restored>
***********************************************************************************************************************************/
#define MEM_CONTEXT_BEGIN(memContext)                                                                                              \
{                                                                                                                                  \
    /* Switch to the new memory context */                                                                                         \
    MemContext *MEM_CONTEXT_memContextPrior = memContextSwitch(memContext);                                                        \
                                                                                                                                   \
    /* Try the statement block */                                                                                                  \
    TRY_BEGIN()

#define memContextPrior()                                                                                                          \
    MEM_CONTEXT_memContextPrior

#define MEM_CONTEXT_END()                                                                                                          \
    /* Switch back to the prior context */                                                                                         \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
        memContextSwitch(memContextPrior());                                                                                       \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
}

/***********************************************************************************************************************************
Switch to prior context and ensure that the previous prior memory context is restored after the block executes (even on error)

MEM_CONTEXT_PRIOR_BEGIN(memContext)
{
    <The mem context specified is now the prior context>
}
MEM_CONTEXT_PRIOR_END();

<Previous prior memory context is restored>
***********************************************************************************************************************************/
#define MEM_CONTEXT_PRIOR_BEGIN(memContext)                                                                                        \
{                                                                                                                                  \
    /* Switch to the new memory context */                                                                                         \
    MemContext *MEM_CONTEXT_memContextRestore = memContextSwitch(memContextPrior());                                               \
                                                                                                                                   \
    /* Try the statement block */                                                                                                  \
    TRY_BEGIN()

#define MEM_CONTEXT_PRIOR_END()                                                                                                    \
    /* Switch back to the prior context */                                                                                         \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
        memContextSwitch(MEM_CONTEXT_memContextRestore);                                                                           \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
}

/***********************************************************************************************************************************
Create a new context and make sure it is freed on error and prior context is restored in all cases

MEM_CONTEXT_NEW_BEGIN(memContextName)
{
    <The mem context created is now the current context and can be accessed with the MEM_CONTEXT_NEW() macro>

    ObjectType *object = memNew(sizeof(ObjectType));
    object->memContext = MEM_CONTEXT_NEW();

    <Prior context can be accessed with the memContextPrior() macro>
    <On error the newly created context will be freed and the error rethrown>
}
MEM_CONTEXT_NEW_END();

<Prior memory context is restored>

Note that memory context names are expected to live for the lifetime of the context -- no copy is made.
***********************************************************************************************************************************/
#define MEM_CONTEXT_NEW()                                                                                                          \
    MEM_CONTEXT_NEW_memContext

#define MEM_CONTEXT_NEW_BEGIN(memContextName)                                                                                      \
{                                                                                                                                  \
    MemContext *MEM_CONTEXT_NEW() = memContextNew(memContextName);                                                                 \
                                                                                                                                   \
    MEM_CONTEXT_BEGIN(MEM_CONTEXT_NEW())

#define MEM_CONTEXT_NEW_END()                                                                                                      \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        memContextSwitch(memContextPrior());                                                                                       \
        memContextFree(MEM_CONTEXT_NEW());                                                                                         \
        RETHROW();                                                                                                                 \
    }                                                                                                                              \
    MEM_CONTEXT_END();                                                                                                             \
}

/***********************************************************************************************************************************
Create a temporary memory context and make sure it is freed when done (even on error)

MEM_CONTEXT_TEMP_BEGIN()
{
    <A temp memory context is now the current context>
    <Temp context can be accessed with the MEM_CONTEXT_TEMP() macro>
    <Prior context can be accessed with the memContextPrior() macro>
}
MEM_CONTEXT_TEMP_END();

<Prior memory context is restored>
<Temp memory context is freed>
***********************************************************************************************************************************/
#define MEM_CONTEXT_TEMP()                                                                                                         \
    MEM_CONTEXT_TEMP_memContext

#define MEM_CONTEXT_TEMP_BEGIN()                                                                                                   \
{                                                                                                                                  \
    MemContext *MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                   \
                                                                                                                                   \
    MEM_CONTEXT_BEGIN(MEM_CONTEXT_TEMP())

#define MEM_CONTEXT_TEMP_RESET_BEGIN()                                                                                             \
{                                                                                                                                  \
    MemContext *MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                   \
    unsigned int MEM_CONTEXT_TEMP_loopTotal = 0;                                                                                   \
                                                                                                                                   \
    MEM_CONTEXT_BEGIN(MEM_CONTEXT_TEMP())

#define MEM_CONTEXT_TEMP_RESET(resetTotal)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        MEM_CONTEXT_TEMP_loopTotal++;                                                                                              \
                                                                                                                                   \
        if (MEM_CONTEXT_TEMP_loopTotal >= resetTotal)                                                                              \
        {                                                                                                                          \
            memContextSwitch(memContextPrior());                                                                                   \
            memContextFree(MEM_CONTEXT_TEMP());                                                                                    \
            MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                       \
            memContextSwitch(MEM_CONTEXT_TEMP());                                                                                  \
            MEM_CONTEXT_TEMP_loopTotal = 0;                                                                                        \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

#define MEM_CONTEXT_TEMP_END()                                                                                                     \
        /* Switch back to the prior context and free temp context */                                                               \
        FINALLY()                                                                                                                  \
        {                                                                                                                          \
            memContextSwitch(memContextPrior());                                                                                   \
            memContextFree(MEM_CONTEXT_TEMP());                                                                                    \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MEM_CONTEXT_TYPE                                                                                              \
    MemContext *
#define FUNCTION_LOG_MEM_CONTEXT_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "MemContext", buffer, bufferSize)

#endif
