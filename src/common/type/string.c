/***********************************************************************************************************************************
String Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/debug.h"
#include "common/macro.h"
#include "common/memContext.h"
#include "common/type/string.h"
#include "common/type/stringList.h"
#include "common/type/stringz.h"

/***********************************************************************************************************************************
Constant strings that are generally useful
***********************************************************************************************************************************/
STRING_EXTERN(BRACKETL_STR,                                         BRACKETL_Z);
STRING_EXTERN(BRACKETR_STR,                                         BRACKETR_Z);
STRING_EXTERN(COLON_STR,                                            COLON_Z);
STRING_EXTERN(CR_STR,                                               CR_Z);
STRING_EXTERN(CRLF_STR,                                             CRLF_Z);
STRING_EXTERN(DASH_STR,                                             DASH_Z);
STRING_EXTERN(DOT_STR,                                              DOT_Z);
STRING_EXTERN(DOTDOT_STR,                                           DOTDOT_Z);
STRING_EXTERN(EMPTY_STR,                                            EMPTY_Z);
STRING_EXTERN(EQ_STR,                                               EQ_Z);
STRING_EXTERN(FALSE_STR,                                            FALSE_Z);
STRING_EXTERN(FSLASH_STR,                                           FSLASH_Z);
STRING_EXTERN(LF_STR,                                               LF_Z);
STRING_EXTERN(N_STR,                                                N_Z);
STRING_EXTERN(NULL_STR,                                             NULL_Z);
STRING_EXTERN(QUOTED_STR,                                           QUOTED_Z);
STRING_EXTERN(TRUE_STR,                                             TRUE_Z);
STRING_EXTERN(Y_STR,                                                Y_Z);
STRING_EXTERN(ZERO_STR,                                             ZERO_Z);

/***********************************************************************************************************************************
Maximum size of a string
***********************************************************************************************************************************/
#define STRING_SIZE_MAX                                            1073741824

#define CHECK_SIZE(size)                                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        if ((size) > STRING_SIZE_MAX)                                                                                              \
            THROW(AssertError, "string size must be <= " STRINGIFY(STRING_SIZE_MAX) " bytes");                                     \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct String
{
    StringPub pub;                                                  // Publicly accessible variables
    MemContext *memContext;                                         // Required for dynamically allocated strings
};

/**********************************************************************************************************************************/
String *
strNew(void)
{
    FUNCTION_TEST_VOID();

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .pub =
        {
            // A zero-length string is not very useful so assume this string is being created for appending and allocate extra space
            .extra = STRING_EXTRA_MIN,
        },
        .memContext = memContextCurrent(),
    };

    // Allocate and assign string
    this->pub.buffer = memNew(STRING_EXTRA_MIN + 1);
    this->pub.buffer[0] = '\0';

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strNewZ(const char *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, string);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Check size
    size_t stringSize = strlen(string);
    CHECK_SIZE(stringSize);

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .pub =
        {
            .size = (unsigned int)stringSize,
        },
        .memContext = memContextCurrent(),
    };

    // Allocate and assign string
    this->pub.buffer = memNew(strSize(this) + this->pub.extra + 1);
    strcpy(this->pub.buffer, string);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *strNewDbl(double value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DOUBLE, value);
    FUNCTION_TEST_END();

    char working[CVT_BASE10_BUFFER_SIZE];

    cvtDoubleToZ(value, working, sizeof(working));

    FUNCTION_TEST_RETURN(strNewZ(working));
}

