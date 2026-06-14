/***********************************************************************************************************************************
Regular Expression Handler Extensions
***********************************************************************************************************************************/
// Include core module
#include "common/regExp.c"

#include "build/common/regExp.h"

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
const char *
regExpMatchPtr(RegExp *const this, const String *const string)
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

    // Return pointer to match
    if (result == 0)
        FUNCTION_TEST_RETURN_CONST(STRINGZ, strZ(string) + matchPtr.rm_so);

    // Return NULL when no match
    FUNCTION_TEST_RETURN_CONST(STRINGZ, NULL);
}

String *
regExpMatchStr(RegExp *const this, const String *const string)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, this);
        FUNCTION_TEST_PARAM(STRING, string);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(string != NULL);

    // Test for a match
    regmatch_t matchPtr;
    int result = regexec(&this->regExp, strZ(string), 1, &matchPtr, 0);

    // Check for an error
    regExpErrorCheck(result);

    // Return match as string
    if (result == 0)
        FUNCTION_TEST_RETURN(STRING, strNewZN(strZ(string) + matchPtr.rm_so, (size_t)(matchPtr.rm_eo - matchPtr.rm_so)));

    // Return NULL when no match
    FUNCTION_TEST_RETURN(STRING, NULL);
}
