/***********************************************************************************************************************************
Memory Context Manager
***********************************************************************************************************************************/
#ifndef COMMON_MEMCONTEXT_H
#define COMMON_MEMCONTEXT_H

#include "common/error.h"
#include "common/type.h"

/***********************************************************************************************************************************
Memory context names cannot be larger than this size (excluding terminator) or an error will be thrown
***********************************************************************************************************************************/
#define MEM_CONTEXT_NAME_SIZE                                       64

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
Memory context object
***********************************************************************************************************************************/
typedef struct MemContext MemContext;

/***********************************************************************************************************************************
Memory context callback function type, useful for casts in memContextCallback()
***********************************************************************************************************************************/
typedef void (*MemContextCallback)(void *callbackArgument);

/***********************************************************************************************************************************
Memory context management functions

MemContext *context = memContextNew();
MemContext *contextOld = memContextSwitch(context);

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
    memContextSwitch(context);
}
TRY_END();

Use the MEM_CONTEXT*() macros when possible rather than implement error-handling for every memory context block.
***********************************************************************************************************************************/
MemContext *memContextNew(const char *name);
void memContextCallback(MemContext *this, void (*callbackFunction)(void *), void *callbackArgument);
MemContext *memContextSwitch(MemContext *this);
void memContextFree(MemContext *this);

/***********************************************************************************************************************************
Memory context accessors
***********************************************************************************************************************************/
MemContext *memContextCurrent();
MemContext *memContextTop();
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
Ensure that the old memory context is restored after the block executes (even on error)

MEM_CONTEXT_BEGIN(memContext)
{
    <The mem context specified is now the current context>
    <Old context can be accessed with the MEM_CONTEXT_OLD() macro>
}
MEM_CONTEXT_END();

<Old memory context is restored>
***********************************************************************************************************************************/
#define MEM_CONTEXT_BEGIN(memContext)                                                                                              \
{                                                                                                                                  \
    /* Switch to the new memory context */                                                                                         \
    MemContext *MEM_CONTEXT_memContextOld = memContextSwitch(memContext);                                                          \
                                                                                                                                   \
    /* Try the statement block */                                                                                                  \
    TRY_BEGIN()

#define MEM_CONTEXT_OLD()                                                                                                          \
    MEM_CONTEXT_memContextOld

#define MEM_CONTEXT_END()                                                                                                          \
    /* Switch back to the old context */                                                                                           \
    FINALLY()                                                                                                                      \
    {                                                                                                                              \
        memContextSwitch(MEM_CONTEXT_OLD());                                                                                       \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
}

/***********************************************************************************************************************************
Create a new context and make sure it is freed on error and old context is restored in all cases

MEM_CONTEXT_NEW_BEGIN(memContextName)
{
    <The mem context created is now the current context and can be accessed with the MEM_CONTEXT_NEW() macro>

    ObjectType *object = memNew(sizeof(ObjectType));
    object->memContext = MEM_CONTEXT_NEW();

    <Old context can be accessed with the MEM_CONTEXT_OLD() macro>
    <On error the newly created context will be freed and the error rethrown>
}
MEM_CONTEXT_NEW_END();

<Old memory context is restored>
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
        memContextSwitch(MEM_CONTEXT_OLD());                                                                                       \
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
    <Old context can be accessed with the MEM_CONTEXT_OLD() macro>
}
MEM_CONTEXT_TEMP_END();

<Old memory context is restored>
<Temp memory context is freed>
***********************************************************************************************************************************/
#define MEM_CONTEXT_TEMP()                                                                                                         \
    MEM_CONTEXT_TEMP_memContext

#define MEM_CONTEXT_TEMP_BEGIN()                                                                                                   \
{                                                                                                                                  \
    MemContext *MEM_CONTEXT_TEMP() = memContextNew("temporary");                                                                   \
                                                                                                                                   \
    MEM_CONTEXT_BEGIN(MEM_CONTEXT_TEMP())

#define MEM_CONTEXT_TEMP_END()                                                                                                     \
        /* Switch back to the old context and free temp context */                                                                 \
        FINALLY()                                                                                                                  \
        {                                                                                                                          \
            memContextSwitch(MEM_CONTEXT_OLD());                                                                                   \
            memContextFree(MEM_CONTEXT_TEMP());                                                                                    \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    }                                                                                                                              \
}

#endif
