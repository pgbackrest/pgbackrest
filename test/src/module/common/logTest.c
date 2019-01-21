/***********************************************************************************************************************************
Test Log Handler
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include <common/regExp.h>

/***********************************************************************************************************************************
Open a log file
***********************************************************************************************************************************/
static int
testLogOpen(const char *logFile, int flags, int mode)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
        FUNCTION_HARNESS_PARAM(INT, flags);
        FUNCTION_HARNESS_PARAM(INT, mode);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_END();

    int result = open(logFile, flags, mode);

    if (result == -1)                                                                           // {uncovered - no errors in test}
        THROW_SYS_ERROR_FMT(FileOpenError, "unable to open log file '%s'", logFile);            // {+uncovered}

    FUNCTION_HARNESS_RESULT(INT, result);
}

/***********************************************************************************************************************************
Load log result from file into a buffer
***********************************************************************************************************************************/
static void
testLogLoad(const char *logFile, char *buffer, size_t bufferSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
        FUNCTION_HARNESS_PARAM(CHARP, buffer);
        FUNCTION_HARNESS_PARAM(SIZE, bufferSize);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
        FUNCTION_HARNESS_ASSERT(buffer != NULL);
    FUNCTION_HARNESS_END();

    buffer[0] = 0;

    int handle = testLogOpen(logFile, O_RDONLY, 0);

    size_t totalBytes = 0;
    ssize_t actualBytes = 0;

    do
    {
        actualBytes = read(handle, buffer, bufferSize - totalBytes);

        if (actualBytes == -1)                                                                  // {uncovered - no errors in test}
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to read log file '%s'", logFile);        // {+uncovered}

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    if (close(handle) == -1)                                                                    // {uncovered - no errors in test}
        THROW_SYS_ERROR_FMT(FileOpenError, "unable to close log file '%s'", logFile);           // {+uncovered}

    // Remove final linefeed
    buffer[totalBytes - 1] = 0;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Compare log to a static string
***********************************************************************************************************************************/
void
testLogResult(const char *logFile, const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
        FUNCTION_HARNESS_ASSERT(expected != NULL);
    FUNCTION_HARNESS_END();

    char actual[32768];
    testLogLoad(logFile, actual, sizeof(actual));

    if (strcmp(actual, expected) != 0)                                                          // {uncovered - no errors in test}
        THROW_FMT(                                                                              // {+uncovered}
            AssertError, "\n\nexpected log:\n\n%s\n\nbut actual log was:\n\n%s\n\n", expected, actual);

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("logLevelEnum() and logLevelStr()"))
    {
        TEST_ERROR(logLevelEnum(BOGUS_STR), AssertError, "log level 'BOGUS' not found");
        TEST_RESULT_INT(logLevelEnum("OFF"), logLevelOff, "log level 'OFF' found");
        TEST_RESULT_INT(logLevelEnum("info"), logLevelInfo, "log level 'info' found");
        TEST_RESULT_INT(logLevelEnum("TRACE"), logLevelTrace, "log level 'TRACE' found");

        TEST_ERROR(logLevelStr(999), AssertError, "assertion 'logLevel <= LOG_LEVEL_MAX' failed");
        TEST_RESULT_STR(logLevelStr(logLevelOff), "OFF", "log level 'OFF' found");
        TEST_RESULT_STR(logLevelStr(logLevelInfo), "INFO", "log level 'INFO' found");
        TEST_RESULT_STR(logLevelStr(logLevelTrace), "TRACE", "log level 'TRACE' found");
    }

    // *****************************************************************************************************************************
    if (testBegin("logInit()"))
    {
        TEST_RESULT_INT(logLevelStdOut, logLevelError, "console logging is error");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
        TEST_RESULT_INT(logLevelFile, logLevelOff, "file logging is off");
        TEST_RESULT_VOID(logInit(logLevelInfo, logLevelWarn, logLevelError, true), "init logging");
        TEST_RESULT_INT(logLevelStdOut, logLevelInfo, "console logging is info");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");
        TEST_RESULT_INT(logLevelFile, logLevelError, "file logging is error");
    }

    // *****************************************************************************************************************************
    if (testBegin("logWill*()"))
    {
        logLevelStdOut = logLevelOff;
        logLevelStdErr = logLevelOff;
        logLevelFile = logLevelOff;
        logHandleFile = -1;
        TEST_RESULT_BOOL(logWill(logLevelError), false, "will not log");
        TEST_RESULT_BOOL(logWillFile(logLevelError), false, "will not log file");
        TEST_RESULT_BOOL(logWillStdErr(logLevelError), false, "will not log stderr");
        TEST_RESULT_BOOL(logWillStdOut(logLevelError), false, "will not log stdout");

        logLevelStdOut = logLevelError;
        TEST_RESULT_BOOL(logWill(logLevelError), true, "will log");
        TEST_RESULT_BOOL(logWillStdOut(logLevelError), true, "will log stdout");

        logLevelStdOut = logLevelOff;
        logLevelStdErr = logLevelError;
        TEST_RESULT_BOOL(logWill(logLevelError), true, "will log");
        TEST_RESULT_BOOL(logWillStdErr(logLevelError), true, "will log stderr");

        logLevelStdErr = logLevelOff;
        logLevelFile = logLevelError;
        TEST_RESULT_BOOL(logWill(logLevelError), false, "will not log");
        TEST_RESULT_BOOL(logWillFile(logLevelError), false, "will not log file");

        logHandleFile = 1;
        TEST_RESULT_BOOL(logWill(logLevelError), true, "will log");
        TEST_RESULT_BOOL(logWillFile(logLevelError), true, "will log file");
        logHandleFile = -1;
    }

    // *****************************************************************************************************************************
    if (testBegin("logWrite()"))
    {
        // Just test the error here -- success is well tested elsewhere
        TEST_ERROR(
            logWrite(-999, "message", 7, "invalid handle"), FileWriteError,
            "unable to write invalid handle: [9] Bad file descriptor");
    }

    // *****************************************************************************************************************************
    if (testBegin("logInternal()"))
    {
        TEST_RESULT_VOID(logInit(logLevelOff, logLevelOff, logLevelOff, false), "init logging to off");
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "file", "function", 0, "format"),
            "message not logged anywhere");

        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, true), "init logging to warn (timestamp on)");
        TEST_RESULT_VOID(logFileSet(BOGUS_STR), "ignore bogus filename because file logging is off");
        TEST_RESULT_VOID(logInternal(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "file", "function", 0, "TEST"), "log timestamp");

        String *logTime = strNewN(logBuffer, 23);
        TEST_RESULT_BOOL(
            regExpMatchOne(
                strNew("^20[0-9]{2}\\-[0-1][0-9]\\-[0-3][0-9] [0-2][0-9]\\:[0-5][0-9]\\:[0-5][0-9]\\.[0-9]{3}$"), logTime),
            true, "check timestamp format: %s", strPtr(logTime));

        // Redirect output to files
        char stdoutFile[1024];
        snprintf(stdoutFile, sizeof(stdoutFile), "%s/stdout.log", testPath());
        logHandleStdOut = testLogOpen(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

        char stderrFile[1024];
        snprintf(stderrFile, sizeof(stderrFile), "%s/stderr.log", testPath());
        logHandleStdErr = testLogOpen(stderrFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, false), "init logging to warn (timestamp off)");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "file", "function", 0, "format %d", 99), "log warn");
        TEST_RESULT_STR(logBuffer, "P00   WARN: format 99\n", "    check log");

        // This won't be logged due to the range
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, logLevelError, logLevelError, "file", "function", 0, "NOT OUTPUT"), "out of range");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "file", "function", 26, "message"), "log error");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [026]: message\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelError, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "file", "function", 26, "message1\nmessage2"),
            "log error with multiple lines");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [026]: message1\nmessage2\n", "    check log");

        TEST_RESULT_VOID(logInit(logLevelDebug, logLevelDebug, logLevelDebug, false), "init logging to debug");

        // Log to file
        char fileFile[1024];
        snprintf(fileFile, sizeof(stdoutFile), "%s/file.log", testPath());
        logFileSet(fileFile);

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelDebug, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "test.c", "test_func", 0, "message\nmessage2"), "log debug");
        TEST_RESULT_STR(logBuffer, "P00  DEBUG:     test::test_func: message\nmessage2\n", "    check log");

        // This won't be logged due to the range
        TEST_RESULT_VOID(
            logInternal(logLevelDebug, logLevelTrace, logLevelTrace, "test.c", "test_func", 0, "NOT OUTPUT"), "out of range");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelTrace, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "test.c", "test_func", 0, "message"), "log debug");
        TEST_RESULT_STR(logBuffer, "P00  TRACE:         test::test_func: message\n", "    check log");

        // Reopen the log file
        TEST_RESULT_BOOL(logFileSet(fileFile), true, "open valid file");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelInfo, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "test.c", "test_func", 0, "info message"), "log info");
        TEST_RESULT_STR(logBuffer, "P00   INFO: info message\n", "    check log");
        TEST_RESULT_VOID(
            logInternal(logLevelInfo, LOG_LEVEL_MIN, LOG_LEVEL_MAX, "test.c", "test_func", 0, "info message 2"), "log info");
        TEST_RESULT_STR(logBuffer, "P00   INFO: info message 2\n", "    check log");

        // Reopen invalid log file
        TEST_RESULT_BOOL(logFileSet("/" BOGUS_STR), false, "attempt to open bogus file");

        // Check stdout
        testLogResult(
            stdoutFile,
            "P00   WARN: format 99\n"
            "P00  ERROR: [026]: message\n"
            "P00  ERROR: [026]: message1\n"
            "            message2");

        // Check stderr
        testLogResult(
            stderrFile,
            "DEBUG:     test::test_func: message\n"
            "           message2\n"
            "INFO: info message\n"
            "INFO: info message 2\n"
            "WARN: unable to open log file '/BOGUS': Permission denied\n"
            "      NOTE: process will continue without log file.");

        // Check file
        testLogResult(
            fileFile,
            "-------------------PROCESS START-------------------\n"
            "P00  DEBUG:     test::test_func: message\n"
            "                message2\n"
            "\n"
            "-------------------PROCESS START-------------------\n"
            "P00   INFO: info message\n"
            "P00   INFO: info message 2");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
