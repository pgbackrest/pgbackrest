***********************************************************************************************************************************
Test Doxygen documentation.

Portions of this test file (opening and comparing files) were borrowed from the equivalent log file tests.
***********************************************************************************************************************************/



/***********************************************************************************************************************************
Open a test file
***********************************************************************************************************************************/
static int
testFileOpen(const char *logFile, int flags, int mode)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
    FUNCTION_HARNESS_PARAM(INT, flags);
    FUNCTION_HARNESS_PARAM(INT, mode);

    FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_END();

    int result = open(logFile, flags, mode);

    THROW_ON_SYS_ERROR_FMT(result == -1, FileOpenError, "unable to open log file '%s'", logFile);

    FUNCTION_HARNESS_RETURN(INT, result);
}


/***********************************************************************************************************************************
Load file contents into a buffer
***********************************************************************************************************************************/
static void
testFileLoad(const char *fileName, char *buffer, size_t bufferSize)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
    FUNCTION_HARNESS_PARAM_P(CHARDATA, buffer);
    FUNCTION_HARNESS_PARAM(SIZE, bufferSize);

    FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_ASSERT(buffer != NULL);
    FUNCTION_HARNESS_END();

    buffer[0] = 0;

    int fd = testFileOpen(fileName, O_RDONLY, 0);

    size_t totalBytes = 0;
    ssize_t actualBytes = 0;

    do
    {
        THROW_ON_SYS_ERROR_FMT(
                (actualBytes = read(fd, buffer, bufferSize - totalBytes)) == -1, FileOpenError, "unable to read log file '%s'",
                logFile);

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    THROW_ON_SYS_ERROR_FMT(close(fd) == -1, FileOpenError, "unable to close log file '%s'", logFile);

    // Remove final linefeed
    buffer[totalBytes - 1] = 0;

    FUNCTION_HARNESS_RETURN_VOID();
}


/***********************************************************************************************************************************
Write the string to a test file
***********************************************************************************************************************************/
static void writeToTestFile (const char *contents)
{
    HRN_STORAGE_PUT_Z(storageTest???, "src/doc/doxygen/doxygen_test.c", contents);
);

/***********************************************************************************************************************************
Run the doxygen filter on a string and produce an output file.
***********************************************************************************************************************************/





***********************************************************************************************************************************
Compare file contents to a static string
***********************************************************************************************************************************/
static void
testFileResult(const char *fileName, const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
    FUNCTION_HARNESS_PARAM(STRINGZ, expected);

    FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_ASSERT(expected != NULL);
    FUNCTION_HARNESS_END();

    char actual[32768];
    testFileLoad(fileName, actual, sizeof(actual));

    if (strcmp(actual, expected) != 0)                                                          // {uncoverable_branch}
        THROW_FMT(                                                                              // {+uncovered}
                AssertError, "\n\nexpected log:\n\n%s\n\nbut actual log was:\n\n%s\n\n", expected, actual);

    FUNCTION_HARNESS_RETURN_VOID();
}



/***********************************************************************************************************************************
Given a string of data, compare doxygen's output with the expected output.
***********************************************************************************************************************************/






/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    /*******************************************************************************************************************************
    * Package a script based doxy_filter test into the framework. This script is temporary and will be rewritten.
    *******************************************************************************************************************************/
    if (testBegin("doxy_filter"))
    {

    }
        // Run the doxy_filter script.
        TEST_RESULT_BOOL(timeMSec() > (TimeMSec)1483228800000, true, "lower range check");
        TEST_RESULT_BOOL(timeMSec() < (TimeMSec)4102444800000, true, "upper range check");
    }
