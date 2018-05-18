/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#include <stdio.h>

#include "common/debug.h"

/***********************************************************************************************************************************
Convert object to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
objToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    if (object == NULL)
        result = (size_t)snprintf(buffer, bufferSize, "null");
    else
        result = (size_t)snprintf(buffer, bufferSize, "{%s}", objectName);

    return result;
}

/***********************************************************************************************************************************
Convert pointer to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    if (pointer == NULL)
        result = (size_t)snprintf(buffer, bufferSize, "null");
    else
        result = (size_t)snprintf(buffer, bufferSize, "(%s)", pointerName);

    return result;
}

/***********************************************************************************************************************************
Convert zero-terminated string for logging
***********************************************************************************************************************************/
size_t
strzToLog(const char *string, char *buffer, size_t bufferSize)
{
    return (size_t)snprintf(buffer, bufferSize, string == NULL ? "%s" : "\"%s\"", string == NULL ? "null" : string);
}
