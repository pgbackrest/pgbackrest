/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_H
#define TEST_COMMON_HARNESS_H

#include <inttypes.h>

#include "common/harnessTest.intern.h"

#include "common/debug.h"
#include "common/error.h"

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

// Replace common test values in a string and return a buffer with the replacements.
//
// Note that the returned buffer will be overwritten with each call.  Values that can be replaced are:
//
// {[path]} - the current test path
// {[path-data]} - the current test data path
// {[user-id]} - the current test user id
// {[user]} - the current test user
// {[group-id]} - the current test group id
// {[group]} - the current test group
// {[project-exe]} - the project exe
const char *hrnReplaceKey(const char *string);

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

// Path where test data is written
const char *testPath(void);

// Path to a copy of the repository
const char *testRepoPath(void);

// Test OS user
const char *testUser(void);

// Test OS group
const char *testGroup(void);

// Is this test running in a container?
bool testContainer(void);

// Location of the data path were the harness can write data that won't be visible to the test
const char *testDataPath(void);

// Get the 0-based index of the test.  Useful for modifying resources like port numbers to avoid conflicts when running tests in
// parallel.
unsigned int testIdx(void);

// Location of the project exe
const char *testProjectExe(void);

// For scaling performance tests
uint64_t testScale(void);

/***********************************************************************************************************************************
Test that an expected error is actually thrown and error when it isn't
***********************************************************************************************************************************/
#define TEST_ERROR(statement, errorTypeExpected, errorMessageExpected)                                                             \
{                                                                                                                                  \
    bool TEST_ERROR_catch = false;                                                                                                 \
                                                                                                                                   \
    /* Set the line number for the current function in the stack trace */                                                          \
    stackTraceTestFileLineSet(__LINE__);                                                                                           \
                                                                                                                                   \
    hrnTestLogPrefix(__LINE__, true);                                                                                              \
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
    stackTraceTestFileLineSet(0);                                                                                                  \
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
                                                                                                                                   \
    stackTraceTestFileLineSet(0);                                                                                                  \
}

/***********************************************************************************************************************************
Output information about the test
***********************************************************************************************************************************/
#define TEST_RESULT_INFO(...)                                                                                                      \
    hrnTestLogPrefix(__LINE__, true);                                                                                              \
    printf(__VA_ARGS__);                                                                                                           \
    printf("\n");                                                                                                                  \
    fflush(stdout);

