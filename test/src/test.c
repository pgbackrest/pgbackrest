/***********************************************************************************************************************************
C Test Wrapper

This wrapper runs the the C unit tests.
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef NO_ERROR
    #include "common/debug.h"
    #include "common/error.h"
#endif

#include "common/harnessDebug.h"
#include "common/harnessTest.h"

#ifndef NO_LOG
    #include "common/harnessLog.h"
#endif

#ifndef NO_MEM_CONTEXT
    #include "common/memContext.h"
#endif

/***********************************************************************************************************************************
C files to be tested

The files are included directly so the test can see and manipulate variables and functions in the module without the need to extern.
If a .c file does not exist for a module then the header file will be included instead.
***********************************************************************************************************************************/
{[C_INCLUDE]}

/***********************************************************************************************************************************
The test code is included directly so it can freely interact with the included C files
***********************************************************************************************************************************/
{[C_TEST_INCLUDE]}

/***********************************************************************************************************************************
Include assert.h here since it is not generally used by tests
***********************************************************************************************************************************/
#include <assert.h>

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

    // Set neutral umask for testing
    umask(0000);

    // Set globals
    testExeSet(argList[0]);
    testPathSet("{[C_TEST_PATH]}");

    // Initialize tests
    //      run, selected
    {[C_TEST_LIST]}

#ifndef NO_ERROR
    TRY_BEGIN()
    {
#endif
        // Run the tests
        testRun();

        // End test run and make sure all tests completed
        testComplete();

        printf("\nTESTS COMPLETED SUCCESSFULLY\n");
        fflush(stdout);
#ifndef NO_ERROR
    }
    CATCH_ANY()
    {
        fprintf(
            stderr,
            "\nTEST FAILED WITH %s:\n\n"
                "--------------------------------------------------------------------------------\n"
                "%s\n"
                "--------------------------------------------------------------------------------\n"
                "\nTHROWN AT:\n%s\n",
            errorName(), errorMessage(), errorStackTrace());

        fflush(stderr);
        result = errorCode();
    }
#ifndef NO_MEM_CONTEXT
    FINALLY()
    {
        memContextFree(memContextTop());
    }
#endif
    TRY_END();
#endif

    FUNCTION_HARNESS_RESULT(INT, result);
}
