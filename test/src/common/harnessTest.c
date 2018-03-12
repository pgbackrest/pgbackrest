/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/harnessTest.h"
#include "common/logTest.h"

#define TEST_LIST_SIZE                                              64

typedef struct TestData
{
    bool selected;
} TestData;

static TestData testList[TEST_LIST_SIZE];

static int testRun = 0;
static int testTotal = 0;
static bool testFirst = true;

static const char *testPathData = NULL;

/***********************************************************************************************************************************
Get and set the test path, i.e., the path where this test should write its files
***********************************************************************************************************************************/
const char *
testPath()
{
    return testPathData;
}

void
testPathSet(const char *testPathParam)
{
    testPathData = testPathParam;
}

/***********************************************************************************************************************************
testAdd - add a new test
***********************************************************************************************************************************/
void
testAdd(int run, bool selected)
{
    if (run != testTotal + 1)
    {
        fprintf(stderr, "ERROR: test run %d is not in order\n", run);
        fflush(stderr);
        exit(255);
    }

    testList[testTotal].selected = selected;
    testTotal++;
}

/***********************************************************************************************************************************
testBegin - should this test run?
***********************************************************************************************************************************/
bool
testBegin(const char *name)
{
    testRun++;

    if (testList[testRun - 1].selected)
    {
#ifndef NO_LOG
        // Make sure there is nothing untested left in the log
        if (!testFirst)
            testLogFinal();
#endif
        // No longer the first test
        testFirst = false;

        if (testRun != 1)
            printf("\n");

        printf("run %03d - %s\n", testRun, name);
        fflush(stdout);

#ifndef NO_LOG
        // Initialize logging
        testLogInit();
#endif

        return true;
    }

    return false;
}

/***********************************************************************************************************************************
testComplete - make sure all expected tests ran
***********************************************************************************************************************************/
void
testComplete()
{
#ifndef NO_LOG
    // Make sure there is nothing untested left in the log
    testLogFinal();
#endif

    // Check that all tests ran
    if (testRun != testTotal)
    {
        fprintf(stderr, "ERROR: expected %d tests but %d were run\n", testTotal, testRun);
        fflush(stderr);
        exit(255);
    }
}
