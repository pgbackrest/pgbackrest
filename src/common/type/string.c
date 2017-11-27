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
    size_t size;
    char *buffer;
};

/***********************************************************************************************************************************
Create a new string from another string
***********************************************************************************************************************************/
String *
strNew(const char *string)
{
    // Create object
    String *this = memNew(sizeof(String));
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
Append a string
***********************************************************************************************************************************/
String *
strCat(String *this, const char *cat)
{
    // Determine length of string to append
    int sizeGrow = strlen(cat);

    // Allocate and append string
    this->buffer = memGrowRaw(this->buffer, this->size + sizeGrow + 1);
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
    this->buffer = memGrowRaw(this->buffer, this->size + sizeGrow + 1);

    va_start(argumentList, format);
    vsnprintf(this->buffer + this->size, sizeGrow + 1, format, argumentList);
    va_end(argumentList);

    this->size += sizeGrow;

    // Return buffer
    return this;
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
Free the string
***********************************************************************************************************************************/
void
strFree(String *this)
{
    memFree(this->buffer);
    memFree(this);
}
