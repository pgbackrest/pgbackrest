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

    THROW_ON_SYS_ERROR_FMT(result == -1, FileOpenError, "unable to open log file '%s'", logFile);

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
        FUNCTION_HARNESS_PARAM_P(CHARDATA, buffer);
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
        THROW_ON_SYS_ERROR_FMT(
            (actualBytes = read(handle, buffer, bufferSize - totalBytes)) == -1, FileOpenError, "unable to read log file '%s'",
            logFile);

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    THROW_ON_SYS_ERROR_FMT(close(handle) == -1, FileOpenError, "unable to close log file '%s'", logFile);

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

    if (strcmp(actual, expected) != 0)                                                          // {uncoverable_branch}
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
        TEST_RESULT_Z(logLevelStr(logLevelOff), "OFF", "log level 'OFF' found");
        TEST_RESULT_Z(logLevelStr(logLevelInfo), "INFO", "log level 'INFO' found");
        TEST_RESULT_Z(logLevelStr(logLevelTrace), "TRACE", "log level 'TRACE' found");
    }

    // *****************************************************************************************************************************
    if (testBegin("logInit()"))
    {
        TEST_RESULT_INT(logLevelStdOut, logLevelError, "console logging is error");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
        TEST_RESULT_INT(logLevelFile, logLevelOff, "file logging is off");

        TEST_RESULT_VOID(logInit(logLevelInfo, logLevelWarn, logLevelError, true, 99, true), "init logging");
        TEST_RESULT_INT(logLevelStdOut, logLevelInfo, "console logging is info");
        TEST_RESULT_INT(logLevelStdErr, logLevelWarn, "stderr logging is warn");
        TEST_RESULT_INT(logLevelFile, logLevelError, "file logging is error");
        TEST_RESULT_INT(logProcessSize, 2, "process field size is 2");
        TEST_RESULT_BOOL(logDryRun, true, "dry run is true");

        TEST_RESULT_VOID(logInit(logLevelInfo, logLevelWarn, logLevelError, true, 100, false), "init logging");
        TEST_RESULT_INT(logProcessSize, 3, "process field size is 3");
        TEST_RESULT_BOOL(logDryRun, false, "dry run is false");
    }

    // *****************************************************************************************************************************
    if (testBegin("logAny*()"))
    {
        logLevelStdOut = logLevelOff;
        logLevelStdErr = logLevelOff;
        logLevelFile = logLevelOff;
        logHandleFile = -1;
        TEST_RESULT_VOID(logAnySet(), "set log any");
        TEST_RESULT_BOOL(logAny(logLevelError), false, "will not log");

        logLevelStdErr = logLevelError;
        TEST_RESULT_VOID(logAnySet(), "set log any");
        TEST_RESULT_BOOL(logAny(logLevelError), true, "will log");

        logLevelFile = logLevelWarn;
        TEST_RESULT_VOID(logAnySet(), "set log any");
        TEST_RESULT_BOOL(logAny(logLevelWarn), false, "will not log");

        logHandleFile = 1;
        TEST_RESULT_VOID(logAnySet(), "set log any");
        TEST_RESULT_BOOL(logAny(logLevelWarn), true, "will log");
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
    if (testBegin("logInternal() and logInternalFmt()"))
    {
        TEST_RESULT_VOID(logInit(logLevelOff, logLevelOff, logLevelOff, false, 1, false), "init logging to off");
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "file", "function", 0, "format"),
            "message not logged anywhere");

        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, true, 1, false), "init logging to warn (timestamp on)");
        TEST_RESULT_VOID(logFileSet(BOGUS_STR), "ignore bogus filename because file logging is off");
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "file", "function", 0, "TEST"), "log timestamp");

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

        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, false, 1, false), "init logging to warn (timestamp off)");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternalFmt(logLevelWarn, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "file", "function", 0, "format %d", 99), "log warn");
        TEST_RESULT_Z(logBuffer, "P00   WARN: format 99\n", "    check log");

        // This won't be logged due to the range
        TEST_RESULT_VOID(
            logInternal(logLevelWarn, logLevelError, logLevelError, 0, "file", "function", 0, "NOT OUTPUT"), "out of range");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelError, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "file", "function", 26, "message"), "log error");
        TEST_RESULT_Z(logBuffer, "P00  ERROR: [026]: message\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelError, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "file", "function", 26, "message1\nmessage2"),
            "log error with multiple lines");
        TEST_RESULT_Z(logBuffer, "P00  ERROR: [026]: message1\nmessage2\n", "    check log");

        TEST_RESULT_VOID(logInit(logLevelDebug, logLevelDebug, logLevelDebug, false, 999, false), "init logging to debug");

        // Log to file
        char fileFile[1024];
        snprintf(fileFile, sizeof(stdoutFile), "%s/file.log", testPath());
        logFileSet(fileFile);

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(
                logLevelDebug, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 999, "test.c", "test_func", 0, "message\nmessage2"), "log debug");
        TEST_RESULT_Z(logBuffer, "P999  DEBUG:     test::test_func: message\nmessage2\n", "    check log");

        // This won't be logged due to the range
        TEST_RESULT_VOID(
            logInternal(logLevelDebug, logLevelTrace, logLevelTrace, 0, "test.c", "test_func", 0, "NOT OUTPUT"), "out of range");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelTrace, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 0, "test.c", "test_func", 0, "message"), "log debug");
        TEST_RESULT_Z(logBuffer, "P000  TRACE:         test::test_func: message\n", "    check log");

        // Reopen the log file
        TEST_RESULT_VOID(
            logInit(logLevelDebug, logLevelDebug, logLevelDebug, false, 99, true), "reduce process-id size and dry-run");
        TEST_RESULT_BOOL(logFileSet(fileFile), true, "open valid file");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(
            logInternal(logLevelInfo, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 1, "test.c", "test_func", 0, "info message"), "log info");
        TEST_RESULT_Z(logBuffer, "P01   INFO: [DRY-RUN] info message\n", "    check log");
        TEST_RESULT_VOID(
            logInternal(logLevelInfo, LOG_LEVEL_MIN, LOG_LEVEL_MAX, 99, "test.c", "test_func", 0, "info message 2"), "log info");
        TEST_RESULT_Z(logBuffer, "P99   INFO: [DRY-RUN] info message 2\n", "    check log");

        // Reopen invalid log file
        TEST_RESULT_BOOL(logFileSet("/" BOGUS_STR), false, "attempt to open bogus file");
        TEST_RESULT_INT(logHandleFile, -1, "log file is closed");

        // Close logging again
        TEST_RESULT_VOID(logInit(logLevelDebug, logLevelDebug, logLevelDebug, false, 99, false), "reduce log size");
        TEST_RESULT_BOOL(logFileSet(fileFile), true, "open valid file");
        TEST_RESULT_BOOL(logHandleFile != -1, true, "log file is open");

        logClose();
        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelOff, "stderr logging is off");
        TEST_RESULT_INT(logLevelFile, logLevelOff, "file logging is off");
        TEST_RESULT_INT(logHandleFile, -1, "log file is closed");

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
            "INFO: [DRY-RUN] info message\n"
            "INFO: [DRY-RUN] info message 2\n"
            "WARN: [DRY-RUN] unable to open log file '/BOGUS': Permission denied\n"
            "      NOTE: process will continue without log file.");

        // Check file
        testLogResult(
            fileFile,
            "-------------------PROCESS START-------------------\n"
            "P999  DEBUG:     test::test_func: message\n"
            "                 message2\n"
            "\n"
            "-------------------PROCESS START-------------------\n"
            "P01   INFO: [DRY-RUN] info message\n"
            "P99   INFO: [DRY-RUN] info message 2");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