/**********************************************************************************************************************************/
String *
strNewBuf(const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Check size
    CHECK_SIZE(bufUsed(buffer));

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .pub =
        {
            .size = (unsigned int)bufUsed(buffer),
        },
        .memContext = memContextCurrent(),
    };

    // Allocate and assign string
    this->pub.buffer = memNew(strSize(this) + 1);
    memcpy(this->pub.buffer, bufPtrConst(buffer), strSize(this));
    this->pub.buffer[strSize(this)] = 0;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strNewEncode(EncodeType type, const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(buffer != NULL);

    // Check encoded size
    size_t size = encodeToStrSize(type, bufUsed(buffer));
    CHECK_SIZE(size);

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .pub =
        {
            .size = (unsigned int)size,
        },
        .memContext = memContextCurrent(),
    };

    // Allocate and encode buffer
    this->pub.buffer = memNew(strSize(this) + 1);
    encodeToStr(type, bufPtrConst(buffer), bufUsed(buffer), this->pub.buffer);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strNewFmt(const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(format != NULL);

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .memContext = memContextCurrent(),
    };

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    size_t formatSize = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Check size
    CHECK_SIZE(formatSize);

    // Allocate and assign string
    this->pub.size = (unsigned int)formatSize;
    this->pub.buffer = memNew(strSize(this) + 1);
    va_start(argumentList, format);
    vsnprintf(this->pub.buffer, strSize(this) + 1, format, argumentList);
    va_end(argumentList);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strNewN(const char *string, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(CHARDATA, string);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(string != NULL);

    // Check size
    CHECK_SIZE(size);

    // Create object
    String *this = memNew(sizeof(String));

    *this = (String)
    {
        .pub =
        {
            .size = (unsigned int)size,
        },
        .memContext = memContextCurrent(),
    };

    // Allocate and assign string
    this->pub.buffer = memNew(strSize(this) + 1);
    strncpy(this->pub.buffer, string, strSize(this));
    this->pub.buffer[strSize(this)] = 0;

    // Return buffer
    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strBase(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(strNewZ(strBaseZ(this)));
}

const char *
strBaseZ(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char *end = this->pub.buffer + strSize(this);

    while (end > this->pub.buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RETURN(end);
}

/**********************************************************************************************************************************/
bool
strBeginsWith(const String *this, const String *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, beginsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(beginsWith != NULL);

    FUNCTION_TEST_RETURN(strBeginsWithZ(this, strZ(beginsWith)));
}

bool
strBeginsWithZ(const String *this, const char *beginsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, beginsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(beginsWith != NULL);

    bool result = false;
    unsigned int beginsWithSize = (unsigned int)strlen(beginsWith);

    if (strSize(this) >= beginsWithSize)
        result = strncmp(strZ(this), beginsWith, beginsWithSize) == 0;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Resize the string to allow the requested number of characters to be appended
***********************************************************************************************************************************/
static void
strResize(String *this, size_t requested)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, requested);
    FUNCTION_TEST_END();

    if (requested > this->pub.extra)
    {
        // Check size
        CHECK_SIZE(strSize(this) + requested);

        // Calculate new extra needs to satisfy request and leave extra space for new growth
        this->pub.extra = (unsigned int)(requested + ((strSize(this) + requested) / 2));

        // Adding too little extra space usually leads to immediate resizing so enforce a minimum
        if (this->pub.extra < STRING_EXTRA_MIN)
            this->pub.extra = STRING_EXTRA_MIN;

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->pub.buffer = memResize(this->pub.buffer, strSize(this) + this->pub.extra + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
strCat(String *this, const String *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != NULL);

    FUNCTION_TEST_RETURN(strCatZN(this, strZ(cat), strSize(cat)));
}

String *
strCatZ(String *this, const char *cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != NULL);

    // Determine length of string to append
    size_t sizeGrow = strlen(cat);

    // Ensure there is enough space to grow the string
    strResize(this, sizeGrow);

    // Append the string
    strcpy(this->pub.buffer + strSize(this), cat);
    this->pub.size += (unsigned int)sizeGrow;
    this->pub.extra -= (unsigned int)sizeGrow;

    FUNCTION_TEST_RETURN(this);
}

String *
strCatZN(String *this, const char *cat, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, cat);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != NULL);

    // Ensure there is enough space to grow the string
    strResize(this, size);

    // Append the string
    strncpy(this->pub.buffer + strSize(this), cat, size);
    this->pub.buffer[strSize(this) + size] = '\0';

    // Update size/extra
    this->pub.size += (unsigned int)size;
    this->pub.extra -= (unsigned int)size;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strCatChr(String *this, char cat)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, cat);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(cat != 0);

    // Ensure there is enough space to grow the string
    strResize(this, 1);

    // Append the character
    this->pub.buffer[this->pub.size++] = cat;
    this->pub.buffer[this->pub.size] = 0;
    this->pub.extra--;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strCatEncode(String *this, EncodeType type, const Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(ENUM, type);
        FUNCTION_TEST_PARAM(BUFFER, buffer);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    // Ensure there is enough space to grow the string
    size_t encodeSize = encodeToStrSize(type, bufUsed(buffer));
    strResize(this, encodeSize);

    // Append the encoded string
    encodeToStr(type, bufPtrConst(buffer), bufUsed(buffer), this->pub.buffer + strSize(this));

    // Update size/extra
    this->pub.size += (unsigned int)encodeSize;
    this->pub.extra -= (unsigned int)encodeSize;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strCatFmt(String *this, const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(format != NULL);

    // Determine how long the allocated string needs to be
    va_list argumentList;
    va_start(argumentList, format);
    size_t sizeGrow = (size_t)vsnprintf(NULL, 0, format, argumentList);
    va_end(argumentList);

    // Ensure there is enough space to grow the string
    strResize(this, sizeGrow);

    // Append the formatted string
    va_start(argumentList, format);
    vsnprintf(this->pub.buffer + strSize(this), sizeGrow + 1, format, argumentList);
    va_end(argumentList);

    this->pub.size += (unsigned int)sizeGrow;
    this->pub.extra -= (unsigned int)sizeGrow;

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
int
strCmp(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);
    FUNCTION_TEST_END();

    if (this != NULL && compare != NULL)
        FUNCTION_TEST_RETURN(strcmp(strZ(this), strZ(compare)));
    else if (this == NULL)
    {
        if (compare == NULL)
            FUNCTION_TEST_RETURN(0);

        FUNCTION_TEST_RETURN(-1);
    }

    FUNCTION_TEST_RETURN(1);
}

int
strCmpZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(strCmp(this, compare == NULL ? NULL : STRDEF(compare)));
}

