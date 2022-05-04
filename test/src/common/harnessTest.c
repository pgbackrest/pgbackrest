/***********************************************************************************************************************************
C Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
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
static int testRunSub = 0;
static int testTotal = 0;
static bool testFirst = true;

static uint64_t timeMSecBegin;

static const char *testExeData = NULL;
static const char *testProjectExeData = NULL;
static bool testContainerData = false;
static unsigned int testIdxData = 0;
static bool testTiming = true;
static const char *testPathData = NULL;
static const char *testDataPathData = NULL;
static const char *testRepoPathData = NULL;

static struct HarnessTestLocal
{
    uint64_t logLastBeginTime;                                      // Store the begin time of the last log for deltas
    int logLastLineNo;                                              // Store the line number to be used in debugging

    struct HarnessTestResult
    {
        bool running;                                               // Is the test currently running?
        const char *statement;                                      // statement that is being tested
        int lineNo;                                                 // Line number the test is on
        bool result;                                                // Is there a result or is it void?
    } result;
} harnessTestLocal;

/***********************************************************************************************************************************
Extern functions
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_LOG
    void harnessLogInit(void);
    void harnessLogFinal(void);
#endif

/***********************************************************************************************************************************
Initialize harness
***********************************************************************************************************************************/
void
hrnInit(
    const char *testExe, const char *testProjectExe, bool testContainer, unsigned int testIdx, bool timing, const char *testPath,
    const char *testDataPath, const char *testRepoPath)
{
    FUNCTION_HARNESS_VOID();

    // Set test configuration
    testExeData = testExe;
    testProjectExeData = testProjectExe;
    testContainerData = testContainer;
    testIdxData = testIdx;
    testTiming = timing;
    testPathData = testPath;
    testDataPathData = testDataPath;
    testRepoPathData = testRepoPath;

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
testAdd - add a new test
***********************************************************************************************************************************/
void
hrnAdd(int run, bool selected)
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

    FUNCTION_HARNESS_RETURN_VOID();
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
#ifdef HRN_FEATURE_LOG
        if (!testFirst)
        {
            // Make sure there is nothing untested left in the log
            harnessLogFinal();

            // It is possible the test left the cwd in a weird place
            if (chdir(testPath()) != 0)
            {
                fprintf(stderr, "ERROR: unable to chdir to test path '%s'\n", testPath());
                fflush(stderr);
                exit(255);
            }

            // Clear out the test directory so the next test starts clean
            char buffer[2048];
            snprintf(
                buffer, sizeof(buffer), "%schmod -R 700 %s/" "* > /dev/null 2>&1;%srm -rf %s/" "*", testContainer() ? "sudo " : "",
                testPath(), testContainer() ? "sudo " : "", testPath());

            if (system(buffer) != 0)
            {
                fprintf(stderr, "ERROR: unable to clear test path '%s'\n", testPath());
                fflush(stderr);
                exit(255);
            }

            // Clear out the data directory so the next test starts clean
            snprintf(
                buffer, sizeof(buffer), "%schmod -R 700 %s/" "* > /dev/null 2>&1;%srm -rf %s/" "*", testContainer() ? "sudo " : "",
                hrnPath(), testContainer() ? "sudo " : "", hrnPath());

            if (system(buffer) != 0)
            {
                fprintf(stderr, "ERROR: unable to clear data path '%s'\n", hrnPath());
                fflush(stderr);
                exit(255);
            }

            // Clear any log replacements
            hrnLogReplaceClear();
        }
#endif
        // No longer the first test
        testFirst = false;

        if (testRun != 1)
            printf("\n");

        printf("run %d - %s\n", testRun, name);
        fflush(stdout);

        testRunSub = 1;
        timeMSecBegin = testTimeMSec();

#ifdef HRN_FEATURE_LOG
        // Initialize logging
        harnessLogInit();
#endif

        result = true;
    }

    harnessTestLocal.logLastBeginTime = 0;

    FUNCTION_HARNESS_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
