/***********************************************************************************************************************************
Regular Expression Handler
***********************************************************************************************************************************/
#include <regex.h>
#include <sys/types.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/regExp.h"

/***********************************************************************************************************************************
Contains information about the regular expression handler
***********************************************************************************************************************************/
struct RegExp
{
    MemContext *memContext;
    regex_t regExp;
};

/***********************************************************************************************************************************
Handle errors
***********************************************************************************************************************************/
static void
regExpError(int error)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, error);
    FUNCTION_TEST_END();

    char buffer[4096];
    regerror(error, NULL, buffer, sizeof(buffer));
    THROW(FormatError, buffer);

    FUNCTION_TEST_RESULT_VOID();
}

/***********************************************************************************************************************************
New regular expression handler
***********************************************************************************************************************************/
RegExp *
regExpNew(const String *expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);

        FUNCTION_TEST_ASSERT(expression != NULL);
    FUNCTION_TEST_END();

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
        memContextCallback(this->memContext, (MemContextCallback)regExpFree, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RESULT(REGEXP, this);
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

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

    // Test for a match
    int result = regexec(&this->regExp, strPtr(string), 0, NULL, 0);

    // Check for an error
    if (result != 0 && result != REG_NOMATCH)                                   // {uncoverable - no error condition known}
        regExpError(result);                                                    // {+uncoverable}

    FUNCTION_TEST_RESULT(BOOL, result == 0);
}

/***********************************************************************************************************************************
Free regular expression
***********************************************************************************************************************************/
void
regExpFree(RegExp *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        regfree(&this->regExp);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_TEST_RESULT_VOID();
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

        FUNCTION_TEST_ASSERT(expression != NULL);
        FUNCTION_TEST_ASSERT(string != NULL);
    FUNCTION_TEST_END();

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

    FUNCTION_TEST_RESULT(BOOL, result);
}
