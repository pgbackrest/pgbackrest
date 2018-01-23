/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Contains information about the string
***********************************************************************************************************************************/
struct String
{
    MemContext *memContext;
    size_t size;
    char *buffer;
};

/***********************************************************************************************************************************
Create a new string from a zero-terminated string
***********************************************************************************************************************************/
String *
strNew(const char *string)
{
    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = strlen(string);

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    strcpy(this->buffer, string);

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Create a new string from a buffer

If the buffer has a NULL character this may not work as expected.  All the data will be copied but only the data before the NULL
character will be used as a string.
***********************************************************************************************************************************/
String *
strNewBuf(const Buffer *buffer)
{
    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = bufSize(buffer);

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    memcpy(this->buffer, (char *)bufPtr(buffer), this->size);
    this->buffer[this->size] = 0;

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Create a new string from a format string with parameters (i.e. sprintf)
***********************************************************************************************************************************/
String *
strNewFmt(const char *format, ...)
{
    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    this->size = vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    va_start(argumentList, format);
    vsnprintf(this->buffer, this->size + 1, format, argumentList);
    va_end(argumentList);

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Create a new string from a string with a specific length

The string may or may not be zero-terminated but we'll use that nomeclature since we're not concerned about the end of the string.
***********************************************************************************************************************************/
String *
strNewN(const char *string, size_t size)
{
    // Create object
    String *this = memNew(sizeof(String));
    this->memContext = memContextCurrent();
    this->size = size;

    // Allocate and assign string
    this->buffer = memNewRaw(this->size + 1);
    strncpy(this->buffer, string, this->size);
    this->buffer[this->size] = 0;

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
Return the file part of a string (i.e. everthing after the last / or the entire string if there is no /)
***********************************************************************************************************************************/
String *
strBase(const String *this)
{
    const char *end = this->buffer + this->size;

    while (end > this->buffer && *(end - 1) != '/')
        end--;

    return strNew(end);
}

/***********************************************************************************************************************************
Does the string begin with the specified string?
***********************************************************************************************************************************/
bool
strBeginsWith(const String *this, const String *beginsWith)
{
    return strBeginsWithZ(this, strPtr(beginsWith));
}

bool
strBeginsWithZ(const String *this, const char *beginsWith)
{
    bool result = false;
    unsigned int beginsWithSize = (unsigned int)strlen(beginsWith);

    if (this->size >= beginsWithSize)
        result = strncmp(strPtr(this), beginsWith, beginsWithSize) == 0;

    return result;
}

/***********************************************************************************************************************************
Append a string
***********************************************************************************************************************************/
String *
strCat(String *this, const char *cat)
{
    // Determine length of string to append
    int sizeGrow = strlen(cat);

    // Allocate and append string
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->buffer = memGrowRaw(this->buffer, this->size + sizeGrow + 1);
    }
    MEM_CONTEXT_END();

    strcpy(this->buffer + this->size, cat);
    this->size += sizeGrow;

    return this;
}

/***********************************************************************************************************************************
Append a formatted string
***********************************************************************************************************************************/
String *
strCatFmt(String *this, const char *format, ...)
{
    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    int sizeGrow = vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Allocate and append string
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->buffer = memGrowRaw(this->buffer, this->size + sizeGrow + 1);
    }
    MEM_CONTEXT_END();

    va_start(argumentList, format);
    vsnprintf(this->buffer + this->size, sizeGrow + 1, format, argumentList);
    va_end(argumentList);

    this->size += sizeGrow;

    // Return buffer
    return this;
}

/***********************************************************************************************************************************
C-style string compare
***********************************************************************************************************************************/
int
strCmp(const String *this, const String *compare)
{
    return strcmp(strPtr(this), strPtr(compare));
}

int
strCmpZ(const String *this, const char *compare)
{
    return strcmp(strPtr(this), compare);
}

/***********************************************************************************************************************************
Duplicate a string from an existing string
***********************************************************************************************************************************/
String *
strDup(const String *this)
{
    String *result = NULL;

    if (this != NULL)
        result = strNew(strPtr(this));

    return result;
}

/***********************************************************************************************************************************
Does the string end with the specified string?
***********************************************************************************************************************************/
bool
strEndsWith(const String *this, const String *endsWith)
{
    return strEndsWithZ(this, strPtr(endsWith));
}

bool
strEndsWithZ(const String *this, const char *endsWith)
{
    bool result = false;
    unsigned int endsWithSize = (unsigned int)strlen(endsWith);

    if (this->size >= endsWithSize)
        result = strcmp(strPtr(this) + (this->size - endsWithSize), endsWith) == 0;

    return result;
}

/***********************************************************************************************************************************
Are two strings equal?

There are two separate implementations because string objects can get the size very efficiently whereas the zero-terminated strings
would need a call to strlen().
***********************************************************************************************************************************/
bool
strEq(const String *this, const String *compare)
{
    bool result = false;

    if (this->size == compare->size)
        result = strcmp(strPtr(this), strPtr(compare)) == 0;

    return result;
}

bool
strEqZ(const String *this, const char *compare)
{
    return strcmp(strPtr(this), compare) == 0;
}

/***********************************************************************************************************************************
Upper-case the first letter
***********************************************************************************************************************************/
String *
strFirstUpper(String *this)
{
    if (this->size > 0)
        this->buffer[0] = toupper(this->buffer[0]);

    return this;
}

/***********************************************************************************************************************************
Lower-case the first letter
***********************************************************************************************************************************/
String *
strFirstLower(String *this)
{
    if (this->size > 0)
        this->buffer[0] = tolower(this->buffer[0]);

    return this;
}

/***********************************************************************************************************************************
Return string ptr
***********************************************************************************************************************************/
const char *
strPtr(const String *this)
{
    const char *result = NULL;

    if (this != NULL)
        result = (const char *)this->buffer;

    return result;
}

/***********************************************************************************************************************************
Return string size
***********************************************************************************************************************************/
size_t
strSize(const String *this)
{
    return this->size;
}

/***********************************************************************************************************************************
Return string size
***********************************************************************************************************************************/
String *
strTrim(String *this)
{
    // Nothing to trim if size is zero
    if (this->size > 0)
    {
        // Find the beginning of the string skipping all whitespace
        char *begin = this->buffer;

        while (*begin != 0 && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n'))
            begin++;

        // Find the end of the string skipping all whitespace
        char *end = this->buffer + (this->size - 1);

        while (end > begin && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
            end--;

        // Is there anything to trim?
        size_t newSize = (size_t)(end - begin + 1);

        if (begin != this->buffer || newSize < strSize(this))
        {
            // Calculate new size
            this->size = newSize;

            // Move the substr to the beginning of the buffer
            memmove(this->buffer, begin, this->size);
            this->buffer[this->size] = 0;

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // Resize the buffer
                this->buffer = memGrowRaw(this->buffer, this->size + 1);
            }
            MEM_CONTEXT_END();
        }
    }

    return this;
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
strFree(String *this)
{
    if (this != NULL)
    {
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            memFree(this->buffer);
            memFree(this);
        }
        MEM_CONTEXT_END();
    }
}
