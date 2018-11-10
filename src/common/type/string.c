/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constant strings that are generally useful
***********************************************************************************************************************************/
STRING_EXTERN(FSLASH_STR,                                           "/");
STRING_EXTERN(N_STR,                                                "n");
STRING_EXTERN(NULL_STR,                                             "null");
STRING_EXTERN(Y_STR,                                                "y");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct String
{
    // This struct must always be declared first.  These are the fields that are shared in common with constant strings.
    struct StringCommon common;

    MemContext *memContext;                                         // Required for dynamically allocated strings
};

/***********************************************************************************************************************************
Create a new string from a zero-terminated string
***********************************************************************************************************************************/
String *
strNew(const char *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);

        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->common.size = strlen(string);

    // Allocate and assign string
    this->common.buffer = memNewRaw(this->common.size + 1);
    strcpy(this->common.buffer, string);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Create a new string from a buffer

If the buffer has a NULL character this may not work as expected.  All the data will be copied but only the data before the NULL
character will be used as a string.
***********************************************************************************************************************************/
String *
strNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_TEST_END();

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->common.size = bufUsed(buffer);

    // Allocate and assign string
    this->common.buffer = memNewRaw(this->common.size + 1);
    memcpy(this->common.buffer, (char *)bufPtr(buffer), this->common.size);
    this->common.buffer[this->common.size] = 0;

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Create a new string from a format string with parameters (i.e. sprintf)
***********************************************************************************************************************************/
String *
strNewFmt(const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);

        FUNCTION_TEST_ASSERT(format != NULL);
    FUNCTION_TEST_END();

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    this->common.size = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Allocate and assign string
    this->common.buffer = memNewRaw(this->common.size + 1);
    va_start(argumentList, format);
    vsnprintf(this->common.buffer, this->common.size + 1, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Create a new string from a string with a specific length

The string may or may not be zero-terminated but we'll use that nomeclature since we're not concerned about the end of the string.
***********************************************************************************************************************************/
String *
strNewN(const char *string, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(CHARP, string);
        FUNCTION_TEST_PARAM(SIZE, size);

        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->common.size = size;

    // Allocate and assign string
    this->common.buffer = memNewRaw(this->common.size + 1);
    strncpy(this->common.buffer, string, this->common.size);
    this->common.buffer[this->common.size] = 0;

    // Return buffer
    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Return the file part of a string (i.e. everything after the last / or the entire string if there is no /)
***********************************************************************************************************************************/
String *
strBase(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    const char *end = this->common.buffer + this->common.size;

    while (end > this->common.buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RESULT(STRING, strNew(end));
}

/***********************************************************************************************************************************
Does the string begin with the specified string?
***********************************************************************************************************************************/
bool
strBeginsWith(const String *this, const String *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, beginsWith);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(beginsWith != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, strBeginsWithZ(this, strPtr(beginsWith)));
}

bool
strBeginsWithZ(const String *this, const char *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, beginsWith);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(beginsWith != NULL);
    FUNCTION_TEST_END();

    bool result = false;
    unsigned int beginsWithSize = (unsigned int)strlen(beginsWith);

    if (this->common.size >= beginsWithSize)
        result = strncmp(strPtr(this), beginsWith, beginsWithSize) == 0;

    FUNCTION_TEST_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Append a string
***********************************************************************************************************************************/
String *
strCat(String *this, const char *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, cat);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(cat != NULL);
    FUNCTION_TEST_END();

    // Determine length of string to append
    size_t sizeGrow = strlen(cat);

    // Allocate and append string
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->common.buffer = memGrowRaw(this->common.buffer, this->common.size + sizeGrow + 1);
    }
    MEM_CONTEXT_END();

    strcpy(this->common.buffer + this->common.size, cat);
    this->common.size += sizeGrow;

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Append a character
***********************************************************************************************************************************/
String *
strCatChr(String *this, char cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, cat);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(cat != 0);
    FUNCTION_TEST_END();

    // Allocate and append character
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->common.buffer = memGrowRaw(this->common.buffer, this->common.size + 2);
    }
    MEM_CONTEXT_END();

    this->common.buffer[this->common.size++] = cat;
    this->common.buffer[this->common.size] = 0;

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Append a formatted string
***********************************************************************************************************************************/
String *
strCatFmt(String *this, const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, format);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(format != NULL);
    FUNCTION_TEST_END();

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    size_t sizeGrow = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Allocate and append string
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->common.buffer = memGrowRaw(this->common.buffer, this->common.size + sizeGrow + 1);
    }
    MEM_CONTEXT_END();

    va_start(argumentList, format);
    vsnprintf(this->common.buffer + this->common.size, sizeGrow + 1, format, argumentList);
    va_end(argumentList);

    this->common.size += sizeGrow;

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
C-style string compare
***********************************************************************************************************************************/
int
strCmp(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(compare != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INT, strcmp(strPtr(this), strPtr(compare)));
}

