/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_H
#define TEST_COMMON_HARNESS_H

#include <inttypes.h>

#include "common/debug.h"
#include "common/error.h"

#include "common/harnessTest.intern.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define BOGUS_STR                                                   "BOGUS"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Begin a test if this function returns true, otherwise the user has skipped it
bool testBegin(const char *name);

// Read a file (max 256k) into a buffer
void hrnFileRead(const char *fileName, unsigned char *buffer, size_t bufferSize);

// Write a buffer to a file
void hrnFileWrite(const char *fileName, const unsigned char *buffer, size_t bufferSize);

// Diff two strings using command-line diff tool
const char *hrnDiff(const char *actual, const char *expected);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Time in MS
uint64_t testTimeMSec(void);

// Time in MS at the beginning of the test run (since testBegin() was called)
uint64_t testTimeMSecBegin(void);

// The path and name of the test executable
const char *testExe(void);

// Is this test running in a container?
bool testContainer(void);

// Get the 0-based index of the test.  Useful for modifying resources like port numbers to avoid conflicts when running tests in
// parallel.
unsigned int testIdx(void);

/***********************************************************************************************************************************
Test that an expected error is actually thrown and error when it isn't
***********************************************************************************************************************************/
#define TEST_ERROR(statement, errorTypeExpected, errorMessageExpected)                                                             \
{                                                                                                                                  \
    bool TEST_ERROR_catch = false;                                                                                                 \
                                                                                                                                   \
    /* Set the line number for the current function in the stack trace */                                                          \
    FUNCTION_HARNESS_STACK_TRACE_LINE_SET(__LINE__);                                                                               \
                                                                                                                                   \
    hrnTestLogPrefix(__LINE__);                                                                                                    \
    printf("expect %s: %s\n", errorTypeName(&errorTypeExpected), errorMessageExpected);                                            \
    fflush(stdout);                                                                                                                \
                                                                                                                                   \
    TRY_BEGIN()                                                                                                                    \
    {                                                                                                                              \
        statement;                                                                                                                 \
    }                                                                                                                              \
    CATCH_ANY()                                                                                                                    \
    {                                                                                                                              \
        TEST_ERROR_catch = true;                                                                                                   \
                                                                                                                                   \
        if (strcmp(errorMessage(), errorMessageExpected) != 0 || errorType() != &errorTypeExpected)                                \
            THROW_FMT(                                                                                                             \
                TestError, "EXPECTED %s: %s\n\n BUT GOT %s: %s\n\nTHROWN AT:\n%s", errorTypeName(&errorTypeExpected),              \
                errorMessageExpected, errorName(), errorMessage(), errorStackTrace());                                             \
    }                                                                                                                              \
    TRY_END();                                                                                                                     \
                                                                                                                                   \
    if (!TEST_ERROR_catch)                                                                                                         \
        THROW_FMT(                                                                                                                 \
            TestError, "statement '%s' returned but error %s, '%s' was expected", #statement, errorTypeName(&errorTypeExpected),   \
            errorMessageExpected);                                                                                                 \
                                                                                                                                   \
    FUNCTION_HARNESS_STACK_TRACE_LINE_SET(0);                                                                                      \
}

/***********************************************************************************************************************************
Test error with a formatted expected message
***********************************************************************************************************************************/
#define TEST_ERROR_FMT(statement, errorTypeExpected, ...)                                                                          \
{                                                                                                                                  \
    char TEST_ERROR_FMT_buffer[8192];                                                                                              \
                                                                                                                                   \
    if (snprintf(TEST_ERROR_FMT_buffer, sizeof(TEST_ERROR_FMT_buffer), __VA_ARGS__) >= (int)sizeof(TEST_ERROR_FMT_buffer))         \
        THROW_FMT(AssertError, "error message needs more than the %zu characters available", sizeof(TEST_ERROR_FMT_buffer));       \
                                                                                                                                   \
    TEST_ERROR(statement, errorTypeExpected, TEST_ERROR_FMT_buffer);                                                               \
}

/***********************************************************************************************************************************
Output information about the test
***********************************************************************************************************************************/
#define TEST_RESULT_INFO(comment)                                                                                                  \
    hrnTestLogPrefix(__LINE__);                                                                                                    \
    puts(comment);                                                                                                                 \
    fflush(stdout);

