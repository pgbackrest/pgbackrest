/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "common/harnessDebug.h"
#include "common/harnessTest.h"
#include "common/harnessLog.h"

#define TEST_LIST_SIZE                                              64

typedef struct TestData
{
    bool selected;
} TestData;

static TestData testList[TEST_LIST_SIZE];

static int testRun = 0;
static int testTotal = 0;
static bool testFirst = true;

static uint64_t timeMSecBegin;

static const char *testExeData = NULL;
static const char *testPathData = NULL;
static const char *testRepoPathData = NULL;
static const char *testExpectPathData = NULL;

/***********************************************************************************************************************************
Extern functions
***********************************************************************************************************************************/
#ifndef NO_LOG
    void harnessLogInit(void);
    void harnessLogFinal(void);
#endif

/***********************************************************************************************************************************
Get and set the test exe
***********************************************************************************************************************************/
const char *
testExe(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testExeData);
}

void
testExeSet(const char *testExe)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testExe);

        FUNCTION_HARNESS_ASSERT(testExe != NULL);
    FUNCTION_HARNESS_END();

    testExeData = testExe;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get and set the path for the pgbackrest repo
***********************************************************************************************************************************/
const char *
testRepoPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testRepoPathData);
}

void
testRepoPathSet(const char *testRepoPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testRepoPath);

        FUNCTION_HARNESS_ASSERT(testRepoPath != NULL);
    FUNCTION_HARNESS_END();

    testRepoPathData = testRepoPath;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get and set the test path, i.e., the path where this test should write its files
***********************************************************************************************************************************/
const char *
testPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testPathData);
}

void
testPathSet(const char *testPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testPath);

        FUNCTION_HARNESS_ASSERT(testPath != NULL);
    FUNCTION_HARNESS_END();

    testPathData = testPath;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get and set the expect path, i.e., the path where expect logs will be stored by the harnessLog module
***********************************************************************************************************************************/
const char *
testExpectPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testExpectPathData);
}

void
testExpectPathSet(const char *testExpectPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testExpectPath);

        FUNCTION_HARNESS_ASSERT(testExpectPath != NULL);
    FUNCTION_HARNESS_END();

    testExpectPathData = testExpectPath;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get the time in milliseconds
***********************************************************************************************************************************/
uint64_t
testTimeMSec(void)
{
    FUNCTION_HARNESS_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_HARNESS_RESULT(UINT64, ((uint64_t)currentTime.tv_sec * 1000) + (uint64_t)currentTime.tv_usec / 1000);
}

/***********************************************************************************************************************************
Get time at beginning of current run
***********************************************************************************************************************************/
uint64_t
testTimeMSecBegin(void)
{
    FUNCTION_HARNESS_VOID();

    FUNCTION_HARNESS_RESULT(UINT64, timeMSecBegin);
}

/***********************************************************************************************************************************
testAdd - add a new test
***********************************************************************************************************************************/
void
testAdd(int run, bool selected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, run);
        FUNCTION_HARNESS_PARAM(BOOL, selected);
    FUNCTION_HARNESS_END();

    if (run != testTotal + 1)
    {
        fprintf(stderr, "ERROR: test run %d is not in order\n", run);
        fflush(stderr);
        exit(255);
    }

    testList[testTotal].selected = selected;
    testTotal++;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
testBegin - should this test run?
***********************************************************************************************************************************/
bool
testBegin(const char *name)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, name);

        FUNCTION_HARNESS_ASSERT(name != NULL);
    FUNCTION_HARNESS_END();

    bool result = false;
    testRun++;

    if (testList[testRun - 1].selected)
    {
#ifndef NO_LOG
        if (!testFirst)
        {
            // Make sure there is nothing untested left in the log
            harnessLogFinal();

            // Clear out the test directory so the next test starts clean
            char buffer[2048];
            snprintf(buffer, sizeof(buffer), "sudo rm -rf %s/" "*", testPath());

            if (system(buffer) != 0)
            {
                fprintf(stderr, "ERROR: unable to clear test path '%s'\n", testPath());
                fflush(stderr);
                exit(255);
            }
        }
#endif
        // No longer the first test
        testFirst = false;

        if (testRun != 1)
            printf("\n");

        printf("run %03d - %s\n", testRun, name);
        fflush(stdout);

        timeMSecBegin = testTimeMSec();

#ifndef NO_LOG
        // Initialize logging
        harnessLogInit();
#endif

        result = true;
    }

    FUNCTION_HARNESS_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
testComplete - make sure all expected tests ran
***********************************************************************************************************************************/
void
testComplete(void)
{
    FUNCTION_HARNESS_VOID();

#ifndef NO_LOG
    // Make sure there is nothing untested left in the log
    harnessLogFinal();
#endif

    // Check that all tests ran
    if (testRun != testTotal)
    {
        fprintf(stderr, "ERROR: expected %d tests but %d were run\n", testTotal, testRun);
        fflush(stderr);
        exit(255);
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
