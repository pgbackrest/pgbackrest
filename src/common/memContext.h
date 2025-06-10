/***********************************************************************************************************************************
Memory Context Manager

Memory is allocated inside contexts and all allocations (and child memory contexts) are freed when the context is freed. The goal
is to make memory management both easier and more performant.

Memory context management is encapsulated in macros so there is rarely any need to call the functions directly. Memory allocations
are mostly performed in the constructors of objects and reallocated as needed.

See the sections on memory context management and memory allocations below for more details.
***********************************************************************************************************************************/
#ifndef COMMON_MEMCONTEXT_H
#define COMMON_MEMCONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/***********************************************************************************************************************************
Memory context object
***********************************************************************************************************************************/
typedef struct MemContext MemContext;

#include "common/type/param.h"

/***********************************************************************************************************************************
Define initial number of memory contexts

No space is reserved for child contexts when a new context is created because most contexts will be leaves. When a child context is
requested then space will be reserved for this many child contexts initially. When more space is needed the size will be doubled.
***********************************************************************************************************************************/
#define MEM_CONTEXT_INITIAL_SIZE                                    4

/***********************************************************************************************************************************
Define initial number of memory allocations

Space is reserved for this many allocations when a context is created. When more space is needed the size will be doubled.
***********************************************************************************************************************************/
#define MEM_CONTEXT_ALLOC_INITIAL_SIZE                              4

/***********************************************************************************************************************************
Functions and macros to audit a mem context by detecting new child contexts/allocations that were created begin the begin/end but
are not the expected return type.
***********************************************************************************************************************************/
#if defined(DEBUG)

typedef struct MemContextAuditState
{
    MemContext *memContext;                                         // Mem context to audit

    bool returnTypeAny;                                             // Skip auditing for this mem context
    uint64_t sequenceContextNew;                                    // Max sequence for new contexts at beginning
} MemContextAuditState;

// Begin the audit
FN_EXTERN void memContextAuditBegin(MemContextAuditState *state);

// End the audit and make sure the return type is as expected
FN_EXTERN void memContextAuditEnd(const MemContextAuditState *state, const char *returnTypeDefault);

// Rename a mem context using the extra allocation pointer
#define MEM_CONTEXT_AUDIT_ALLOC_EXTRA_NAME(this, name)              memContextAuditAllocExtraName(this, #name)

FN_EXTERN void *memContextAuditAllocExtraName(void *allocExtra, const char *name);

#else
#define MEM_CONTEXT_AUDIT_ALLOC_EXTRA_NAME(this, name)              this
#endif

/***********************************************************************************************************************************
Memory management functions

All these functions operate in the current memory context, including memResize() and memFree().
***********************************************************************************************************************************/
// Allocate memory in the current memory context
FN_EXTERN void *memNew(size_t size);

// Allocate requested number of pointers and initialize them to NULL
FN_EXTERN void *memNewPtrArray(size_t size);

// Reallocate to the new size. Original buffer pointer is undefined on return.
FN_EXTERN void *memResize(void *buffer, size_t size);

// Free memory allocation
FN_EXTERN void memFree(void *buffer);

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
        memContextSwitch(memContext); /* Switch to the new memory context */

#define MEM_CONTEXT_END()                                                                                                          \
        memContextSwitchBack(); /* Switch back to the prior context */                                                             \
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

MEM_CONTEXT_NEW_BEGIN(memContextName, ...)
{
    <The mem context created is now the current context and can be accessed with the MEM_CONTEXT_NEW() macro>
    <If extra memory was allocation with the context if can be accessed with the MEM_CONTEXT_NEW_ALLOC() macro>
    <Prior context can be accessed with the memContextPrior() function>
    <On error the newly created context will be freed and the error rethrown>
}
MEM_CONTEXT_NEW_END();

<Prior memory context is restored>

Note that memory context names are expected to live for the lifetime of the context -- no copy is made.
***********************************************************************************************************************************/
#define MEM_CONTEXT_NEW()                                                                                                          \
    MEM_CONTEXT_NEW_memContext

#define MEM_CONTEXT_NEW_BEGIN(memContextName, ...)                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        MemContext *MEM_CONTEXT_NEW() = memContextNewP(STRINGIFY(memContextName), __VA_ARGS__);                                    \
        memContextSwitch(MEM_CONTEXT_NEW());

#define MEM_CONTEXT_NEW_ALLOC()                                                                                                    \
    memContextAllocExtra(MEM_CONTEXT_NEW())

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
        MemContext *MEM_CONTEXT_TEMP() = memContextNewP(                                                                           \
            "temporary", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX);                                        \
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
            MEM_CONTEXT_TEMP() = memContextNewP("temporary", .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX);    \
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

memContextSwitch(memContextNewP());

<Do something with the memory context, e.g. allocation memory with memNew()>
<Current memory context can be accessed with memContextCurrent()>
<Prior memory context can be accessed with memContextPrior()>

