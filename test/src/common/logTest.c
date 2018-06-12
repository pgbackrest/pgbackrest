/***********************************************************************************************************************************
Log Test Harness
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

#include "common/log.h"

#include "common/harnessDebug.h"
#include "common/harnessTest.h"

#ifndef NO_LOG

/***********************************************************************************************************************************
Has the log harness been init'd?
***********************************************************************************************************************************/
static bool harnessLogInit = false;

/***********************************************************************************************************************************
Name of file where logs are stored for testing
***********************************************************************************************************************************/
static char stdoutFile[1024];
static char stderrFile[1024];

/***********************************************************************************************************************************
Buffer where log results are loaded for comparison purposes
***********************************************************************************************************************************/
char harnessLogBuffer[256 * 1024];

/***********************************************************************************************************************************
Open a log file -- centralized here for error handling
***********************************************************************************************************************************/
static int
harnessLogOpen(const char *logFile, int flags, int mode)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);
        FUNCTION_HARNESS_PARAM(INT, flags);
        FUNCTION_HARNESS_PARAM(INT, mode);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_END();

    int result = open(logFile, flags, mode);

    if (result == -1)
        THROW_SYS_ERROR_FMT(FileOpenError, "unable to open log file '%s'", logFile);

    FUNCTION_HARNESS_RESULT(INT, result);
}

/***********************************************************************************************************************************
Initialize log for testing
***********************************************************************************************************************************/
void
testLogInit()
{
    FUNCTION_HARNESS_VOID();

    if (!harnessLogInit)
    {
        logInit(logLevelInfo, logLevelOff, logLevelOff, false);

        snprintf(stdoutFile, sizeof(stdoutFile), "%s/stdout.log", testPath());
        logHandleStdOut = harnessLogOpen(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

        snprintf(stderrFile, sizeof(stderrFile), "%s/stderr.log", testPath());
        logHandleStdErr = harnessLogOpen(stderrFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

        harnessLogInit = true;
    }

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Load log result from file into the log buffer
***********************************************************************************************************************************/
void
harnessLogLoad(const char *logFile)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_END();

    harnessLogBuffer[0] = 0;

    int handle = harnessLogOpen(logFile, O_RDONLY, 0);

    size_t totalBytes = 0;
    ssize_t actualBytes = 0;

    do
    {
        actualBytes = read(handle, harnessLogBuffer, sizeof(harnessLogBuffer) - totalBytes);

        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to read log file '%s'", logFile);

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    if (close(handle) == -1)
        THROW_SYS_ERROR_FMT(FileOpenError, "unable to close log file '%s'", logFile);

    // Remove final linefeed
    harnessLogBuffer[totalBytes - 1] = 0;

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Compare log to a static string

After the comparison the log is cleared so the next result can be compared.
***********************************************************************************************************************************/
void
testLogResult(const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);

        FUNCTION_HARNESS_ASSERT(expected != NULL);
    FUNCTION_HARNESS_END();

    harnessLogLoad(stdoutFile);

    if (strcmp(harnessLogBuffer, expected) != 0)
        THROW_FMT(AssertError, "\n\nexpected log:\n\n%s\n\nbut actual log was:\n\n%s\n\n", expected, harnessLogBuffer);

    close(logHandleStdOut);
    logHandleStdOut = harnessLogOpen(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Compare log to a regexp

After the comparison the log is cleared so the next result can be compared.
***********************************************************************************************************************************/
void
testLogResultRegExp(const char *expression)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expression);

        FUNCTION_HARNESS_ASSERT(expression != NULL);
    FUNCTION_HARNESS_END();

    regex_t regExp;

    TRY_BEGIN()
    {
        harnessLogLoad(stdoutFile);

        // Compile the regexp and process errors
        int result = 0;

        if ((result = regcomp(&regExp, expression, REG_NOSUB | REG_EXTENDED)) != 0)
        {
            char buffer[4096];
            regerror(result, NULL, buffer, sizeof(buffer));
            THROW(FormatError, buffer);
        }

        // Do the match
        if (regexec(&regExp, harnessLogBuffer, 0, NULL, 0) != 0)
            THROW_FMT(AssertError, "\n\nexpected log regexp:\n\n%s\n\nbut actual log was:\n\n%s\n\n", expression, harnessLogBuffer);

        close(logHandleStdOut);
        logHandleStdOut = harnessLogOpen(stdoutFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    }
    FINALLY()
    {
        regfree(&regExp);
    }
    TRY_END();

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Compare error log to a static string

After the comparison the log is cleared so the next result can be compared.
***********************************************************************************************************************************/
void
testLogErrResult(const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);

        FUNCTION_HARNESS_ASSERT(expected != NULL);
    FUNCTION_HARNESS_END();

    harnessLogLoad(stderrFile);

    if (strcmp(harnessLogBuffer, expected) != 0)
        THROW_FMT(AssertError, "\n\nexpected error log:\n\n%s\n\nbut actual error log was:\n\n%s\n\n", expected, harnessLogBuffer);

    close(logHandleStdErr);
    logHandleStdErr = harnessLogOpen(stderrFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Make sure nothing is left in the log after all tests have completed
***********************************************************************************************************************************/
void
testLogFinal()
{
    FUNCTION_HARNESS_VOID();

    harnessLogLoad(stdoutFile);

    if (strcmp(harnessLogBuffer, "") != 0)
        THROW_FMT(AssertError, "\n\nexpected log to be empty but actual log was:\n\n%s\n\n", harnessLogBuffer);

    harnessLogLoad(stderrFile);

    if (strcmp(harnessLogBuffer, "") != 0)
        THROW_FMT(AssertError, "\n\nexpected error log to be empty but actual error log was:\n\n%s\n\n", harnessLogBuffer);

    FUNCTION_HARNESS_RESULT_VOID();
}

#endif