int
strCmpZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(compare != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(INT, strcmp(strPtr(this), compare));
}

/***********************************************************************************************************************************
Duplicate a string from an existing string
***********************************************************************************************************************************/
String *
strDup(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
        result = strNew(strPtr(this));

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Does the string end with the specified string?
***********************************************************************************************************************************/
bool
strEndsWith(const String *this, const String *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, endsWith);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(endsWith != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, strEndsWithZ(this, strPtr(endsWith)));
}

bool
strEndsWithZ(const String *this, const char *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, endsWith);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(endsWith != NULL);
    FUNCTION_TEST_END();

    bool result = false;
    unsigned int endsWithSize = (unsigned int)strlen(endsWith);

    if (this->common.size >= endsWithSize)
        result = strcmp(strPtr(this) + (this->common.size - endsWithSize), endsWith) == 0;

    FUNCTION_TEST_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Are two strings equal?

There are two separate implementations because string objects can get the size very efficiently whereas the zero-terminated strings
would need a call to strlen().
***********************************************************************************************************************************/
bool
strEq(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(compare != NULL);
    FUNCTION_TEST_END();

    bool result = false;

    if (this->common.size == compare->common.size)
        result = strcmp(strPtr(this), strPtr(compare)) == 0;

    FUNCTION_TEST_RESULT(BOOL, result);
}

bool
strEqZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(compare != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, strcmp(strPtr(this), compare) == 0);
}

/***********************************************************************************************************************************
Upper-case the first letter
***********************************************************************************************************************************/
String *
strFirstUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (this->common.size > 0)
        this->common.buffer[0] = (char)toupper(this->common.buffer[0]);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Lower-case the first letter
***********************************************************************************************************************************/
String *
strFirstLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (this->common.size > 0)
        this->common.buffer[0] = (char)tolower(this->common.buffer[0]);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Upper-case entire string
***********************************************************************************************************************************/
String *
strUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (this->common.size > 0)
        for (unsigned int idx = 0; idx <= this->common.size; idx++)
            this->common.buffer[idx] = (char)toupper(this->common.buffer[idx]);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Upper-case entire string
***********************************************************************************************************************************/
String *
strLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    if (this->common.size > 0)
        for (unsigned int idx = 0; idx <= this->common.size; idx++)
            this->common.buffer[idx] = (char)tolower(this->common.buffer[idx]);

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Return the path part of a string (i.e. everything before the last / or "" if there is no /)
***********************************************************************************************************************************/
String *
strPath(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    const char *end = this->common.buffer + this->common.size;

    while (end > this->common.buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RESULT(
        STRING,
        strNewN(
            this->common.buffer,
            end - this->common.buffer <= 1 ? (size_t)(end - this->common.buffer) : (size_t)(end - this->common.buffer - 1)));
}

/***********************************************************************************************************************************
Return string ptr
***********************************************************************************************************************************/
const char *
strPtr(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    char *result = NULL;

    if (this != NULL)
        result = this->common.buffer;

    FUNCTION_TEST_RESULT(CHARP, result);
}

/***********************************************************************************************************************************
Quote a string
***********************************************************************************************************************************/
String *
strQuote(const String *this, const String *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, quote);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(quote != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, strQuoteZ(this, strPtr(quote)));
}

