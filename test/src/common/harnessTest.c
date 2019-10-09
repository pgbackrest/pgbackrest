/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

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

static char testUserData[64];
static char testGroupData[64];

/***********************************************************************************************************************************
Extern functions
***********************************************************************************************************************************/
#ifndef NO_LOG
    void harnessLogInit(void);
    void harnessLogFinal(void);
#endif

/***********************************************************************************************************************************
Is this test running in a container? i.e., can we use sudo and system paths with impunity?
***********************************************************************************************************************************/
static bool testContainerData = false;

bool
testContainer(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(BOOL, testContainerData);
}

void
testContainerSet(bool testContainer)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(BOOL, testContainer);
    FUNCTION_HARNESS_END();

    testContainerData = testContainer;

    FUNCTION_HARNESS_RESULT_VOID();
}

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
Get and set the project exe
***********************************************************************************************************************************/
static const char *testProjectExeData = NULL;

const char *
testProjectExe(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testProjectExeData);
}

void
testProjectExeSet(const char *testProjectExe)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testProjectExe);
    FUNCTION_HARNESS_END();

    testProjectExeData = testProjectExe;

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

/**********************************************************************************************************************************/
static const char *testDataPathData = NULL;

const char *
testDataPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(STRINGZ, testDataPathData);
}

void
testDataPathSet(const char *testDataPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, testDataPath);
    FUNCTION_HARNESS_END();

    testDataPathData = testDataPath;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get and set scale for performance testing
***********************************************************************************************************************************/
static uint64_t testScaleData = 1;

uint64_t
testScale(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RESULT(UINT64, testScaleData);
}

void
testScaleSet(uint64_t testScale)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(UINT64, testScale);
    FUNCTION_HARNESS_END();

    testScaleData = testScale;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Get test user/group
***********************************************************************************************************************************/
const char *
testUser(void)
{
    return testUserData;
}

const char *
testGroup(void)
{
    return testGroupData;
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
        if (testFirst)
        {
            // Set test user
            const char *testUserTemp = getpwuid(getuid())->pw_name;

            if (strlen(testUserTemp) > sizeof(testUserData) - 1)
                THROW_FMT(AssertError, "test user name must be less than %zu characters", sizeof(testUserData) - 1);

            strcpy(testUserData, testUserTemp);

            // Set test group
            const char *testGroupTemp = getgrgid(getgid())->gr_name;

            if (strlen(testGroupTemp) > sizeof(testGroupData) - 1)
                THROW_FMT(AssertError, "test group name must be less than %zu characters", sizeof(testGroupData) - 1);

            strcpy(testGroupData, testGroupTemp);
        }

#ifndef NO_LOG
        if (!testFirst)
        {
            // Make sure there is nothing untested left in the log
            harnessLogFinal();

            // Clear out the test directory so the next test starts clean
            char buffer[2048];
            snprintf(buffer, sizeof(buffer), "%srm -rf %s/" "*", testContainer() ? "sudo " : "", testPath());

            if (system(buffer) != 0)
            {
                fprintf(stderr, "ERROR: unable to clear test path '%s'\n", testPath());
                fflush(stderr);
                exit(255);
            }

            // Clear out the data directory so the next test starts clean
            snprintf(buffer, sizeof(buffer), "%srm -rf %s/" "*", testContainer() ? "sudo " : "", testDataPath());

            if (system(buffer) != 0)
            {
                fprintf(stderr, "ERROR: unable to clear data path '%s'\n", testDataPath());
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

/***********************************************************************************************************************************
Replace a substring with another string
***********************************************************************************************************************************/
static void
hrnReplaceStr(char *string, size_t bufferSize, const char *substring, const char *replace)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, string);
        FUNCTION_HARNESS_PARAM(SIZE, bufferSize);
        FUNCTION_HARNESS_PARAM(STRINGZ, substring);
        FUNCTION_HARNESS_PARAM(STRINGZ, replace);
    FUNCTION_HARNESS_END();

    ASSERT(string != NULL);
    ASSERT(substring != NULL);

    // Find substring
    char *begin = strstr(string, substring);

    while (begin != NULL)
    {
        // Find end of substring and calculate replace size difference
        char *end = begin + strlen(substring);
        int diff = (int)strlen(replace) - (int)strlen(substring);

        // Make sure we won't overflow the buffer
        CHECK((size_t)((int)strlen(string) + diff) < bufferSize - 1);

        // Move data from end of string enough to make room for the replacement and copy replacement
        memmove(end + diff, end, strlen(end) + 1);
        memcpy(begin, replace, strlen(replace));

        // Find next substring
        begin = strstr(begin + strlen(replace), substring);
    }

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
char harnessReplaceKeyBuffer[256 * 1024];

const char *
hrnReplaceKey(const char *string)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, string);
    FUNCTION_HARNESS_END();

    ASSERT(string != NULL);

    // Make sure we won't overflow the buffer
    ASSERT(strlen(string) < sizeof(harnessReplaceKeyBuffer) - 1);

    strcpy(harnessReplaceKeyBuffer, string);
    hrnReplaceStr(harnessReplaceKeyBuffer, sizeof(harnessReplaceKeyBuffer), "{[path]}", testPath());
    hrnReplaceStr(harnessReplaceKeyBuffer, sizeof(harnessReplaceKeyBuffer), "{[path-data]}", testDataPath());
    hrnReplaceStr(harnessReplaceKeyBuffer, sizeof(harnessReplaceKeyBuffer), "{[user]}", testUser());
    hrnReplaceStr(harnessReplaceKeyBuffer, sizeof(harnessReplaceKeyBuffer), "{[group]}", testGroup());
    hrnReplaceStr(harnessReplaceKeyBuffer, sizeof(harnessReplaceKeyBuffer), "{[project-exe]}", testProjectExe());

    FUNCTION_HARNESS_RESULT(STRINGZ, harnessReplaceKeyBuffer);
}

