/***********************************************************************************************************************************
Object Helper Macros and Functions

These macros and functions implement common object functionality.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_OBJECT_H
#define COMMON_TYPE_OBJECT_H

#include "common/assert.h"
#include "common/macro.h"
#include "common/memContext.h"

/***********************************************************************************************************************************
Create a new object

This is a thin wrapper on MEM_CONTEXT_NEW_*() to do object specific initialization. The general pattern
for a new object is:

MyObj *this = NULL;

OBJ_NEW_BEGIN(MyObj)
{
    this = OBJ_NEW_ALLOC();

    *this = (MyObj)
    {
        .data = ...
    };
}
OBJ_NEW_END();
***********************************************************************************************************************************/
#define OBJ_NEW_BEGIN(type)                                                                                                        \
    MEM_CONTEXT_NEW_BEGIN(STRINGIFY(type), .allocExtra = sizeof(type))

#define OBJ_NEW_ALLOC()                                                                                                            \
    memContextAllocExtra(memContextCurrent())

#define OBJ_NEW_END()                                                                                                              \
    MEM_CONTEXT_NEW_END()

/***********************************************************************************************************************************
Used in interface function parameter lists to discourage use of the untyped thisVoid parameter, e.g.:

size_t bufferRead(THIS_VOID, Buffer *buffer)

This macro should not be used unless the function is assigned to an interface or passed as a parameter.
***********************************************************************************************************************************/
#define THIS_VOID                                                   void *thisVoid

/***********************************************************************************************************************************
Create a local "this" variable of the correct type from a THIS_VOID parameter
***********************************************************************************************************************************/
#define THIS(type)                                                  type *this = thisVoid

/***********************************************************************************************************************************
Get the mem context of this object
***********************************************************************************************************************************/
#define THIS_MEM_CONTEXT()                                                                                                         \
    memContextFromAllocExtra(this)

/***********************************************************************************************************************************
Cast this private struct, e.g. List, to the associated public struct, e.g. ListPub. Note that the public struct must be the first
member of the private struct. For example:

__attribute__((always_inline)) static inline unsigned int
lstSize(const List *const this)
{
    return THIS_PUB(List)->listSize;
}

The macro also ensures that this != NULL so there is no need to do that in the calling function.
***********************************************************************************************************************************/
#define THIS_PUB(type)                                              ((type##Pub *)thisNotNull(this))

__attribute__((always_inline)) static inline const void *
thisNotNull(const void *const this)
{
    ASSERT_INLINE(this != NULL);
    return this;
}

/***********************************************************************************************************************************
Functions

To ensure proper type checking, these functions are meant to be called from inline functions created specifically for each object:

__attribute__((always_inline)) static inline void
storageFree(Storage *this)
{
    objFree(this);
}
***********************************************************************************************************************************/
// Get the object mem context
__attribute__((always_inline)) static inline MemContext *
objMemContext(void *const this)
{
    return memContextFromAllocExtra(this);
}

// Is the object mem context currently being freed?
__attribute__((always_inline)) static inline bool
objMemContextFreeing(const void *const this)
{
    return memContextFreeing(memContextConstFromAllocExtra(this));
}

// Move an object to a new context if this != NULL
void *objMove(THIS_VOID, MemContext *parentNew);

// Move an object to a new context if this != NULL. The mem context to move must be the first member of the object struct. This
// pattern is typically used by interfaces.
void *objMoveContext(THIS_VOID, MemContext *parentNew);

// Free the object mem context if this != NULL
void objFree(THIS_VOID);

// Free the object mem context if not NULL. The mem context to be freed must be the first member of the object struct. This pattern
// is typically used by interfaces.
void objFreeContext(THIS_VOID);

#endif
