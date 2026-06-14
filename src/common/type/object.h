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

This is a thin wrapper on MEM_CONTEXT_NEW_*() to do object specific initialization. The general pattern for a new object is:

OBJ_NEW_BEGIN(MyObj)
{
    *this = (MyObj)
    {
        .data = ...
    };
}
OBJ_NEW_END();

The this variable is variable is automatically created with the specified type. The BASE and EXTRA variants are available when a
custom variable must be used or different amount of memory must be allocated.
***********************************************************************************************************************************/
#define OBJ_NEW_BASE_EXTRA_BEGIN(type, extra, ...)                                                                                 \
    MEM_CONTEXT_NEW_BEGIN(type, .allocExtra = extra, __VA_ARGS__)

#define OBJ_NEW_EXTRA_BEGIN(type, extra, ...)                                                                                      \
    memContextSwitch(memContextNewP(STRINGIFY(type), .allocExtra = extra, __VA_ARGS__));                                           \
    type *const this = OBJ_NEW_ALLOC();                                                                                            \
                                                                                                                                   \
    do                                                                                                                             \
    {

#define OBJ_NEW_BASE_BEGIN(type, ...)                                                                                              \
    OBJ_NEW_BASE_EXTRA_BEGIN(type, sizeof(type), __VA_ARGS__)

#define OBJ_NEW_BEGIN(type, ...)                                                                                                   \
    OBJ_NEW_EXTRA_BEGIN(type, sizeof(type), __VA_ARGS__)

#define OBJ_NEW_ALLOC()                                                                                                            \
    memContextAllocExtra(memContextCurrent())

#define OBJ_NEW_END()                                                                                                              \
    MEM_CONTEXT_NEW_END()

/***********************************************************************************************************************************
Rename an object for auditing purposes. The original name for an object is based on the type used to create the object but a
different name might be needed to identify it during auditing. For example, a filter named IoSink would need to be renamed to
IoFilter to identify it as a filter for auditing.

This only has an effect in test builds.
***********************************************************************************************************************************/
#define OBJ_NAME(this, name)                                        MEM_CONTEXT_AUDIT_ALLOC_EXTRA_NAME(this, name)

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
Cast this private struct, e.g. List, to the associated public struct, e.g. ListPub. Note that the public struct must be the first
member of the private struct. For example:

FN_INLINE_ALWAYS unsigned int
lstSize(const List *const this)
{
    return THIS_PUB(List)->listSize;
}

The macro also ensures that this != NULL so there is no need to do that in the calling function.
***********************************************************************************************************************************/
#define THIS_PUB(type)                                              ((const type##Pub *)thisNotNull(this))

FN_INLINE_ALWAYS const void *
thisNotNull(const void *const this)
{
    ASSERT_INLINE(this != NULL);
    return this;
}

/***********************************************************************************************************************************
Switch to the object memory context and ensure that the prior memory context is restored after the block executes (even on error)
***********************************************************************************************************************************/
#define MEM_CONTEXT_OBJ_BEGIN(this)                         MEM_CONTEXT_BEGIN(objMemContext(this))
#define MEM_CONTEXT_OBJ_END()                               MEM_CONTEXT_END()

/***********************************************************************************************************************************
Functions

To ensure proper type checking, these functions are meant to be called from inline functions created specifically for each object:

FN_INLINE_ALWAYS void
storageFree(Storage *this)
{
    objFree(this);
}
***********************************************************************************************************************************/
// Get the object mem context
FN_INLINE_ALWAYS MemContext *
objMemContext(void *const this)
{
    return memContextFromAllocExtra(this);
}

// Move an object to a new context if this != NULL
FN_EXTERN void *objMove(THIS_VOID, MemContext *parentNew);

// Move driver object into an interface object when required. This is the general case, but sometimes an object will expose its
// interfaces in a different way, e.g. methods, so the object retains ownership of the interfaces. If this function is called in the
// driver object context then the driver object will retain ownership of the interface. Otherwise, the interface object will become
// the owner of the driver object.
FN_EXTERN void *objMoveToInterface(THIS_VOID, void *interfaceVoid, const MemContext *current);

// Free the object mem context if this != NULL
FN_EXTERN void objFree(THIS_VOID);

#endif