testComplete - make sure all expected tests ran
***********************************************************************************************************************************/
void
hrnComplete(void)
{
    FUNCTION_HARNESS_VOID();

#ifdef HRN_FEATURE_LOG
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

    FUNCTION_HARNESS_RETURN_VOID();
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
hrnDiff(const char *expected, const char *actual)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);
        FUNCTION_HARNESS_PARAM(STRINGZ, actual);
    FUNCTION_HARNESS_END();

    ASSERT(actual != NULL);

    // Write expected file
    char expectedFile[1024];
    snprintf(expectedFile, sizeof(expectedFile), "%s/diff.expected", hrnPath());
    hrnFileWrite(expectedFile, (unsigned char *)expected, strlen(expected));

    // Write actual file
    char actualFile[1024];
    snprintf(actualFile, sizeof(actualFile), "%s/diff.actual", hrnPath());
    hrnFileWrite(actualFile, (unsigned char *)actual, strlen(actual));

    // Perform diff
    char command[2560];
    snprintf(command, sizeof(command), "diff -u %s %s > %s/diff.result", expectedFile, actualFile, hrnPath());

    if (system(command) == 2)
    {
        fprintf(stderr, "ERROR: unable to execute '%s'\n", command);
        fflush(stderr);
        exit(255);
    }

    // Read result
    char resultFile[1024];
    snprintf(resultFile, sizeof(resultFile), "%s/diff.result", hrnPath());
    hrnFileRead(resultFile, (unsigned char *)harnessDiffBuffer, sizeof(harnessDiffBuffer));

    // Remove last linefeed from diff output
    harnessDiffBuffer[strlen(harnessDiffBuffer) - 1] = 0;

    FUNCTION_HARNESS_RETURN(STRINGZ, harnessDiffBuffer);
}

/**********************************************************************************************************************************/
void
hrnTestLogTitle(int lineNo)
{
    // Output run/test
    char buffer[32];
    int bufferSize = snprintf(buffer, sizeof(buffer), "%d/%d", testRun, testRunSub);

    printf("\nrun %s ", buffer);

    // Output dashes
    for (int dashIdx = 0; dashIdx < 16 - bufferSize; dashIdx++)
        putchar('-');

    // Output line number
    printf(" L%04d", lineNo);

    // Increment testSub and reset log time
    testRunSub++;
}

/**********************************************************************************************************************************/
void
hrnTestLogPrefix(const int lineNo)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, lineNo);
    FUNCTION_HARNESS_END();

    // Always indent at the beginning
    printf("    ");

    // Add timing if requested
    if (testTiming)
    {
        uint64_t currentTime = testTimeMSec();

        // Print elapsed time size the beginning of the test run
        printf(
            "%03" PRIu64 ".%03" PRIu64"s", ((currentTime - testTimeMSecBegin()) / 1000),
            ((currentTime - testTimeMSecBegin()) % 1000));

        // Print delta time since the last log message
        if (harnessTestLocal.logLastBeginTime != 0)
        {
            printf(
                " %03" PRIu64 ".%03" PRIu64"s ", ((currentTime - harnessTestLocal.logLastBeginTime) / 1000),
                ((currentTime - harnessTestLocal.logLastBeginTime) % 1000));
        }
        else
            printf("          ");

        harnessTestLocal.logLastBeginTime = currentTime;
    }

    // Store line number for
    harnessTestLocal.logLastLineNo = lineNo;

    // Add line number and padding
    printf("L%04d     ", lineNo);
    fflush(stdout);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnTestResultBegin(const char *const statement, const bool result)
{
    ASSERT(!harnessTestLocal.result.running);
    ASSERT(harnessTestLocal.logLastLineNo != 0);

    // Set the line number for the current function in the stack trace
    FUNCTION_HARNESS_STACK_TRACE_LINE_SET(harnessTestLocal.logLastLineNo);

    // Set info to report if an error is thrown
    harnessTestLocal.result =
        (struct HarnessTestResult){
            .running = true, .statement = statement, .lineNo = harnessTestLocal.logLastLineNo, .result = result};

    // Reset line number so it is not used by another test
    harnessTestLocal.logLastLineNo = 0;
}

/**********************************************************************************************************************************/
void
hrnTestResultComment(const char *const comment)
{
    if (comment != NULL)
        printf(" (%s)\n", comment);
    else
        puts("");

    fflush(stdout);
}