memContextSwitchBack();

<The memory context must now be kept or discarded>
memContextKeep()/memContextDiscard();

There is no need to implement any error handling. The mem context system will automatically clean up any mem contexts that were
created but not marked as keep when an error occurs and reset the current mem context to whatever it was at the beginning of the
nearest try block.

Use the MEM_CONTEXT*() macros when possible rather than reimplement the boilerplate for every memory context block.
***********************************************************************************************************************************/
// Create a new mem context in the current mem context. The new context must be either kept with memContextKeep() or discarded with
// memContextDiscard() before switching back from the parent context.
typedef struct MemContextNewParam
{
    VAR_PARAM_HEADER;
    uint8_t childQty;                                               // How many child contexts can this context have?
    uint8_t allocQty;                                               // How many allocations can this context have?
    uint8_t callbackQty;                                            // How many callbacks can this context have?
    uint16_t allocExtra;                                            // Extra memory to allocate with the context
} MemContextNewParam;

// Maximum amount of extra memory that can be allocated with the context using allocExtra
#define MEM_CONTEXT_ALLOC_EXTRA_MAX                                 UINT16_MAX

// Specify maximum quantity of child contexts or allocations using childQty or allocQty
#define MEM_CONTEXT_QTY_MAX                                         UINT8_MAX

#ifdef DEBUG
#define memContextNewP(name, ...)                                                                                                  \
    memContextNew(name, (MemContextNewParam){VAR_PARAM_INIT, __VA_ARGS__})
#else
#define memContextNewP(name, ...)                                                                                                  \
    memContextNew((MemContextNewParam){VAR_PARAM_INIT, __VA_ARGS__})
#endif

FN_EXTERN MemContext *memContextNew(
#ifdef DEBUG
    const char *name,
#endif
    MemContextNewParam param);

// Switch to a context making it the current mem context
FN_EXTERN void memContextSwitch(MemContext *this);

// Switch back to the context that was current before the last switch. If the last function called was memContextNewP(), then
// memContextKeep()/memContextDiscard() must be called before calling memContextSwitchBack(), otherwise an error will occur in
// debug builds and the behavior is undefined in production builds.
FN_EXTERN void memContextSwitchBack(void);

// Keep a context created by memContextNewP() so that it will not be automatically freed if an error occurs. If the context was
// switched after the call to memContextNewP(), then it must be switched back before calling memContextKeep() or an error will occur
// in debug builds and the behavior is undefined in production builds.
FN_EXTERN void memContextKeep(void);

// Discard a context created by memContextNewP(). If the context was switched after the call to memContextNewP(), then it must be
// switched back before calling memContextDiscard() or an error will occur in debug builds and the behavior is undefined in
// production builds.
FN_EXTERN void memContextDiscard(void);

// Move mem context to a new parent. This is generally used to move objects to a new context once they have been successfully
// created.
FN_EXTERN void memContextMove(MemContext *this, MemContext *parentNew);

// Set a function that will be called when this mem context is freed
FN_EXTERN void memContextCallbackSet(MemContext *this, void (*callbackFunction)(void *), void *);

// Clear the callback function so it won't be called when the mem context is freed. This is usually done in the object free method
// after resources have been freed but before memContextFree() is called. The goal is to prevent the object free method from being
// called more than once.
FN_EXTERN void memContextCallbackClear(MemContext *this);

// Free a memory context
FN_EXTERN void memContextFree(MemContext *this);

/***********************************************************************************************************************************
Memory context getters
***********************************************************************************************************************************/
// Pointer to the extra memory allocated with the mem context
FN_EXTERN void *memContextAllocExtra(MemContext *this);

// Get mem context using pointer to the memory allocated with the mem context
FN_EXTERN MemContext *memContextFromAllocExtra(void *allocExtra);

// Current memory context
FN_EXTERN MemContext *memContextCurrent(void);

// Prior context, i.e. the context that was current before the last memContextSwitch()
FN_EXTERN MemContext *memContextPrior(void);

// "top" context. This context is created at initialization and is always present, i.e. it is never freed. The top context is a good
// place to put long-lived mem contexts since they won't be automatically freed until the program exits.
FN_EXTERN MemContext *memContextTop(void);

// Get total size of mem context and all children
#ifdef DEBUG
FN_EXTERN size_t memContextSize(const MemContext *this);
#endif // DEBUG

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MEM_CONTEXT_TYPE                                                                                              \
    MemContext *
#define FUNCTION_LOG_MEM_CONTEXT_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "MemContext", buffer, bufferSize)

/***********************************************************************************************************************************
Internal functions
***********************************************************************************************************************************/
// Clean up mem contexts after an error. Should only be called from error handling routines.
FN_EXTERN void memContextClean(unsigned int tryDepth, bool fatal);

#endif
