/***********************************************************************************************************************************
Object Helper Macros

Macros to automate definitions of various boilerplate functions and log macros.

Each object should have at least two macros defined it its header file, <OBJECT_NAME>_TYPE and <OBJECT_NAME>_PREFIX.  So if the
object type is "Object" the macros would be:

#define OBJECT_TYPE                                                 Object
#define OBJECT_PREFIX                                               object

In most cases _PREFIX will be identical to _TYPE except that the first letter is lower-cased.  For commonly used objects (e.g.
String) a shorter prefix may be used.

When a macro exists to create a function definition in a C file there is no equivalent macro to create the prototype in the header.
The prototype is not repetitious enough to justify a macro and it would only serve to obfuscate the header file.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_OBJECT_H
#define COMMON_TYPE_OBJECT_H

#include "common/macro.h"

/***********************************************************************************************************************************
Used in interface function parameter lists to discourage use of the untyped thisVoid parameter, e.g.:

size_t bufferRead(THIS_VOID, Buffer *buffer)

This macro should not be used unless the function is assigned to an interface.
***********************************************************************************************************************************/
#define THIS_VOID                                                   void *thisVoid

/***********************************************************************************************************************************
Create a local "this" variable of the correct type from a THIS_VOID parameter
***********************************************************************************************************************************/
#define THIS(type)                                                  type *this = thisVoid

/***********************************************************************************************************************************
Define a function used by the caller to move an object from one context to another

The object type is expected to have a memmber named "memContext" and the object must allocate *all* memory in that context.

If "this" is NULL then no action is taken.
***********************************************************************************************************************************/
#define OBJECT_DEFINE_MOVE(objectMacro)                                                                                            \
    objectMacro##_TYPE *                                                                                                           \
    GLUE(objectMacro##_PREFIX, Move)(objectMacro##_TYPE *this, MemContext *parentNew)                                              \
    {                                                                                                                              \
        FUNCTION_TEST_BEGIN();                                                                                                     \
            FUNCTION_TEST_PARAM(objectMacro, this);                                                                                \
            FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);                                                                           \
        FUNCTION_TEST_END();                                                                                                       \
                                                                                                                                   \
        ASSERT(parentNew != NULL);                                                                                                 \
                                                                                                                                   \
        if (this != NULL)                                                                                                          \
            memContextMove(this->memContext, parentNew);                                                                           \
                                                                                                                                   \
        FUNCTION_TEST_RETURN(this);                                                                                                \
    }

/***********************************************************************************************************************************
Free resource associated with an object that was not allocated by a mem context

Create a callback function intended to be use with memContextCallbackSet() that frees a resource that was allocated by, e.g., a
third-party library and not by a mem context.  Don't call memFree() or memContextFree() in this function -- that will be handled
when the mem context is freed.

If the object prefix is "object" then the function will be defined as:

static void objectFreeResource(THIS_VOID)
***********************************************************************************************************************************/
#define OBJECT_DEFINE_FREE_RESOURCE_BEGIN(objectMacro, logTypeMacro, logLevelMacro)                                                \
    static void                                                                                                                    \
    GLUE(objectMacro##_PREFIX, FreeResource)(THIS_VOID)                                                                            \
    {                                                                                                                              \
        THIS(objectMacro##_TYPE);                                                                                                  \
                                                                                                                                   \
        FUNCTION_##logTypeMacro##_BEGIN(logLevelMacro);                                                                            \
            FUNCTION_##logTypeMacro##_PARAM(objectMacro, this);                                                                    \
        FUNCTION_##logTypeMacro##_END();                                                                                           \
                                                                                                                                   \
        ASSERT(this != NULL);

#define OBJECT_DEFINE_FREE_RESOURCE_END(logTypeMacro)                                                                              \
        FUNCTION_##logTypeMacro##_RETURN_VOID();                                                                                   \
    }

/***********************************************************************************************************************************
Define a function used by the caller to dispose of an object that is no longer needed when it would consume significant amounts of
memory, e.g. in a loop.  For the most part free does not need to be called explicitly, and in fact should not be since the automatic
cleanup is much more efficient.

If the object type/prefix is "Object"/"object" then the function will be defined as:

static void objectFree(Object *this)

Note that this function is externed as there no need for a static free function since the context will be cleaned up automatically
by the parent context.
***********************************************************************************************************************************/
#define OBJECT_DEFINE_FREE(objectMacro)                                                                                            \
    void                                                                                                                           \
    GLUE(objectMacro##_PREFIX, Free)(objectMacro##_TYPE *this)                                                                     \
    {                                                                                                                              \
        FUNCTION_TEST_BEGIN();                                                                                                     \
            FUNCTION_TEST_PARAM(objectMacro, this);                                                                                \
        FUNCTION_TEST_END();                                                                                                       \
                                                                                                                                   \
        if (this != NULL)                                                                                                          \
            memContextFree(this->memContext);                                                                                      \
                                                                                                                                   \
        FUNCTION_TEST_RETURN_VOID();                                                                                               \
    }

#endif