/***********************************************************************************************************************************
Test that a void statement returns and does not throw an error
***********************************************************************************************************************************/
#define TEST_RESULT_VOID(statement, comment)                                                                                       \
{                                                                                                                                  \
    TEST_RESULT_INFO(comment);                                                                                                     \
    hrnTestResultBegin(#statement, false);                                                                                         \
    statement;                                                                                                                     \
    hrnTestResultEnd();                                                                                                            \
}

/***********************************************************************************************************************************
Test that a statement does not error and assign it to the specified variable if not
***********************************************************************************************************************************/
#define TEST_ASSIGN(lValue, statement, comment)                                                                                    \
{                                                                                                                                  \
    TEST_RESULT_INFO(comment);                                                                                                     \
    hrnTestResultBegin(#statement, true);                                                                                          \
    lValue = statement;                                                                                                            \
    hrnTestResultEnd();                                                                                                            \
}

/***********************************************************************************************************************************
Macros to compare results of common data types
***********************************************************************************************************************************/
#define TEST_RESULT_BOOL_PARAM(statement, expected, comment)                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultBool(statement, expected);                                                                                    \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_BOOL(statement, expected, comment)                                                                             \
    TEST_RESULT_BOOL_PARAM(statement, expected, comment)

#define TEST_RESULT_DOUBLE_PARAM(statement, expected, comment)                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultDouble(statement, expected);                                                                                  \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_DOUBLE(statement, expected, comment)                                                                           \
    TEST_RESULT_DOUBLE_PARAM(statement, expected, comment)

#define TEST_RESULT_INT_PARAM(statement, expected, operation, comment)                                                             \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultInt64(statement, expected, operation);                                                                        \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_INT(statement, expected, comment)                                                                              \
    TEST_RESULT_INT_PARAM(statement, expected, harnessTestResultOperationEq, comment)
#define TEST_RESULT_INT_NE(statement, expected, comment)                                                                           \
    TEST_RESULT_INT_PARAM(statement, expected, harnessTestResultOperationNe, comment)

#define TEST_RESULT_PTR_PARAM(statement, expected, operation, comment)                                                             \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultPtr(statement, expected, operation);                                                                          \
    }                                                                                                                              \
    while (0)

// Compare raw pointers. When checking for NULL use the type-specific macro when available, e.g. TEST_RESULT_STR(). This is more
// type-safe and makes it clearer what is being tested.
#define TEST_RESULT_PTR(statement, expected, comment)                                                                              \
    TEST_RESULT_PTR_PARAM(statement, expected, harnessTestResultOperationEq, comment)
#define TEST_RESULT_PTR_NE(statement, expected, comment)                                                                           \
    TEST_RESULT_PTR_PARAM(statement, expected, harnessTestResultOperationNe, comment)

#define TEST_RESULT_Z_PARAM(statement, expected, operation, comment)                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultZ(statement, expected, operation);                                                                            \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_Z(statement, expected, comment)                                                                                \
    TEST_RESULT_Z_PARAM(statement, expected, harnessTestResultOperationEq, comment)
#define TEST_RESULT_Z_NE(statement, expected, comment)                                                                             \
    TEST_RESULT_Z_PARAM(statement, expected, harnessTestResultOperationNe, comment)

#define TEST_RESULT_STR(statement, resultExpected, comment)                                                                        \
    TEST_RESULT_Z(strZNull(statement), strZNull(resultExpected), comment)
#define TEST_RESULT_STR_Z(statement, resultExpected, comment)                                                                      \
    TEST_RESULT_Z(strZNull(statement), resultExpected, comment)
#define TEST_RESULT_Z_STR(statement, resultExpected, comment)                                                                      \
    TEST_RESULT_Z(statement, strZNull(resultExpected), comment)

// Compare a string list to a \n delimited string
#define TEST_RESULT_STRLST_Z(statement, resultExpected, comment)                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultStringList(statement, resultExpected, harnessTestResultOperationEq);                                          \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_STRLST_STR(statement, resultExpected, comment)                                                                 \
    TEST_RESULT_STRLST_Z(statement, strZNull(resultExpected), comment)

#define TEST_RESULT_UINT_PARAM(statement, expected, operation, comment)                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultUInt64(statement, expected, operation);                                                                       \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_UINT(statement, expected, comment)                                                                             \
    TEST_RESULT_UINT_PARAM(statement, expected, harnessTestResultOperationEq, comment)

#define TEST_RESULT_UINT_INT_PARAM(statement, expected, operation, comment)                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(comment);                                                                                                 \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnTestResultUInt64Int64(statement, expected, operation);                                                                  \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_UINT_INT(statement, expected, comment)                                                                         \
    TEST_RESULT_UINT_INT_PARAM(statement, expected, harnessTestResultOperationEq, comment)

/***********************************************************************************************************************************
System call harness
***********************************************************************************************************************************/
#define HRN_SYSTEM(command)                                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        int TEST_SYSTEM_FMT_result = system(command);                                                                              \
                                                                                                                                   \
        if (TEST_SYSTEM_FMT_result != 0)                                                                                           \
        {                                                                                                                          \
            THROW_FMT(                                                                                                             \
                AssertError, "SYSTEM COMMAND: %s\n\nFAILED WITH CODE %d\n\nTHROWN AT:\n%s", command, TEST_SYSTEM_FMT_result,       \
                errorStackTrace());                                                                                                \
        }                                                                                                                          \
    } while (0)