/**********************************************************************************************************************************/
String *
strDup(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    String *result = NULL;

    if (this != NULL)
        result = strNewZ(strZ(this));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
bool
strEmpty(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(strSize(this) == 0);
}

/**********************************************************************************************************************************/
bool
strEndsWith(const String *this, const String *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, endsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(endsWith != NULL);

    FUNCTION_TEST_RETURN(strEndsWithZ(this, strZ(endsWith)));
}

bool
strEndsWithZ(const String *this, const char *endsWith)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, endsWith);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(endsWith != NULL);

    bool result = false;
    unsigned int endsWithSize = (unsigned int)strlen(endsWith);

    if (strSize(this) >= endsWithSize)
        result = strcmp(strZ(this) + (strSize(this) - endsWithSize), endsWith) == 0;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
There are two separate implementations because string objects can get the size very efficiently whereas the zero-terminated strings
would need a call to strlen().
***********************************************************************************************************************************/
bool
strEq(const String *this, const String *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, compare);
    FUNCTION_TEST_END();

    bool result = false;

    if (this != NULL && compare != NULL)
    {
        if (strSize(this) == strSize(compare))
            result = strcmp(strZ(this), strZ(compare)) == 0;
    }
    else
        result = this == NULL && compare == NULL;

    FUNCTION_TEST_RETURN(result);
}

bool
strEqZ(const String *this, const char *compare)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, compare);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(compare != NULL);

    FUNCTION_TEST_RETURN(strcmp(strZ(this), compare) == 0);
}

