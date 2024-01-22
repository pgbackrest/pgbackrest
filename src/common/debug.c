/***********************************************************************************************************************************
Debug Routines
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>

#include "common/debug.h"

/**********************************************************************************************************************************/
FN_EXTERN size_t
objToLog(const void *const object, ObjToLogFormat formatFunc, char *const buffer, const size_t bufferSize)
{
    StringStatic debugLog = strStcInit(buffer, bufferSize);

    if (object == NULL)
        strStcCat(&debugLog, NULL_Z);
    else
        formatFunc(object, &debugLog);

    return strStcResultSize(&debugLog);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
objNameToLog(const void *object, const char *objectName, char *buffer, size_t bufferSize)
{
    StringStatic debugLog = strStcInit(buffer, bufferSize);

    if (object == NULL)
        strStcCat(&debugLog, NULL_Z);
    else
        strStcFmt(&debugLog, "{%s}", objectName);

    return strStcResultSize(&debugLog);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
ptrToLog(const void *pointer, const char *pointerName, char *buffer, size_t bufferSize)
{
    StringStatic debugLog = strStcInit(buffer, bufferSize);

    if (pointer == NULL)
        strStcCat(&debugLog, NULL_Z);
    else
        strStcFmt(&debugLog, "(%s)", pointerName);

    return strStcResultSize(&debugLog);
}

/**********************************************************************************************************************************/
FN_EXTERN size_t
typeToLog(const char *typeName, char *buffer, size_t bufferSize)
{
    StringStatic debugLog = strStcInit(buffer, bufferSize);

    strStcFmt(&debugLog, "%s", typeName);

    return strStcResultSize(&debugLog);
}
