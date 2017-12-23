/***********************************************************************************************************************************
C Test Wrapper

This wrapper runs the the C unit tests.
***********************************************************************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef NO_ERROR
    #include "common/error.h"
#endif

#include "common/harnessTest.h"

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
main - run the tests
***********************************************************************************************************************************/
int
main(void)
{
    // Set globals
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
#ifndef NO_ERROR
    }
    CATCH_ANY()
    {
        fprintf(stderr, "TEST FAILED WITH %s, \"%s\" at %s:%d\n", errorName(), errorMessage(), errorFileName(), errorFileLine());
    }
#ifndef NO_MEM_CONTEXT
    FINALLY()
    {
        memContextFree(memContextTop());
    }
#endif
    TRY_END();
#endif
}
