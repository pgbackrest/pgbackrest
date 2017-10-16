/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#include "common/error.h"
#include "common/type.h"

// Bogus values
#define BOGUS_STR                                                   "BOGUS"

// Functions
void testAdd(int run, bool selected);
bool testBegin(const char *name);
void testComplete();

/***********************************************************************************************************************************
Maximum size of a formatted result in the TEST_RESULT macro.  Strings don't count as they are output directly, so this only applies
to the formatting of bools, ints, floats, etc.  This should be plenty of room for any of those types.
***********************************************************************************************************************************/
#define TEST_RESULT_FORMAT_SIZE                                     128

/***********************************************************************************************************************************
TEST_ERROR macro

Test that an expected error is actually thrown and error when it isn't.
***********************************************************************************************************************************/
#define TEST_ERROR(statement, errorTypeExpected, errorMessageExpected)                                                             \
{                                                                                                                                  \
    bool TEST_ERROR_catch = false;                                                                                                 \
                                                                                                                                   \
    printf("    l%04d - expect error: %s\n", __LINE__, errorMessageExpected);                                                      \
    fflush(stdout);                                                                                                                \
                                                                                                                                   \
    ERROR_TRY()                                                                                                                    \
    {                                                                                                                              \
        statement;                                                                                                                 \
    }                                                                                                                              \
    ERROR_CATCH_ANY()                                                                                                              \
    {                                                                                                                              \
        TEST_ERROR_catch = true;                                                                                                   \
                                                                                                                                   \
        if (strcmp(errorMessage(), errorMessageExpected) != 0 || errorType() != &errorTypeExpected)                                \
            ERROR_THROW(                                                                                                           \
                AssertError, "expected error %s, '%s' but got %s, '%s'", errorTypeName(&errorTypeExpected), errorMessageExpected,  \
                errorName(), errorMessage());                                                                                      \
    }                                                                                                                              \
                                                                                                                                   \
    if (!TEST_ERROR_catch)                                                                                                         \
        ERROR_THROW(                                                                                                               \
            AssertError, "statement '%s' returned but error %s, '%s' was expected", #statement, errorTypeName(&errorTypeExpected), \
            errorMessageExpected);                                                                                                 \
}