/**********************************************************************************************************************************/
void
hrnFileRead(const char *fileName, unsigned char *buffer, size_t bufferSize)
{
    int result = open(fileName, O_RDONLY, 0660);

    if (result == -1)
    {
        fprintf(stderr, "ERROR: unable to open '%s' for read\n", fileName);
        fflush(stderr);
        exit(255);
    }

    ssize_t bufferRead = read(result, buffer, bufferSize);

    if (bufferRead == -1)
    {
        fprintf(stderr, "ERROR: unable to read '%s'\n", fileName);
        fflush(stderr);
        exit(255);
    }

    buffer[bufferRead] = 0;

    close(result);
}

/**********************************************************************************************************************************/
void
hrnFileWrite(const char *fileName, const unsigned char *buffer, size_t bufferSize)
{
    int result = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0660);

    if (result == -1)
    {
        fprintf(stderr, "ERROR: unable to open '%s' for write\n", fileName);
        fflush(stderr);
        exit(255);
    }

    if (write(result, buffer, bufferSize) != (int)bufferSize)
    {
        fprintf(stderr, "ERROR: unable to write '%s'\n", fileName);
        fflush(stderr);
        exit(255);
    }

    close(result);
}

/**********************************************************************************************************************************/
char harnessDiffBuffer[256 * 1024];

const char *
hrnDiff(const char *actual, const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, actual);
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);
    FUNCTION_HARNESS_END();

    ASSERT(actual != NULL);

    // Write actual file
    char actualFile[1024];
    snprintf(actualFile, sizeof(actualFile), "%s/diff.actual", testDataPath());
    hrnFileWrite(actualFile, (unsigned char *)actual, strlen(actual));

    // Write expected file
    char expectedFile[1024];
    snprintf(expectedFile, sizeof(expectedFile), "%s/diff.expected", testDataPath());
    hrnFileWrite(expectedFile, (unsigned char *)expected, strlen(expected));

    // Perform diff
    char command[2560];
    snprintf(command, sizeof(command), "diff -u %s %s > %s/diff.result", actualFile, expectedFile, testDataPath());

    if (system(command) == 2)
    {
        fprintf(stderr, "ERROR: unable to execute '%s'\n", command);
        fflush(stderr);
        exit(255);
    }

    // Read result
    char resultFile[1024];
    snprintf(resultFile, sizeof(resultFile), "%s/diff.result", testDataPath());
    hrnFileRead(resultFile, (unsigned char *)harnessDiffBuffer, sizeof(harnessDiffBuffer));

    // Remove last linefeed from diff output
    harnessDiffBuffer[strlen(harnessDiffBuffer) - 1] = 0;

    FUNCTION_HARNESS_RESULT(STRINGZ, harnessDiffBuffer);
}
