/***********************************************************************************************************************************
Static String Handler

Format a statically-size char buffer with automatic truncation and size tracking. These are convenience methods on top of the
built-in format and memcpy functions to make the interface simpler.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRINGSTATIC_H
#define COMMON_TYPE_STRINGSTATIC_H

#include <stddef.h>

/***********************************************************************************************************************************
Type
***********************************************************************************************************************************/
typedef struct StringStatic
{
    char *buffer;
    size_t bufferSize;
    size_t resultSize;
} StringStatic;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
// Init static string
FN_INLINE_ALWAYS StringStatic
strStcInit(char *const buffer, const size_t bufferSize)
{
    return (StringStatic){.buffer = buffer, .bufferSize = bufferSize};
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Cat formatted string to static string
FN_EXTERN FN_PRINTF(2, 3) StringStatic *strStcFmt(StringStatic *debugLog, const char *format, ...);

// Cat zero-terminated string to static string
FN_EXTERN void strStcCat(StringStatic *debugLog, const char *cat);

// Cat character to static string
FN_EXTERN void strStcCatChr(StringStatic *debugLog, char cat);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Pointer to remaining buffer
FN_INLINE_ALWAYS char *
strStcRemains(const StringStatic *const debugLog)
{
    return debugLog->buffer + debugLog->resultSize;
}

// Remaining buffer size
FN_INLINE_ALWAYS size_t
strStcRemainsSize(const StringStatic *const debugLog)
{
    return debugLog->bufferSize - debugLog->resultSize;
}

// Result size
FN_INLINE_ALWAYS size_t
strStcResultSize(const StringStatic *const debugLog)
{
    return debugLog->resultSize;
}

// Increment result size
FN_INLINE_ALWAYS void
strStcResultSizeInc(StringStatic *const debugLog, const size_t inc)
{
    debugLog->resultSize += inc;
}

#endif
