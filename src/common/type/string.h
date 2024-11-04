/***********************************************************************************************************************************
String Handler

Strings are lightweight objects in that they do not have their own memory context, instead they exist in the current context in
which they are instantiated. If a string is needed outside the current memory context, the memory context must be switched to the
old context and then back. Below is a simplified example:

    String *result = NULL;     <--- is created in the current memory context (referred to as "old context" below)
    MEM_CONTEXT_TEMP_BEGIN()   <--- begins a new temporary context
    {
        String *resultStr = strNewZN("myNewStr"); <--- creates a string in the temporary memory context

        MEM_CONTEXT_PRIOR_BEGIN() <--- switch to the old context so the duplication of the string is in that context
        {
            result = strDup(resultStr); <--- recreates a copy of the string in the old context where "result" was created
        }
        MEM_CONTEXT_PRIOR_END(); <--- switch back to the temporary context
    }
    MEM_CONTEXT_TEMP_END(); <-- frees everything created inside this temporary memory context - i.e resultStr
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_STRING_H
#define COMMON_TYPE_STRING_H

#include <stdint.h>

#include "common/debug.h"

/***********************************************************************************************************************************
Minimum number of extra bytes to allocate for strings that are growing or are likely to grow
***********************************************************************************************************************************/
#ifndef STRING_EXTRA_MIN
#define STRING_EXTRA_MIN                                            64
#endif

/***********************************************************************************************************************************
String object
***********************************************************************************************************************************/
typedef struct String String;

#include "common/assert.h"
#include "common/encode.h"
#include "common/type/buffer.h"
#include "common/type/stringZ.h"

/***********************************************************************************************************************************
Constructors

Only strings created with strNew() can use strCat*() functions. If a string needs to be concatenated then start with, .e.g.:

String *example = strCatZ(strNew(), "example");

This syntax signals that the string will be modified so the string is allocated separately from the object so it can be resized when
needed. Most strings never need to be modified and can be stored more efficiently by allocating their memory with the object.
***********************************************************************************************************************************/
// Create a new empty string for concatenation
FN_EXTERN String *strNew(void);

// Create a new fixed length string from a zero-terminated string
FN_EXTERN String *strNewZ(const char *const string);

// Create a new fixed length string from a zero-terminated string with a specific length. The string may or may not be
// zero-terminated but we'll use that nomenclature since we're not concerned about the end of the string.
FN_EXTERN String *strNewZN(const char *string, size_t size);

// Create a new fixed length string from a buffer. If the buffer has a NULL character this may not work as expected. All the data
// will be copied but only the data before the NULL character will be used as a string.
FN_EXTERN String *strNewBuf(const Buffer *buffer);

// Create a new fixed length string by converting the double value
FN_EXTERN String *strNewDbl(double value);

// Create a new fixed length string by converting a timestamp
typedef struct StrNewTimeParam
{
    VAR_PARAM_HEADER;
    bool utc;                                                       // Use UTC instead of local time?
} StrNewTimeParam;

