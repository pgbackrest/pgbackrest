/***********************************************************************************************************************************
C Test Wrapper

This wrapper runs the C unit tests.
***********************************************************************************************************************************/
#include "build.auto.h"

// Enable FUNCTION_TEST*() macros for enhanced debugging
{[C_TEST_DEBUG_TEST_TRACE]}

// Enable memory debugging
#if defined(HRN_FEATURE_MEMCONTEXT) && defined(DEBUG)
    #define DEBUG_MEM
#endif

/***********************************************************************************************************************************
C files to be tested

The files are included directly so the test can see and manipulate variables and functions in the module without the need to extern.
If a .c file does not exist for a module then the header file will be included instead.  They are included first so they won't see
the includes which are required for the test code.
***********************************************************************************************************************************/
{[C_INCLUDE]}

/***********************************************************************************************************************************
The test code is included directly so it can freely interact with the included C files
***********************************************************************************************************************************/
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef HRN_FEATURE_ERROR
    #include "common/error.h"
    #include "common/macro.h"
#endif

#ifdef HRN_FEATURE_DEBUG
    #include "common/debug.h"
#endif

#ifdef HRN_FEATURE_STRING
    #include "common/type/string.h"
#endif

// Name of the project exe
#define TEST_PROJECT_EXE                                            "{[C_TEST_PROJECT_EXE]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(TEST_PROJECT_EXE_STR, TEST_PROJECT_EXE);
#endif

// Path where the test is running
#define TEST_PATH                                                   "{[C_TEST_PATH]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(TEST_PATH_STR, TEST_PATH);
#endif

// Path to the source repository
#define HRN_PATH_REPO                                               "{[C_HRN_PATH_REPO]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(HRN_PATH_REPO_STR, HRN_PATH_REPO);
#endif

// Path where the harness can store data without interfering with the test
#define HRN_PATH                                                    "{[C_HRN_PATH]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(HRN_PATH_STR, HRN_PATH);
#endif

// User running the test
#define TEST_USER                                                   "{[C_TEST_USER]}"
#define TEST_USER_ID                                                {[C_TEST_USER_ID]}
#define TEST_USER_ID_Z                                              "{[C_TEST_USER_ID]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(TEST_USER_STR, TEST_USER);
#endif

// Group running the test
#define TEST_GROUP                                                  "{[C_TEST_GROUP]}"
#define TEST_GROUP_ID                                               {[C_TEST_GROUP_ID]}
#define TEST_GROUP_ID_Z                                             "{[C_TEST_GROUP_ID]}"

#ifdef HRN_FEATURE_STRING
    STRING_EXTERN(TEST_GROUP_STR, TEST_GROUP);
#endif

// Scaling factor for performance tests
#define TEST_SCALE                                                  {[C_TEST_SCALE]}

// Is this test running in a container?
#define TEST_IN_CONTAINER                                           {[C_TEST_CONTAINER]}

// Path to source -- used to construct __FILENAME__ tests
#define TEST_PGB_PATH                                               "{[C_TEST_PGB_PATH]}"

#include "common/harnessDebug.h"
#include "common/harnessTest.intern.h"

#ifdef HRN_FEATURE_LOG
    #include "common/harnessLog.h"
    void harnessLogLevelDefaultSet(LogLevel logLevel);
#endif

#ifdef HRN_FEATURE_MEMCONTEXT
    #include "common/memContext.h"
#endif

#ifdef HRN_FEATURE_LOG
    #include "common/harnessLog.h"
    void harnessLogLevelDefaultSet(LogLevel logLevel);
#endif

{[C_TEST_INCLUDE]}

/***********************************************************************************************************************************
Includes that are not generally used by tests
***********************************************************************************************************************************/
#include <assert.h>

#if defined(HRN_INTEST_SOCKET) || defined(HRN_FEATURE_SOCKET)
    #include "common/io/socket/common.h"
    #include "common/harnessServer.h"
#endif

#ifdef HRN_FEATURE_STAT
    #include "common/stat.h"
#endif

#ifdef HRN_IN_STACKTRACE
    #include "common/stackTrace.h"
#endif

/***********************************************************************************************************************************
main - run the tests
***********************************************************************************************************************************/
int
main(int argListSize, const char *argList[])
{
    // Basic sanity test on input parameters
    if (argListSize == 0 || argList[0] == NULL)
    {
        fprintf(stderr, "at least one argument expected");
        fflush(stderr);
        exit(25);
    }

    // Initialize stack trace for the harness
    FUNCTION_HARNESS_INIT(argList[0]);

    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, argListSize);
        FUNCTION_HARNESS_PARAM(CHARPY, argList);
    FUNCTION_HARNESS_END();

    int result = 0;

