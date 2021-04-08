/***********************************************************************************************************************************
Object Helper Macros and Functions

These macros and functions implement common object functionality.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_OBJECT_H
#define COMMON_TYPE_OBJECT_H

#include "common/memContext.h"

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
Functions

To ensure proper type checking, these functions are meant to be called from inline functions created specifically for each object:

__attribute__((always_inline)) static inline void
storageFree(Storage *this)
{
    objFree(this);
}
***********************************************************************************************************************************/
// Move an object to a new context if this != NULL. The mem context to move must be the first member of the object struct.
void *objMove(THIS_VOID, MemContext *parentNew);

// Free the object mem context if this != NULL. The mem context to be freed must be the first member of the object struct.
void objFree(THIS_VOID);

#endif