/**********************************************************************************************************************************/
String *
strFirstUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (strSize(this) > 0)
        this->pub.buffer[0] = (char)toupper(this->pub.buffer[0]);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strFirstLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (strSize(this) > 0)
        this->pub.buffer[0] = (char)tolower(this->pub.buffer[0]);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strUpper(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (strSize(this) > 0)
        for (size_t idx = 0; idx <= strSize(this); idx++)
            this->pub.buffer[idx] = (char)toupper(this->pub.buffer[idx]);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strLower(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (strSize(this) > 0)
        for (size_t idx = 0; idx <= strSize(this); idx++)
            this->pub.buffer[idx] = (char)tolower(this->pub.buffer[idx]);

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strPath(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const char *end = this->pub.buffer + strSize(this);

    while (end > this->pub.buffer && *(end - 1) != '/')
        end--;

    FUNCTION_TEST_RETURN(
        strNewN(
            this->pub.buffer,
            end - this->pub.buffer <= 1 ? (size_t)(end - this->pub.buffer) : (size_t)(end - this->pub.buffer - 1)));
}

/**********************************************************************************************************************************/
String *
strPathAbsolute(const String *this, const String *base)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, base);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    // Path is already absolute so just return it
    if (strBeginsWith(this, FSLASH_STR))
    {
        result = strDup(this);
    }
    // Else we'll need to construct the absolute path.  You would hope we could use realpath() here but it is so broken in the
    // Posix spec that is seems best avoided.
    else
    {
        ASSERT(base != NULL);

        // Base must be absolute to start
        if (!strBeginsWith(base, FSLASH_STR))
            THROW_FMT(AssertError, "base path '%s' is not absolute", strZ(base));

        MEM_CONTEXT_TEMP_BEGIN()
        {
            StringList *baseList = strLstNewSplit(base, FSLASH_STR);
            StringList *pathList = strLstNewSplit(this, FSLASH_STR);

            while (!strLstEmpty(pathList))
            {
                const String *pathPart = strLstGet(pathList, 0);

                // If the last part is empty
                if (strSize(pathPart) == 0)
                {
                    // Allow when this is the last part since it just means there was a trailing /
                    if (strLstSize(pathList) == 1)
                    {
                        strLstRemoveIdx(pathList, 0);
                        break;
                    }

                    THROW_FMT(AssertError, "'%s' is not a valid relative path", strZ(this));
                }

                if (strEq(pathPart, DOTDOT_STR))
                {
                    const String *basePart = strLstGet(baseList, strLstSize(baseList) - 1);

                    if (strSize(basePart) == 0)
                        THROW_FMT(AssertError, "relative path '%s' goes back too far in base path '%s'", strZ(this), strZ(base));

                    strLstRemoveIdx(baseList, strLstSize(baseList) - 1);
                }
                else if (!strEq(pathPart, DOT_STR))
                    strLstAdd(baseList, pathPart);

                strLstRemoveIdx(pathList, 0);
            }

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                if (strLstSize(baseList) == 1)
                    result = strDup(FSLASH_STR);
                else
                    result = strLstJoin(baseList, "/");
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    // There should not be any stray .. or // in the final result
    if (strstr(strZ(result), "/..") != NULL || strstr(strZ(result), "//") != NULL)
        THROW_FMT(AssertError, "result path '%s' is not absolute", strZ(result));

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
const char *
strZNull(const String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(this == NULL ? NULL : strZ(this));
}

/**********************************************************************************************************************************/
String *
strQuote(const String *this, const String *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRING, quote);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(quote != NULL);

    FUNCTION_TEST_RETURN(strQuoteZ(this, strZ(quote)));
}

String *
strQuoteZ(const String *this, const char *quote)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(STRINGZ, quote);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(quote != NULL);

    FUNCTION_TEST_RETURN(strNewFmt("%s%s%s", quote, strZ(this), quote));
}

/**********************************************************************************************************************************/
String *
strReplaceChr(String *this, char find, char replace)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, find);
        FUNCTION_TEST_PARAM(CHAR, replace);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    for (size_t stringIdx = 0; stringIdx < strSize(this); stringIdx++)
    {
        if (this->pub.buffer[stringIdx] == find)
            this->pub.buffer[stringIdx] = replace;
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
String *
strSub(const String *this, size_t start)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(start <= this->pub.size);

    FUNCTION_TEST_RETURN(strSubN(this, start, strSize(this) - start));
}

