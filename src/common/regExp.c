/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <regex.h>
#include <sys/types.h>

#include "common/debug.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Contains information about the regular expression handler
***********************************************************************************************************************************/
struct RegExp
{
    regex_t regExp;
};

/***********************************************************************************************************************************
Free regular expression
***********************************************************************************************************************************/
static void
regExpFreeResource(THIS_VOID)
{
    THIS(RegExp);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    regfree(&this->regExp);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Handle errors
***********************************************************************************************************************************/
static void
regExpError(const int error)
{
    char buffer[4096];
    regerror(error, NULL, buffer, sizeof(buffer));
    THROW(FormatError, buffer);
}

static void
regExpErrorCheck(const int error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, error);
    FUNCTION_TEST_END();

    if (error != 0 && error != REG_NOMATCH)
        regExpError(error);

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN RegExp *
regExpNew(const String *const expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);

    OBJ_NEW_BEGIN(RegExp, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (RegExp){{0}};                                      // Extra braces are required for older gcc versions

        // Compile the regexp and process errors
        int result = 0;

        if ((result = regcomp(&this->regExp, strZ(expression), REG_EXTENDED)) != 0)
            regExpError(result);

        // Set free callback to ensure cipher context is freed
        memContextCallbackSet(objMemContext(this), regExpFreeResource, this);
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(REGEXP, this);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
regExpMatch(RegExp *const this, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    // Test for a match
    regmatch_t matchPtr;
    const int result = regexec(&this->regExp, strZ(string), 1, &matchPtr, 0);

    // Check for an error
    regExpErrorCheck(result);

    FUNCTION_TEST_RETURN(BOOL, result == 0);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
regExpMatchOne(const String *const expression, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);
    ASSERT(string != NULL);

    bool result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = regExpMatch(regExpNew(expression), string);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
regExpPrefix(const String *const expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    String *result = NULL;

    // Only generate prefix if expression is defined and has a beginning anchor
    if (expression != NULL && strZ(expression)[0] == '^')
    {
        const char *const expressionZ = strZ(expression);
        const size_t expressionSize = strSize(expression);
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

    FUNCTION_TEST_RETURN(STRING, result);
}
