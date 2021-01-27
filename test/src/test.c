/***********************************************************************************************************************************
C Test Wrapper

This wrapper runs the the C unit tests.
***********************************************************************************************************************************/

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
#endif

#ifdef HRN_FEATURE_DEBUG
    #include "common/debug.h"
#endif

// Path where the test is running
#define TEST_PATH                                                   "{[C_TEST_PATH]}"

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

{[C_TEST_INCLUDE]}

/***********************************************************************************************************************************
Includes that are not generally used by tests
***********************************************************************************************************************************/
#include <assert.h>

#include "common/io/socket/common.h"

#ifdef HRN_FEATURE_STAT
    #include "common/stat.h"
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

    // Initialize statistics
#if defined(HRN_INTEST_STAT) || defined(HRN_FEATURE_STAT)
    statInit();
#endif

    // Use aggressive keep-alive settings for testing
#if defined(HRN_INTEST_SOCKET) || defined(HRN_FEATURE_SOCKET)
    sckInit(false, true, 2, 5, 5);
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
        "{[C_TEST_PROJECT_EXE]}",   // Project exe
        {[C_TEST_CONTAINER]},       // Is this test running in a container?
        {[C_TEST_IDX]},             // The 0-based index of this test
        {[C_TEST_TIMING]},          // Is timing enabled (may be disabled for reproducible documentation)
        {[C_TEST_SCALE]},           // Scaling factor for performance tests
        "{[C_TEST_PATH]}",          // Path where tests write data
        "{[C_TEST_DATA_PATH]}",     // Path where the harness stores temp files (expect, diff, etc.)
        "{[C_TEST_REPO_PATH]}");    // Path with a copy of the repository

    // Set default test log level
#ifdef HRN_FEATURE_LOG
    harnessLogLevelDefaultSet({[C_LOG_LEVEL_TEST]});
#endif

    // Initialize tests
    //      run, selected
    {[C_TEST_LIST]}

#ifdef HRN_FEATURE_ERROR
    TRY_BEGIN()
    {
        TRY_BEGIN()
        {
#endif
            // Run the tests
            testRun();
#ifdef HRN_FEATURE_ERROR
        }
        CATCH_ANY()
        {
            // If a test was running then throw a detailed result exception
#ifndef NDEBUG
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
    CATCH_ANY()
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
#ifndef NDEBUG
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

    FUNCTION_HARNESS_RESULT(INT, result);
}