/***********************************************************************************************************************************
Test that a void statement returns and does not throw an error
***********************************************************************************************************************************/
#define TEST_RESULT_VOID(statement, ...)                                                                                           \
{                                                                                                                                  \
    TEST_RESULT_INFO(__VA_ARGS__);                                                                                                 \
    hrnTestResultBegin(#statement, __LINE__, false);                                                                               \
    statement;                                                                                                                     \
    hrnTestResultEnd();                                                                                                            \
}

/***********************************************************************************************************************************
Test that a statement does not error and assign it to the specified variable if not
***********************************************************************************************************************************/
#define TEST_ASSIGN(lValue, statement, ...)                                                                                        \
{                                                                                                                                  \
    TEST_RESULT_INFO(__VA_ARGS__);                                                                                                 \
    hrnTestResultBegin(#statement, __LINE__, true);                                                                                \
    lValue = statement;                                                                                                            \
    hrnTestResultEnd();                                                                                                            \
}

/***********************************************************************************************************************************
Macros to compare results of common data types
***********************************************************************************************************************************/
#define TEST_RESULT_BOOL_PARAM(statement, expected, ...)                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultBool(statement, expected);                                                                                    \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_BOOL(statement, expected, ...)                                                                                 \
    TEST_RESULT_BOOL_PARAM(statement, expected, __VA_ARGS__);

#define TEST_RESULT_DOUBLE_PARAM(statement, expected, ...)                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultDouble(statement, expected);                                                                                  \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_DOUBLE(statement, expected, ...)                                                                               \
    TEST_RESULT_DOUBLE_PARAM(statement, expected, __VA_ARGS__);

#define TEST_RESULT_INT_PARAM(statement, expected, operation, ...)                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultInt64(statement, expected, operation);                                                                        \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_INT(statement, expected, ...)                                                                                  \
    TEST_RESULT_INT_PARAM(statement, expected, harnessTestResultOperationEq, __VA_ARGS__);
#define TEST_RESULT_INT_NE(statement, expected, ...)                                                                               \
    TEST_RESULT_INT_PARAM(statement, expected, harnessTestResultOperationNe, __VA_ARGS__);

#define TEST_RESULT_PTR_PARAM(statement, expected, operation, ...)                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultPtr(statement, expected, operation);                                                                          \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_PTR(statement, expected, ...)                                                                                  \
    TEST_RESULT_PTR_PARAM(statement, expected, harnessTestResultOperationEq, __VA_ARGS__);
#define TEST_RESULT_PTR_NE(statement, expected, ...)                                                                               \
    TEST_RESULT_PTR_PARAM(statement, expected, harnessTestResultOperationNe, __VA_ARGS__);

#define TEST_RESULT_Z_PARAM(statement, expected, operation, ...)                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultZ(statement, expected, operation);                                                                            \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_Z(statement, expected, ...)                                                                                    \
    TEST_RESULT_Z_PARAM(statement, expected, harnessTestResultOperationEq, __VA_ARGS__);
#define TEST_RESULT_Z_NE(statement, expected, ...)                                                                                 \
    TEST_RESULT_Z_PARAM(statement, expected, harnessTestResultOperationNe, __VA_ARGS__);

#define TEST_RESULT_STR(statement, resultExpected, ...)                                                                            \
    TEST_RESULT_Z(strPtr(statement), strPtr(resultExpected), __VA_ARGS__);
#define TEST_RESULT_STR_Z(statement, resultExpected, ...)                                                                          \
    TEST_RESULT_Z(strPtr(statement), resultExpected, __VA_ARGS__);
#define TEST_RESULT_STR_Z_KEYRPL(statement, resultExpected, ...)                                                                   \
    TEST_RESULT_Z(strPtr(statement), hrnReplaceKey(resultExpected), __VA_ARGS__);
#define TEST_RESULT_Z_STR(statement, resultExpected, ...)                                                                          \
    TEST_RESULT_Z(statement, strPtr(resultExpected), __VA_ARGS__);

#define TEST_RESULT_UINT_PARAM(statement, expected, operation, ...)                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultUInt64(statement, expected, operation);                                                                       \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_UINT(statement, expected, ...)                                                                                 \
    TEST_RESULT_UINT_PARAM(statement, expected, harnessTestResultOperationEq, __VA_ARGS__);

#define TEST_RESULT_UINT_INT_PARAM(statement, expected, operation, ...)                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO(__VA_ARGS__);                                                                                             \
        hrnTestResultBegin(#statement, __LINE__, true);                                                                            \
        hrnTestResultUInt64Int64(statement, expected, operation);                                                                  \
    }                                                                                                                              \
    while (0)

#define TEST_RESULT_UINT_INT(statement, expected, ...)                                                                             \
    TEST_RESULT_UINT_INT_PARAM(statement, expected, harnessTestResultOperationEq, __VA_ARGS__);

/***********************************************************************************************************************************
Test system calls
***********************************************************************************************************************************/
#define TEST_SYSTEM(command)                                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        int TEST_SYSTEM_FMT_result = system(hrnReplaceKey(command));                                                               \
                                                                                                                                   \
        if (TEST_SYSTEM_FMT_result != 0)                                                                                           \
        {                                                                                                                          \
            THROW_FMT(                                                                                                             \
                AssertError, "SYSTEM COMMAND: %s\n\nFAILED WITH CODE %d\n\nTHROWN AT:\n%s", hrnReplaceKey(command),                \
                TEST_SYSTEM_FMT_result, errorStackTrace());                                                                        \
        }                                                                                                                          \
    } while(0)

#define TEST_SYSTEM_FMT(...)                                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        char TEST_SYSTEM_FMT_buffer[8192];                                                                                         \
                                                                                                                                   \
        if (snprintf(TEST_SYSTEM_FMT_buffer, sizeof(TEST_SYSTEM_FMT_buffer), __VA_ARGS__) >= (int)sizeof(TEST_SYSTEM_FMT_buffer))  \
            THROW_FMT(AssertError, "command needs more than the %zu characters available", sizeof(TEST_SYSTEM_FMT_buffer));        \
                                                                                                                                   \
        TEST_SYSTEM(TEST_SYSTEM_FMT_buffer);                                                                                       \
    } while(0)

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
    } while(0)

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
    } while(0)

/***********************************************************************************************************************************
Logging macros
***********************************************************************************************************************************/
#define TEST_LOG(message)                                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__, true);                                                                                          \
        printf("%s\n", message);                                                                                                   \
        fflush(stdout);                                                                                                            \
    } while(0)

#define TEST_LOG_FMT(format, ...)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__, true);                                                                                          \
        printf(format "\n", __VA_ARGS__);                                                                                          \
        fflush(stdout);                                                                                                            \
    } while(0)

/***********************************************************************************************************************************
Test title macro
***********************************************************************************************************************************/
#define TEST_TITLE(message)                                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__, false);                                                                                         \
        printf("%s\n", message);                                                                                                   \
        fflush(stdout);                                                                                                            \
    } while(0)

#define TEST_TITLE_FMT(format, ...)                                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__, false);                                                                                         \
        printf(format "\n", __VA_ARGS__);                                                                                          \
        fflush(stdout);                                                                                                            \
    } while(0)

/***********************************************************************************************************************************
Is this a 64-bit system?  If not then it is 32-bit since 16-bit systems are not supported.
***********************************************************************************************************************************/
#define TEST_64BIT()                                                                                                               \
    (sizeof(size_t) == 8)

#endif
