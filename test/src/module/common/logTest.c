/***********************************************************************************************************************************
Test Log Handler
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include <common/regExp.h>
#include <storage/storage.h>

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("logLevelEnum() and logLevelStr()"))
    {
        TEST_ERROR(logLevelEnum(BOGUS_STR), AssertError, "log level 'BOGUS' not found");
        TEST_RESULT_INT(logLevelEnum("OFF"), logLevelOff, "log level 'OFF' found");
        TEST_RESULT_INT(logLevelEnum("info"), logLevelInfo, "log level 'info' found");
        TEST_RESULT_INT(logLevelEnum("TRACE"), logLevelTrace, "log level 'TRACE' found");

        TEST_ERROR(logLevelStr(999), AssertError, "invalid log level '999'");
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
    if (testBegin("logInternal()"))
    {
        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, true), "init logging to warn (timestamp on)");
        TEST_RESULT_VOID(logInternal(logLevelWarn, NULL, NULL, 0, "TEST"), "log timestamp");

        String *logTime = strNewN(logBuffer, 23);
        TEST_RESULT_BOOL(
            regExpMatchOne(
                strNew("^20[0-9]{2}\\-[0-1][0-9]\\-[0-3][0-9] [0-2][0-9]\\:[0-5][0-9]\\:[0-5][0-9]\\.[0-9]{3}$"), logTime),
            true, "check timestamp format: %s", strPtr(logTime));

        // Redirect output to files
        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, logLevelOff, false), "init logging to warn (timestamp off)");

        String *stdoutFile = strNewFmt("%s/stdout.log", testPath());
        String *stderrFile = strNewFmt("%s/stderr.log", testPath());
        String *fileFile = strNewFmt("%s/file.log", testPath());

        logHandleStdOut = open(strPtr(stdoutFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);
        logHandleStdErr = open(strPtr(stderrFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelWarn, NULL, NULL, 0, "format %d", 99), "log warn");
        TEST_RESULT_STR(logBuffer, "P00   WARN: format 99\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, NULL, NULL, 26, "message"), "log error");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [026]: message\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, NULL, NULL, 26, "message1\nmessage2"), "log error with multiple lines");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [026]: message1\n            message2\n", "    check log");

        TEST_RESULT_VOID(logInit(logLevelDebug, logLevelDebug, logLevelDebug, false), "init logging to debug");

        // Log to file
        logFileSet(strPtr(fileFile));

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelDebug, "test.c", "test_func", 0, "message"), "log debug");
        TEST_RESULT_STR(logBuffer, "P00  DEBUG: test.c:test_func(): message\n", "    check log");

        // Reopen the log file
        logFileSet(strPtr(fileFile));

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelInfo, "test.c", "test_func", 0, "info message"), "log info");
        TEST_RESULT_STR(logBuffer, "P00   INFO: info message\n", "    check log");

        // Reopen invalid log file
        logFileSet("/" BOGUS_STR);

        // Check stdout
        Storage *storage = storageNewNP(strNew(testPath()));

        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storage, stdoutFile)))),
            "P00   WARN: format 99\n"
            "P00  ERROR: [026]: message\n"
            "P00  ERROR: [026]: message1\n"
            "            message2\n",
            "checkout stdout output");

        // Check stderr
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storage, stderrFile)))),
            "DEBUG: test.c:test_func(): message\n"
            "INFO: info message\n"
            "WARN: unable to open log file '/BOGUS': Permission denied\n"
            "NOTE: process will continue without log file.\n",
            "checkout stderr output");

        // Check file
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storage, fileFile)))),
            "-------------------PROCESS START-------------------\n"
            "P00  DEBUG: test.c:test_func(): message\n"
            "\n"
            "-------------------PROCESS START-------------------\n"
            "P00   INFO: info message\n",
            "checkout file output");
    }
}
