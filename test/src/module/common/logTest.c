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
void testRun()
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
        TEST_RESULT_INT(logLevelStdOut, logLevelOff, "console logging is off");
        TEST_RESULT_INT(logLevelStdErr, logLevelOff, "stderr logging is off");
        TEST_RESULT_VOID(logInit(logLevelInfo, logLevelError, true), "init logging");
        TEST_RESULT_INT(logLevelStdOut, logLevelInfo, "console logging is info");
        TEST_RESULT_INT(logLevelStdErr, logLevelError, "stderr logging is error");
    }

    // *****************************************************************************************************************************
    if (testBegin("logInternal()"))
    {
        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, true), "init logging to warn (timestamp on)");
        TEST_RESULT_VOID(logInternal(logLevelWarn, NULL, NULL, 0, 0, "TEST"), "log timestamp");

        String *logTime = strNewN(logBuffer, 23);
        TEST_RESULT_BOOL(
            regExpMatchOne(
                strNew("^20[0-9]{2}\\-[0-1][0-9]\\-[0-3][0-9] [0-2][0-9]\\:[0-5][0-9]\\:[0-5][0-9]\\.[0-9]{3}$"), logTime),
            true, "check timestamp format: %s", strPtr(logTime));

        // Redirect output to files
        TEST_RESULT_VOID(logInit(logLevelWarn, logLevelOff, false), "init logging to warn (timestamp off)");

        String *stdoutFile = strNewFmt("%s/stdout.log", testPath());
        String *stderrFile = strNewFmt("%s/stderr.log", testPath());

        logHandleStdOut = open(strPtr(stdoutFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);
        logHandleStdErr = open(strPtr(stderrFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelWarn, NULL, NULL, 0, 0, "format %d", 99), "log warn");
        TEST_RESULT_STR(logBuffer, "P00   WARN: format 99\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, NULL, NULL, 0, 25, "message"), "log error");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [025]: message\n", "    check log");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, NULL, NULL, 0, 25, "message1\nmessage2"), "log error with multiple lines");
        TEST_RESULT_STR(logBuffer, "P00  ERROR: [025]: message1\n            message2\n", "    check log");

        TEST_RESULT_VOID(logInit(logLevelDebug, logLevelDebug, false), "init logging to debug");

        logBuffer[0] = 0;
        TEST_RESULT_VOID(logInternal(logLevelError, "test.c", "test_func", 33, 25, "message"), "log error");
        TEST_RESULT_STR(logBuffer, "ERROR: [025]: test.c:test_func():33: message\n", "    check log");

        // Check stdout
        Storage *storage = storageNew(strNew(testPath()), 0750, 65536, NULL);

        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGet(storage, stdoutFile, false))),
            "P00   WARN: format 99\n"
            "P00  ERROR: [025]: message\n"
            "P00  ERROR: [025]: message1\n"
            "            message2\n",
            "checkout stdout output");

        // Check stderr
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGet(storage, stderrFile, false))),
            "ERROR: [025]: test.c:test_func():33: message\n",
            "checkout stderr output");
    }
}
