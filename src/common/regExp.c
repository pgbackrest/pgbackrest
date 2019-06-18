/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <regex.h>
#include <sys/types.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Contains information about the regular expression handler
***********************************************************************************************************************************/
struct RegExp
{
    MemContext *memContext;
    regex_t regExp;
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

/***********************************************************************************************************************************
New regular expression handler
***********************************************************************************************************************************/
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
        this->memContext = MEM_CONTEXT_NEW();

        // Compile the regexp and process errors
        int result = 0;

        if ((result = regcomp(&this->regExp, strPtr(expression), REG_NOSUB | REG_EXTENDED)) != 0)
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

/***********************************************************************************************************************************
Match on a regular expression
***********************************************************************************************************************************/
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
    int result = regexec(&this->regExp, strPtr(string), 0, NULL, 0);

    // Check for an error
    regExpError(result);

    FUNCTION_TEST_RETURN(result == 0);
}

/***********************************************************************************************************************************
Match a regular expression in one call for brevity
***********************************************************************************************************************************/
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

/***********************************************************************************************************************************
Return the constant first part of the regular expression if it has a beginning anchor

This works by scanning the string until the first special regex character is found so escaped characters will not be included.
***********************************************************************************************************************************/
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
        unsigned int expressionIdx = 1;

        for (; expressionIdx < strSize(expression); expressionIdx++)
        {
            char expressionChr = strPtr(expression)[expressionIdx];

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
            result = strSubN(expression, 1, expressionIdx - 1);
    }

    FUNCTION_TEST_RETURN(result);
}