String *
strQuoteZ(const String *this, const char *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, quote);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(quote != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, strNewFmt("%s%s%s", quote, strPtr(this), quote));
}

/***********************************************************************************************************************************
Replace a character with another character
***********************************************************************************************************************************/
String *
strReplaceChr(String *this, char find, char replace)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, find);
        FUNCTION_TEST_PARAM(CHAR, replace);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    for (unsigned int stringIdx = 0; stringIdx < this->common.size; stringIdx++)
    {
        if (this->common.buffer[stringIdx] == find)
            this->common.buffer[stringIdx] = replace;
    }

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Return a substring given only the start position
***********************************************************************************************************************************/
String *
strSub(const String *this, size_t start)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(start < this->common.size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, strSubN(this, start, this->common.size - start));
}

/***********************************************************************************************************************************
Return a substring given the start position and size
***********************************************************************************************************************************/
String *
strSubN(const String *this, size_t start, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);
        FUNCTION_TEST_PARAM(SIZE, size);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(start < this->common.size);
        FUNCTION_TEST_ASSERT(start + size <= this->common.size);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STRING, strNewN(this->common.buffer + start, size));
}

/***********************************************************************************************************************************
Return string size
***********************************************************************************************************************************/
size_t
strSize(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->common.size);
}

/***********************************************************************************************************************************
Trim whitespace from the beginnning and end of a string
***********************************************************************************************************************************/
String *
strTrim(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    // Nothing to trim if size is zero
    if (this->common.size > 0)
    {
        // Find the beginning of the string skipping all whitespace
        char *begin = this->common.buffer;

        while (*begin != 0 && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n'))
            begin++;

        // Find the end of the string skipping all whitespace
        char *end = this->common.buffer + (this->common.size - 1);

        while (end > begin && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
            end--;

        // Is there anything to trim?
        size_t newSize = (size_t)(end - begin + 1);

        if (begin != this->common.buffer || newSize < strSize(this))
        {
            // Calculate new size
            this->common.size = newSize;

            // Move the substr to the beginning of the buffer
            memmove(this->common.buffer, begin, this->common.size);
            this->common.buffer[this->common.size] = 0;

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // Resize the buffer
                this->common.buffer = memGrowRaw(this->common.buffer, this->common.size + 1);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Return the index to the location of the the first occurrence of a character within a string, else -1
***********************************************************************************************************************************/
int
strChr(const String *this, char chr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, chr);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    int result = -1;

    if (this->common.size > 0)
    {
        const char *ptr = strchr(this->common.buffer, chr);

        if (ptr != NULL)
            result = (int)(ptr - this->common.buffer);
    }

    FUNCTION_TEST_RESULT(INT, result);
}

/***********************************************************************************************************************************
Truncate the end of a string from the index provided to the current end (e.g. 123KB pass index of K returns 123)
***********************************************************************************************************************************/
String *
strTrunc(String *this, int idx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(idx >= 0 && (size_t)idx <= this->common.size);
    FUNCTION_TEST_END();

    if (this->common.size > 0)
    {
        // Reset the size to end at the index
        this->common.size = (size_t)(idx);
        this->common.buffer[this->common.size] = 0;

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // Resize the buffer
            this->common.buffer = memGrowRaw(this->common.buffer, this->common.size + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT(STRING, this);
}

/***********************************************************************************************************************************
Convert an object to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (object == NULL)
            string = strNew("null");
        else
            string = formatFunc(object);

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
strToLog(const String *this)
{
    return strNewFmt("{\"%s\"}", strPtr(this));
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
strFree(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            memFree(this->common.buffer);
            memFree(this);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RESULT_VOID();
}