#define HRN_SYSTEM_FMT(...)                                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        char TEST_SYSTEM_FMT_buffer[8192];                                                                                         \
                                                                                                                                   \
        if (snprintf(TEST_SYSTEM_FMT_buffer, sizeof(TEST_SYSTEM_FMT_buffer), __VA_ARGS__) >= (int)sizeof(TEST_SYSTEM_FMT_buffer))  \
            THROW_FMT(AssertError, "command needs more than the %zu characters available", sizeof(TEST_SYSTEM_FMT_buffer));        \
                                                                                                                                   \
        HRN_SYSTEM(TEST_SYSTEM_FMT_buffer);                                                                                        \
    } while (0)

/***********************************************************************************************************************************
Test log result
***********************************************************************************************************************************/
#define TEST_RESULT_LOG(expected)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        TRY_BEGIN()                                                                                                                \
        {                                                                                                                          \
            harnessLogResult(expected);                                                                                            \
        }                                                                                                                          \
        CATCH_ANY()                                                                                                                \
        {                                                                                                                          \
            THROW_FMT(AssertError, "LOG RESULT FAILED WITH:\n%s", errorMessage());                                                 \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    } while (0)

#define TEST_RESULT_LOG_FMT(...)                                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        char TEST_RESULT_LOG_FMT_buffer[65536];                                                                                    \
                                                                                                                                   \
        if (snprintf(TEST_RESULT_LOG_FMT_buffer, sizeof(TEST_RESULT_LOG_FMT_buffer), __VA_ARGS__) >=                               \
            (int)sizeof(TEST_RESULT_LOG_FMT_buffer))                                                                               \
        {                                                                                                                          \
            THROW_FMT(                                                                                                             \
                AssertError, "expected result needs more than the %zu characters available", sizeof(TEST_RESULT_LOG_FMT_buffer));  \
        }                                                                                                                          \
                                                                                                                                   \
        TEST_RESULT_LOG(TEST_RESULT_LOG_FMT_buffer);                                                                               \
    } while (0)

/***********************************************************************************************************************************
Logging macros
***********************************************************************************************************************************/
#define TEST_LOG(message)                                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        printf("%s\n", message);                                                                                                   \
        fflush(stdout);                                                                                                            \
    } while (0)

#define TEST_LOG_FMT(format, ...)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        printf(format "\n", __VA_ARGS__);                                                                                          \
        fflush(stdout);                                                                                                            \
    } while (0)

/***********************************************************************************************************************************
Test title macro
***********************************************************************************************************************************/
#define TEST_TITLE(message)                                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogTitle(__LINE__);                                                                                                 \
        printf(" %s\n", message);                                                                                                  \
        fflush(stdout);                                                                                                            \
    } while (0)

#define TEST_TITLE_FMT(format, ...)                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogTitle(__LINE__);                                                                                                 \
        printf(" " format "\n", __VA_ARGS__);                                                                                      \
        fflush(stdout);                                                                                                            \
    } while (0)

/***********************************************************************************************************************************
Is this a 64-bit system?  If not then it is 32-bit since 16-bit systems are not supported.
***********************************************************************************************************************************/
#define TEST_64BIT()                                                                                                               \
    (sizeof(size_t) == 8)

/***********************************************************************************************************************************
Is this a big-endian system?
***********************************************************************************************************************************/
#define TEST_BIG_ENDIAN() (!*(unsigned char *)&(uint16_t){1})

#endif