/**********************************************************************************************************************************/
String *
strSubN(const String *this, size_t start, size_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(SIZE, start);
        FUNCTION_TEST_PARAM(SIZE, size);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(start <= strSize(this));
    ASSERT(start + size <= strSize(this));

    FUNCTION_TEST_RETURN(strNewN(strZ(this) + start, size));
}

/**********************************************************************************************************************************/
String *
strTrim(String *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Nothing to trim if size is zero
    if (strSize(this) > 0)
    {
        // Find the beginning of the string skipping all whitespace
        char *begin = this->pub.buffer;

        while (*begin != 0 && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n'))
            begin++;

        // Find the end of the string skipping all whitespace
        char *end = this->pub.buffer + (strSize(this) - 1);

        while (end > begin && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
            end--;

        // Is there anything to trim?
        size_t newSize = (size_t)(end - begin + 1);

        if (begin != this->pub.buffer || newSize < strSize(this))
        {
            // Calculate new size
            this->pub.size = (unsigned int)newSize;

            // Move the substr to the beginning of the buffer
            memmove(this->pub.buffer, begin, strSize(this));
            this->pub.buffer[strSize(this)] = 0;
            this->pub.extra = 0;

            MEM_CONTEXT_BEGIN(this->memContext)
            {
                // Resize the buffer
                this->pub.buffer = memResize(this->pub.buffer, strSize(this) + 1);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
int
strChr(const String *this, char chr)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
        FUNCTION_TEST_PARAM(CHAR, chr);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    int result = -1;

    if (strSize(this) > 0)
    {
        const char *ptr = strchr(this->pub.buffer, chr);

        if (ptr != NULL)
            result = (int)(ptr - this->pub.buffer);
    }

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
strTrunc(String *this, int idx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(idx >= 0 && (size_t)idx <= strSize(this));

    if (strSize(this) > 0)
    {
        // Reset the size to end at the index
        this->pub.size = (unsigned int)(idx);
        this->pub.buffer[strSize(this)] = 0;
        this->pub.extra = 0;

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // Resize the buffer
            this->pub.buffer = memResize(this->pub.buffer, strSize(this) + 1);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Convert an object to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t strObjToLog(const void *object, StrObjToLogFormat formatFunc, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = (size_t)snprintf(buffer, bufferSize, "%s", object == NULL ? NULL_Z : strZ(formatFunc(object)));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/**********************************************************************************************************************************/
String *
strToLog(const String *this)
{
    return this == NULL ? strDup(NULL_STR) : strNewFmt("{\"%s\"}", strZ(this));
}

/**********************************************************************************************************************************/
String *
strSizeFormat(const uint64_t size)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, size);

    FUNCTION_TEST_END();

    String *result = NULL;

    if (size < 1024)
        result = strNewFmt("%" PRIu64 "B", size);
    else if (size < (1024 * 1024))
    {
        if ((uint64_t)((double)size / 102.4) % 10 != 0)
            result = strNewFmt("%.1lfKB", (double)size / 1024);
        else
            result = strNewFmt("%" PRIu64 "KB", size / 1024);
    }
    else if (size < (1024 * 1024 * 1024))
    {
        if ((uint64_t)((double)size / (1024 * 102.4)) % 10 != 0)
            result = strNewFmt("%.1lfMB", (double)size / (1024 * 1024));
        else
            result = strNewFmt("%" PRIu64 "MB", size / (1024 * 1024));
    }
    else
    {
        if ((uint64_t)((double)size / (1024 * 1024 * 102.4)) % 10 != 0)
            result = strNewFmt("%.1lfGB", (double)size / (1024 * 1024 * 1024));
        else
            result = strNewFmt("%" PRIu64 "GB", size / (1024 * 1024 * 1024));
    }

    FUNCTION_TEST_RETURN(result);
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
            memFree(this->pub.buffer);
            memFree(this);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}
