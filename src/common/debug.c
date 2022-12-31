/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"

/**********************************************************************************************************************************/
size_t
objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    if (object == NULL)
        result = (size_t)snprintf(buffer, bufferSize, NULL_Z);
    else
        result = (size_t)snprintf(buffer, bufferSize, "{%s}", objectName);

    return result;
}

/**********************************************************************************************************************************/
size_t
ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    if (pointer == NULL)
        result = (size_t)snprintf(buffer, bufferSize, NULL_Z);
    else
        result = (size_t)snprintf(buffer, bufferSize, "(%s)", pointerName);

    return result;
}

/**********************************************************************************************************************************/
size_t
typeToLog(const char *typeName, char *buffer, size_t bufferSize)
{
    return (size_t)snprintf(buffer, bufferSize, "%s", typeName);
}