#ifdef HRN_FEATURE_ERROR
    static const ErrorHandlerFunction handlerList[] =
    {
#if defined(HRN_INTEST_STACKTRACE) || defined(HRN_FEATURE_STACKTRACE)
        stackTraceClean,
#endif
#if defined(HRN_INTEST_MEMCONTEXT) || defined(HRN_FEATURE_MEMCONTEXT)
        memContextClean,
#endif
    };

    errorHandlerSet(handlerList, LENGTH_OF(handlerList));
#endif

    // Initialize statistics
#if defined(HRN_INTEST_STAT) || defined(HRN_FEATURE_STAT)
    MEM_CONTEXT_TEMP_BEGIN()
    {
        statInit();
    }
    MEM_CONTEXT_TEMP_END();
#endif

    // Set neutral umask for testing
    umask(0000);

    // Set timezone if specified
    {[C_TEST_TZ]}

    // Ignore SIGPIPE and check for EPIPE errors on write() instead
    signal(SIGPIPE, SIG_IGN);

    // Set globals
    hrnInit(
        argList[0],                 // Test exe
        TEST_PROJECT_EXE,           // Project exe
        TEST_IN_CONTAINER,          // Is this test running in a container?
        {[C_TEST_IDX]},             // The 0-based index of this test
        {[C_TEST_TIMING]},          // Is timing enabled (may be disabled for reproducible documentation)
        TEST_PATH,                  // Path where tests write data
        HRN_PATH,                   // Path where the harness stores temp files (expect, diff, etc.)
        HRN_PATH_REPO);             // Path with a copy of the repository

    // Set default test log level
#ifdef HRN_FEATURE_LOG
    harnessLogLevelDefaultSet({[C_LOG_LEVEL_TEST]});
#endif

    // Use aggressive keep-alive settings for testing
#if defined(HRN_INTEST_SOCKET) || defined(HRN_FEATURE_SOCKET)
    sckInit(false, true, 2, 5, 5);
    hrnServerInit();
#endif

    // Initialize tests
    //     run, selected
    {[C_TEST_LIST]}

#ifdef HRN_FEATURE_ERROR
    TRY_BEGIN()
    {
        TRY_BEGIN()
        {
#endif

#ifdef HRN_FEATURE_MEMCONTEXT
            MEM_CONTEXT_TEMP_BEGIN()
            {
#endif
                // Run the tests
                testRun();
#ifdef HRN_FEATURE_MEMCONTEXT
            }
            MEM_CONTEXT_TEMP_END();
#endif

#ifdef HRN_FEATURE_ERROR
        }
        CATCH_ANY()
        {
            // If a test was running then throw a detailed result exception
#ifdef DEBUG
            if (!errorInstanceOf(&TestError))
#endif
                hrnTestResultException();

            // Else rethrow the original error
            RETHROW();
        }
        TRY_END();
#endif

        // End test run and make sure all tests completed
        hrnComplete();

        printf("\nTESTS COMPLETED SUCCESSFULLY\n");
        fflush(stdout);
#ifdef HRN_FEATURE_ERROR
    }
    CATCH_FATAL()
    {
        // Make the error really obvious
        fprintf(
            stderr,
            "\nTEST FAILED WITH %s:\n\n"
                "--------------------------------------------------------------------------------\n"
                "%s\n"
                "--------------------------------------------------------------------------------\n",
            errorName(), errorMessage());

        // If not a TestError (which is detailed) then also print the stack trace
#ifdef DEBUG
        if (!errorInstanceOf(&TestError))
#endif
            fprintf(stderr, "\nTHROWN AT:\n%s\n", errorStackTrace());

        fflush(stderr);
        result = errorCode();
    }
#ifdef HRN_FEATURE_MEMCONTEXT
    FINALLY()
    {
        memContextFree(memContextTop());
    }
#endif
    TRY_END();
#endif

    // Switch to build path when profiling so profile data gets written to a predictable location
#if {[C_TEST_PROFILE]}
    if (chdir("{[C_TEST_PATH_BUILD]}") != 0)
    {
        fprintf(stderr, "unable to chdir to '{[C_TEST_PATH_BUILD]}'");
        fflush(stderr);
        result = 25;
    }
#endif

    FUNCTION_HARNESS_RETURN(INT, result);
}
