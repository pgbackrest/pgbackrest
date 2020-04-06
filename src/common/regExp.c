/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <regex.h>
#include <sys/types.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Contains information about the regular expression handler
***********************************************************************************************************************************/
struct RegExp
{
    MemContext *memContext;
    regex_t regExp;
    const char *matchPtr;
    size_t matchSize;
};

OBJECT_DEFINE_FREE(REGEXP);

/***********************************************************************************************************************************
Free regular expression
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(REGEXP, TEST, )
{
    regfree(&this->regExp);
}
OBJECT_DEFINE_FREE_RESOURCE_END(TEST);

/***********************************************************************************************************************************
Handle errors
***********************************************************************************************************************************/
static void
regExpError(int error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, error);
    FUNCTION_TEST_END();

    if (error != 0 && error != REG_NOMATCH)
    {
        char buffer[4096];
        regerror(error, NULL, buffer, sizeof(buffer));
        THROW(FormatError, buffer);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
RegExp *
regExpNew(const String *expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);

    RegExp *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("RegExp")
    {
        this = memNew(sizeof(RegExp));

        *this = (RegExp)
        {
            .memContext = MEM_CONTEXT_NEW(),
        };

        // Compile the regexp and process errors
        int result = 0;

        if ((result = regcomp(&this->regExp, strPtr(expression), REG_EXTENDED)) != 0)
        {
            memFree(this);
            regExpError(result);
        }

        // Set free callback to ensure cipher context is freed
        memContextCallbackSet(this->memContext, regExpFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
bool
regExpMatch(RegExp *this, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    // Test for a match
    regmatch_t matchPtr;
    int result = regexec(&this->regExp, strPtr(string), 1, &matchPtr, 0);

    // Check for an error
    regExpError(result);

    // Store match results
    if (result == 0)
    {
        this->matchPtr = strPtr(string) + matchPtr.rm_so;
        this->matchSize = (size_t)(matchPtr.rm_eo - matchPtr.rm_so);
    }
    // Else reset match results
    else
    {
        this->matchPtr = NULL;
        this->matchSize = 0;
    }

    FUNCTION_TEST_RETURN(result == 0);
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
const char *
regExpMatchPtr(RegExp *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->matchPtr);
}

size_t
regExpMatchSize(RegExp *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->matchSize);
}

String *
regExpMatchStr(RegExp *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->matchPtr == NULL ? NULL : strNewN(regExpMatchPtr(this), regExpMatchSize(this)));
}

/**********************************************************************************************************************************/
bool
regExpMatchOne(const String *expression, const String *string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);
    ASSERT(string != NULL);

    bool result = false;
    RegExp *regExp = regExpNew(expression);

    TRY_BEGIN()
    {
        result = regExpMatch(regExp, string);
    }
    FINALLY()
    {
        regExpFree(regExp);
    }
    TRY_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
String *
regExpPrefix(const String *expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    String *result = NULL;

    // Only generate prefix if expression is defined and has a beginning anchor
    if (expression != NULL && strPtr(expression)[0] == '^')
    {
        const char *expressionZ = strPtr(expression);
        size_t expressionSize = strSize(expression);
        unsigned int expressionIdx = 1;

        for (; expressionIdx < expressionSize; expressionIdx++)
        {
            char expressionChr = expressionZ[expressionIdx];

            // Search for characters that will end the prefix
            if (expressionChr == '.' || expressionChr == '^' || expressionChr == '$' || expressionChr == '*' ||
                expressionChr == '+' || expressionChr == '-' || expressionChr == '?' || expressionChr == '(' ||
                expressionChr == '[' || expressionChr == '{' || expressionChr == ' ' || expressionChr == '|' ||
                expressionChr == '\\')
            {
                break;
            }
        }

        // Will there be any characters in the prefix?
        if (expressionIdx > 1)
        {
            // Search the rest of the string for another begin anchor
            unsigned int anchorIdx = expressionIdx;

            for (; anchorIdx < expressionSize; anchorIdx++)
            {
                // [^ and \^ are not begin anchors
                if (expressionZ[anchorIdx] == '^' && expressionZ[anchorIdx - 1] != '[' && expressionZ[anchorIdx - 1] != '\\')
                    break;
            }

            // If no other begin anchor was found then the prefix is usable
            if (anchorIdx == expressionSize)
                result = strSubN(expression, 1, expressionIdx - 1);
        }
    }

    FUNCTION_TEST_RETURN(result);
}
