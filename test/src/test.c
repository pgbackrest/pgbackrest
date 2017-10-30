/***********************************************************************************************************************************
C Test Wrapper

This wrapper runs the the C unit tests.
***********************************************************************************************************************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common/harnessTest.h"

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
    // Initialize tests
    //      run, selected
    {[C_TEST_LIST]}

    // Run the tests
    testRun();

    // End test run an make sure all tests ran
    testComplete();

    printf("\nTESTS COMPLETED SUCCESSFULLY (DESPITE ANY ERROR MESSAGES YOU SAW)\n");
    fflush(stdout);
}