/**********************************************************************************************************************************/
bool
hrnTestResultException(void)
{
    FUNCTION_HARNESS_VOID();

    if (harnessTestLocal.result.running)
    {
        THROW_FMT(
#ifdef DEBUG
            TestError,
#else
            AssertError,
#endif
            "EXPECTED %sRESULT FROM STATEMENT: %s\n\nBUT GOT %s: %s\n\nTHROWN AT:\n%s",
            harnessTestLocal.result.result ? "" : "VOID ",
            harnessTestLocal.result.statement, errorName(), errorMessage(), errorStackTrace());
    }

    FUNCTION_HARNESS_RETURN(BOOL, false);
}

void
hrnTestResultEnd(void)
{
    ASSERT(harnessTestLocal.result.running);

    // Set the line number for the current function back to unknown
    FUNCTION_HARNESS_STACK_TRACE_LINE_SET(0);

    harnessTestLocal.result.running = false;
}

/**********************************************************************************************************************************/
static void hrnTestResultDiff(const char *actual, const char *expected)
{
    if (actual != NULL && expected != NULL && (strstr(actual, "\n") != NULL || strstr(expected, "\n") != NULL))
    {
        THROW_FMT(
#ifdef DEBUG
            TestError,
#else
            AssertError,
#endif
            "STATEMENT: %s\n\nRESULT IS:\n%s\n\nBUT DIFF FROM EXPECTED IS (- remove from expected, + add to expected):\n%s\n\n",
            harnessTestLocal.result.statement, actual, hrnDiff(expected, actual));
    }
    else
    {
        THROW_FMT(
#ifdef DEBUG
            TestError,
#else
            AssertError,
#endif
            "STATEMENT: %s\n\nRESULT IS:\n%s\n\nBUT EXPECTED:\n%s",
            harnessTestLocal.result.statement, actual == NULL ? "NULL" : actual, expected == NULL ? "NULL" : expected);                                                 \
    }
}

void
hrnTestResultBool(int actual, int expected)
{
    ASSERT(harnessTestLocal.result.running);

    if (actual < 0 || actual > 1 || expected < 0 || expected > 1 || actual != expected)
    {
        char actualZ[256];
        char expectedZ[256];

        if (actual < 0 || actual > 1)
            snprintf(actualZ, sizeof(actualZ), "INVALID(%d)", actual);
        else
            actual ? strcpy(actualZ, "true") : strcpy(actualZ, "false");

        if (expected < 0 || expected > 1)
            snprintf(expectedZ, sizeof(expectedZ), "INVALID(%d)", expected);
        else
            expected ? strcpy(expectedZ, "true") : strcpy(expectedZ, "false");

        hrnTestResultDiff(actualZ, expectedZ);
    }

    hrnTestResultEnd();
}

void
hrnTestResultDouble(double actual, double expected)
{
    ASSERT(harnessTestLocal.result.running);

    if (actual != expected)
    {
        char actualZ[256];
        char expectedZ[256];

        snprintf(actualZ, sizeof(actualZ), "%f", actual);
        snprintf(expectedZ, sizeof(expectedZ), "%f", expected);

        hrnTestResultDiff(actualZ, expectedZ);
    }

    hrnTestResultEnd();
}

void
hrnTestResultInt64(int64_t actual, int64_t expected, HarnessTestResultOperation operation)
{
    ASSERT(harnessTestLocal.result.running);

    bool result = false;

    switch (operation)
    {
        case harnessTestResultOperationEq:
            result = actual == expected;
            break;

        case harnessTestResultOperationNe:
            result = actual != expected;
            break;
    }

    if (!result)
    {
        char actualZ[256];
        char expectedZ[256];

        snprintf(actualZ, sizeof(actualZ), "%" PRId64, actual);
        snprintf(expectedZ, sizeof(expectedZ), "%" PRId64, expected);

        hrnTestResultDiff(actualZ, expectedZ);
    }

    hrnTestResultEnd();
}