#define strNewTimeP(format, timestamp, ...)                                                                                        \
    strNewTime(format, timestamp, (StrNewTimeParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN FN_STRFTIME(1) String *strNewTime(const char *format, time_t timestamp, StrNewTimeParam param);

// Create a new fixed length string encoded with the specified type (e.g. encodingBase64) from a buffer
FN_EXTERN String *strNewEncode(EncodingType type, const Buffer *buffer);

// Create a new fixed length string from a format string with parameters (i.e. sprintf)
FN_EXTERN FN_PRINTF(1, 2) String *strNewFmt(const char *format, ...);

// Create a new fixed length string from a string
FN_EXTERN String *strDup(const String *this);

/***********************************************************************************************************************************
Getters/setters
***********************************************************************************************************************************/
typedef struct StringPub
{
    uint64_t size : 32;                                             // Actual size of the string
    uint64_t extra : 32;                                            // Extra space allocated for expansion
    char *buffer;                                                   // String buffer
} StringPub;

// String size minus null-terminator, i.e. the same value that strlen() would return
FN_INLINE_ALWAYS size_t
strSize(const String *this)
{
    return THIS_PUB(String)->size;
}

// Pointer to zero-terminated string. strZNull() returns NULL when the String is NULL.
FN_INLINE_ALWAYS const char *
strZ(const String *this)
{
    return THIS_PUB(String)->buffer;
}

FN_EXTERN const char *strZNull(const String *this);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Return the file part of a string (i.e. everything after the last / or the entire string if there is no /). strBaseZ() does not
// make a copy of the string so it may be more appropriate for large loops where saving memory is important.
FN_EXTERN String *strBase(const String *this);
FN_EXTERN const char *strBaseZ(const String *this);

// Does the string begin with the specified string?
FN_EXTERN bool strBeginsWith(const String *this, const String *beginsWith);
FN_EXTERN bool strBeginsWithZ(const String *this, const char *beginsWith);

// Append a string or zero-terminated string
FN_EXTERN String *strCat(String *this, const String *cat);
FN_EXTERN String *strCatZ(String *this, const char *cat);

// Append a buffer
FN_EXTERN String *strCatBuf(String *this, const Buffer *buffer);

// Append a character
FN_EXTERN String *strCatChr(String *this, char cat);

// Append a string encoded with the specified type (e.g. encodingBase64) from a buffer
FN_EXTERN String *strCatEncode(String *this, EncodingType type, const Buffer *buffer);

// Append a timestamp
typedef struct StrCatTimeParam
{
    VAR_PARAM_HEADER;
    bool utc;                                                       // Use UTC instead of local time?
} StrCatTimeParam;

#define strCatTimeP(this, format, timestamp, ...)                                                                                  \
    strCatTime(this, format, timestamp, (StrCatTimeParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN FN_STRFTIME(2) String *strCatTime(String *this, const char *format, time_t timestamp, StrCatTimeParam param);

// Append a formatted string
FN_EXTERN FN_PRINTF(2, 3) String *strCatFmt(String *this, const char *format, ...);

// Append N characters from a zero-terminated string. Note that the string does not actually need to be zero-terminated as long as
// N is <= the end of the string being concatenated.
FN_EXTERN String *strCatZN(String *this, const char *cat, size_t size);

FN_INLINE_ALWAYS String *
strCatN(String *this, const String *const cat, const size_t size)
{
    ASSERT_INLINE(cat != NULL);
    return strCatZN(this, strZ(cat), size);
}

// Return the index to the location of the first occurrence of a character within a string, else -1
FN_EXTERN int strChr(const String *this, char chr);

// C-style string compare
FN_EXTERN int strCmp(const String *this, const String *compare);
FN_EXTERN int strCmpZ(const String *this, const char *compare);

// Is the string empty?
FN_EXTERN bool strEmpty(const String *this);

// Does the string end with the specified string?
FN_EXTERN bool strEndsWith(const String *this, const String *endsWith);
FN_EXTERN bool strEndsWithZ(const String *this, const char *endsWith);

// Are two strings equal?
FN_EXTERN bool strEq(const String *this, const String *compare);
FN_EXTERN bool strEqZ(const String *this, const char *compare);

// Upper-case the first letter
FN_EXTERN String *strFirstUpper(String *this);

// Lower-case the first letter
FN_EXTERN String *strFirstLower(String *this);

// Lower-case entire string
FN_EXTERN String *strLower(String *this);

// Return the path part of a string (i.e. everything before the last / or "" if there is no /)
FN_EXTERN String *strPath(const String *this);

// Combine with a base path to get an absolute path
FN_EXTERN String *strPathAbsolute(const String *this, const String *base);

// Replace a character with another character
FN_EXTERN String *strReplaceChr(String *this, char find, char replace);

// Format sizes (file, buffer, etc.) in human-readable form
FN_EXTERN String *strSizeFormat(const uint64_t fileSize);

// Return a substring given only the start position
FN_EXTERN String *strSub(const String *this, size_t start);

// Return a substring given the start position and size
FN_EXTERN String *strSubN(const String *this, size_t start, size_t size);

// Trim whitespace from the beginning and end of a string
FN_EXTERN String *strTrim(String *this);

// Truncate the end of a string from the index provided to the current end (e.g. 123KB pass index of K returns 123)
FN_EXTERN String *strTruncIdx(String *this, int idx);

// Truncate the string to zero size
FN_INLINE_ALWAYS String *
strTrunc(String *const this)
{
    return strTruncIdx(this, 0);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
strFree(String *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for constant strings

Frequently used constant strings can be declared with these macros at compile time rather than dynamically at run time.

Note that strings created in this way are declared as const so can't be modified or freed by the str*() methods. Casting to String *
will result in a segfault due to modifying read-only memory.

By convention all string constant identifiers are appended with _STR.
***********************************************************************************************************************************/
// This struct must be kept in sync with StringPub (except for const qualifiers)
typedef struct StringPubConst
{
    uint64_t size : 32;                                             // Actual size of the string
    uint64_t extra : 32;                                            // Extra space allocated for expansion
    const char *buffer;                                             // String buffer
} StringPubConst;

#define STR_SIZE(bufferParam, sizeParam)                                                                                           \
    ((const String *const)&(const StringPubConst){.buffer = (const char *const)(bufferParam), .size = (unsigned int)(sizeParam)})

// Create a String constant inline from any zero-terminated string
#define STR(buffer)                                                                                                                \
    STR_SIZE(buffer, strlen(buffer))

// Create a String constant inline from a #define or inline string constant
#define STRDEF(buffer)                                                                                                             \
    STR_SIZE(buffer, sizeof(buffer) - 1)

// Used to define String constants that will be externed using STRING_DECLARE(). Must be used in a .c file.
#define STRING_EXTERN(name, buffer)                                                                                                \
    VR_EXTERN_DEFINE const String *const name = STRDEF(buffer)

// Used to define String constants that will be local to the .c file. Must be used in a .c file.
#define STRING_STATIC(name, buffer)                                                                                                \
    static const String *const name = STRDEF(buffer)

// Used to declare externed String constants defined with STRING_EXTERN(). Must be used in a .h file.
#define STRING_DECLARE(name)                                                                                                       \
    VR_EXTERN_DECLARE const String *const name

/***********************************************************************************************************************************
Constant strings that are generally useful
***********************************************************************************************************************************/
STRING_DECLARE(CR_STR);
STRING_DECLARE(CRLF_STR);
STRING_DECLARE(DOT_STR);
STRING_DECLARE(DOTDOT_STR);
STRING_DECLARE(EMPTY_STR);
STRING_DECLARE(FALSE_STR);
STRING_DECLARE(FSLASH_STR);
STRING_DECLARE(LF_STR);
STRING_DECLARE(N_STR);
STRING_DECLARE(NULL_STR);
STRING_DECLARE(TRUE_STR);
STRING_DECLARE(Y_STR);
STRING_DECLARE(ZERO_STR);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void strToLog(const String *this, StringStatic *debugLog);

#define FUNCTION_LOG_STRING_TYPE                                                                                                   \
    String *
#define FUNCTION_LOG_STRING_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_OBJECT_FORMAT(value, strToLog, buffer, bufferSize)

#endif
