/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
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
strNewSzN(const char *string, size_t size)
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
Are two strings equal?
***********************************************************************************************************************************/
bool
strEq(const String *this1, const String *this2)
{
    bool result = false;

    if (this1->size == this2->size)
        result = strcmp(strPtr(this1), strPtr(this2)) == 0;

    return result;
}

/***********************************************************************************************************************************
Return string ptr
***********************************************************************************************************************************/
const char *
strPtr(const String *this)
{
    return (const char *)this->buffer;
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