/***********************************************************************************************************************************
TEST_TYPE_PTR - is the type a pointer?
***********************************************************************************************************************************/
#define TEST_TYPE_PTR(type)                                                                                                        \
    (#type[strlen(#type) - 1] == '*')

/***********************************************************************************************************************************
TEST_TYPE_FORMAT - format the test type into the given buffer -- or return verbatim if char *
***********************************************************************************************************************************/
#define TEST_TYPE_FORMAT(type, format, value)                                                                                      \
    char value##StrBuffer[TEST_RESULT_FORMAT_SIZE + 1];                                                                            \
    char *value##Str = value##StrBuffer;                                                                                           \
                                                                                                                                   \
    if (TEST_TYPE_PTR(type) && *(void **)&value == NULL)                                                                           \
        value##Str = "NULL";                                                                                                       \
    else if (strcmp(#type, "char *") == 0)                                                                                         \
        value##Str = *(char **)&value;                                                                                             \
    else                                                                                                                           \
    {                                                                                                                              \
        if (snprintf(value##Str, TEST_RESULT_FORMAT_SIZE + 1, format, value) > TEST_RESULT_FORMAT_SIZE)                            \
        {                                                                                                                          \
            ERROR_THROW(                                                                                                           \
                AssertError, "formatted type '" format "' needs more than the %d characters available", TEST_RESULT_FORMAT_SIZE);  \
        }                                                                                                                          \
    }

/***********************************************************************************************************************************
TEST_RESULT macro

Test the result of a statement and make sure it matches the expected value.  This macro can test any C type given the correct
parameters.
***********************************************************************************************************************************/
#define TEST_RESULT(statement, resultExpectedValue, type, format, typeOp, ...)                                                     \
{                                                                                                                                  \
    /* Assign expected result to a local variable so the value can be manipulated as a pointer */                                  \
    type TEST_RESULT_resultExpected = (type)(resultExpectedValue);                                                                 \
                                                                                                                                   \
    /* Output test info */                                                                                                         \
    printf("    l%04d - ", __LINE__);                                                                                              \
    printf(__VA_ARGS__);                                                                                                           \
    printf("\n");                                                                                                                  \
    fflush(stdout);                                                                                                                \
                                                                                                                                   \
    /* Format the expected result */                                                                                               \
    TEST_TYPE_FORMAT(type, format, TEST_RESULT_resultExpected);                                                                    \
                                                                                                                                   \
    /* Try to run the statement */                                                                                                 \
    type TEST_RESULT_result;                                                                                                       \
                                                                                                                                   \
    ERROR_TRY()                                                                                                                    \
    {                                                                                                                              \
        TEST_RESULT_result = (type)(statement);                                                                                    \
    }                                                                                                                              \
    /* Catch any errors */                                                                                                         \
    ERROR_CATCH_ANY()                                                                                                              \
    {                                                                                                                              \
        /* No errors were expected so error */                                                                                     \
        ERROR_THROW(                                                                                                               \
            AssertError, "statement '%s' threw error %s, '%s' but result <%s> expected",                                           \
            #statement, errorName(), errorMessage(), TEST_RESULT_resultExpectedStr);                                               \
    }                                                                                                                              \
                                                                                                                                   \
   /* Test the type operator */                                                                                                    \
    bool TEST_RESULT_resultOp = false;                                                                                             \
                                                                                                                                   \
    if (strcmp(#type, "char *") == 0                                                                                               \
        && *(void **)&TEST_RESULT_result != NULL && *(void **)&TEST_RESULT_resultExpected != NULL)                                 \
        TEST_RESULT_resultOp = strcmp(*(char **)&TEST_RESULT_result, *(char **)&TEST_RESULT_resultExpected) typeOp 0;              \
    else                                                                                                                           \
        TEST_RESULT_resultOp = TEST_RESULT_result typeOp TEST_RESULT_resultExpected;                                               \
                                                                                                                                   \
    /* If type operator test was not successful */                                                                                 \
    if (!TEST_RESULT_resultOp)                                                                                                     \
    {                                                                                                                              \
        /* Format the actual result */                                                                                             \
        TEST_TYPE_FORMAT(type, format, TEST_RESULT_result);                                                                        \
                                                                                                                                   \
        /* Throw error */                                                                                                          \
        ERROR_THROW(                                                                                                               \
            AssertError, "statement '%s' result is '%s' but '%s' expected",                                                        \
            #statement, TEST_RESULT_resultStr, TEST_RESULT_resultExpectedStr);                                                     \
    }                                                                                                                              \
}

/***********************************************************************************************************************************
TEST_RESULT* macros

Macros to ease use of common data types.
***********************************************************************************************************************************/
#define TEST_RESULT_BOOL_PARAM(statement, resultExpected, typeOp, ...)                                                             \
    TEST_RESULT(statement, resultExpected, bool, "%d", typeOp, __VA_ARGS__);
#define TEST_RESULT_BOOL(statement, resultExpected, ...)                                                                           \
    TEST_RESULT_BOOL_PARAM(statement, resultExpected, ==, __VA_ARGS__);

#define TEST_RESULT_DOUBLE_PARAM(statement, resultExpected, typeOp, ...)                                                           \
    TEST_RESULT(statement, resultExpected, double, "%f", typeOp, __VA_ARGS__);
#define TEST_RESULT_DOUBLE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_DOUBLE_PARAM(statement, resultExpected, ==, __VA_ARGS__);

#define TEST_RESULT_INT_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, int, "%d", typeOp, __VA_ARGS__);
#define TEST_RESULT_INT(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_INT_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_INT_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_INT_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_PTR_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, void *, "%p", typeOp, __VA_ARGS__);
#define TEST_RESULT_PTR(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_PTR_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_PTR_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_PTR_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_STR_PARAM(statement, resultExpected, typeOp, ...)                                                              \
    TEST_RESULT(statement, resultExpected, char *, "%s", typeOp, __VA_ARGS__);
#define TEST_RESULT_STR(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_STR_PARAM(statement, resultExpected, ==, __VA_ARGS__);
#define TEST_RESULT_STR_NE(statement, resultExpected, ...)                                                                         \
    TEST_RESULT_STR_PARAM(statement, resultExpected, !=, __VA_ARGS__);

#define TEST_RESULT_U16_HEX(statement, resultExpected, ...)                                                                        \
    TEST_RESULT(statement, resultExpected, uint16, "0x%04X", ==, __VA_ARGS__);
