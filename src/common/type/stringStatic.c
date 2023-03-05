/***********************************************************************************************************************************
Static String Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/type/stringStatic.h"

/**********************************************************************************************************************************/
FN_EXTERN StringStatic *
strStcFmt(StringStatic *const debugLog, const char *const format, ...)
{
    // Proceed if there is space for at least one character
    if (strStcRemainsSize(debugLog) > 1)
    {
        va_list argument;
        va_start(argument, format);
        size_t result = (size_t)vsnprintf(strStcRemains(debugLog), strStcRemainsSize(debugLog), format, argument);
        va_end(argument);

        if (result >= strStcRemainsSize(debugLog))
            debugLog->resultSize = debugLog->bufferSize - 1;
        else
            debugLog->resultSize += result;
    }

    return debugLog;
}

/**********************************************************************************************************************************/
FN_EXTERN void
strStcCat(StringStatic *const debugLog, const char *const cat)
{
    // Proceed if there is space for at least one character
    const size_t remainsSize = strStcRemainsSize(debugLog);

    if (remainsSize > 1)
    {
        const size_t catSize = strlen(cat);
        const size_t resultSize = catSize > remainsSize - 1 ? remainsSize - 1 : catSize;

        memcpy(strStcRemains(debugLog), cat, resultSize);
        debugLog->resultSize += resultSize;

        debugLog->buffer[debugLog->resultSize] = '\0';
    }
}

/**********************************************************************************************************************************/
FN_EXTERN void
strStcCatChr(StringStatic *const debugLog, const char cat)
{
    if (strStcRemainsSize(debugLog) > 1)
    {
        debugLog->buffer[debugLog->resultSize] = cat;
        debugLog->buffer[++debugLog->resultSize] = '\0';
    }
}