void
hrnTestResultPtr(const void *actual, const void *expected, HarnessTestResultOperation operation)
{
    ASSERT(harnessTestLocal.result.running);

    bool result = false;

    switch (operation)
    {
        case harnessTestResultOperationEq:
            result = actual == expected;
            break;

        case harnessTestResultOperationNe:
            result = actual != expected;
            break;
    }

    if (!result)
    {
        char actualZ[256];
        char expectedZ[256];

        snprintf(actualZ, sizeof(actualZ), "%p", actual);
        snprintf(expectedZ, sizeof(expectedZ), "%p", expected);

        hrnTestResultDiff(actualZ, expectedZ);
    }

    hrnTestResultEnd();
}

#ifdef HRN_FEATURE_STRING

void
hrnTestResultStringList(const StringList *actual, const char *expected, HarnessTestResultOperation operation)
{
    // Return NULL if list is empty
    if (strLstEmpty(actual))
    {
        hrnTestResultZ(NULL, expected, operation);
        return;
    }

    hrnTestResultZ(strZ(strCatZ(strLstJoin(actual, "\n"), "\n")), expected, operation);
}

#endif

void
hrnTestResultUInt64(uint64_t actual, uint64_t expected, HarnessTestResultOperation operation)
{
    ASSERT(harnessTestLocal.result.running);

    bool result = false;

    switch (operation)
    {
        case harnessTestResultOperationEq:
            result = actual == expected;
            break;

        case harnessTestResultOperationNe:
            result = actual != expected;
            break;
    }

    if (!result)
    {
        char actualZ[256];
        char expectedZ[256];

        snprintf(actualZ, sizeof(actualZ), "%" PRIu64, actual);
        snprintf(expectedZ, sizeof(expectedZ), "%" PRIu64, expected);

        hrnTestResultDiff(actualZ, expectedZ);
    }

    hrnTestResultEnd();
}

void
hrnTestResultUInt64Int64(uint64_t actual, int64_t expected, HarnessTestResultOperation operation)
{
    ASSERT(harnessTestLocal.result.running);

    if (actual <= INT64_MAX && expected >= 0)
        hrnTestResultUInt64(actual, (uint64_t)expected, operation);
    else
    {
        char actualZ[256];
        char expectedZ[256];

        snprintf(actualZ, sizeof(actualZ), "%" PRIu64, actual);
        snprintf(expectedZ, sizeof(expectedZ), "%" PRId64, expected);

        hrnTestResultDiff(actualZ, expectedZ);
    }
}

void
hrnTestResultZ(const char *actual, const char *expected, HarnessTestResultOperation operation)
{
    ASSERT(harnessTestLocal.result.running);

    bool result = false;

    switch (operation)
    {
        case harnessTestResultOperationEq:
            result = (actual == NULL && expected == NULL) || (actual != NULL && expected != NULL && strcmp(actual, expected) == 0);
            break;

        case harnessTestResultOperationNe:
            result =
                (actual == NULL && expected != NULL) || (actual != NULL && expected == NULL) ||
                (actual != NULL && expected != NULL && strcmp(actual, expected) == 0);
            break;
    }

    if (!result)
        hrnTestResultDiff(actual, expected);

    hrnTestResultEnd();
}

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
const char *
testExe(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRINGZ, testExeData);
}

/**********************************************************************************************************************************/
const char *
testProjectExe(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRINGZ, testProjectExeData);
}

/**********************************************************************************************************************************/
bool
testContainer(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(BOOL, testContainerData);
}

/**********************************************************************************************************************************/
unsigned int
testIdx(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(UINT, testIdxData);
}

/**********************************************************************************************************************************/
const char *
testPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRINGZ, testPathData);
}

/**********************************************************************************************************************************/
const char *
hrnPath(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRINGZ, testDataPathData);
}

/**********************************************************************************************************************************/
const char *
hrnPathRepo(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRINGZ, testRepoPathData);
}

/**********************************************************************************************************************************/
uint64_t
testTimeMSec(void)
{
    FUNCTION_HARNESS_VOID();

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    FUNCTION_HARNESS_RETURN(UINT64, ((uint64_t)currentTime.tv_sec * 1000) + (uint64_t)currentTime.tv_usec / 1000);
}

/**********************************************************************************************************************************/
uint64_t
testTimeMSecBegin(void)
{
    FUNCTION_HARNESS_VOID();

    FUNCTION_HARNESS_RETURN(UINT64, timeMSecBegin);
}
