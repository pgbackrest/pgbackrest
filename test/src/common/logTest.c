/***********************************************************************************************************************************
Log Test Harness
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/harnessTest.h"

#include "common/log.h"
#include "common/type.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Name of file where logs are stored for testing
***********************************************************************************************************************************/
String *stdoutFile = NULL;
String *stderrFile = NULL;

/***********************************************************************************************************************************
Initialize log for testing
***********************************************************************************************************************************/
void
testLogInit()
{
    logInit(logLevelInfo, logLevelOff, logLevelOff, false);

    stdoutFile = strNewFmt("%s/stdout.log", testPath());
    logHandleStdOut = open(strPtr(stdoutFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);

    stderrFile = strNewFmt("%s/stderr.log", testPath());
    logHandleStdErr = open(strPtr(stderrFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);
}

/***********************************************************************************************************************************
Compare log to a static string

After the comparison the log is cleared so the next result can be compared.
***********************************************************************************************************************************/
void
testLogResult(const char *expected)
{
    String *actual = strTrim(strNewBuf(storageGet(storageLocal(), stdoutFile, false)));

    if (!strEqZ(actual, expected))
        THROW(AssertError, "\n\nexpected log:\n\n%s\n\nbut actual log was:\n\n%s\n\n", expected, strPtr(actual));

    close(logHandleStdOut);
    logHandleStdOut = open(strPtr(stdoutFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);
}

/***********************************************************************************************************************************
Compare error log to a static string

After the comparison the log is cleared so the next result can be compared.
***********************************************************************************************************************************/
void
testLogErrResult(const char *expected)
{
    String *actual = strTrim(strNewBuf(storageGet(storageLocal(), stderrFile, false)));

    if (!strEqZ(actual, expected))
        THROW(AssertError, "\n\nexpected error log:\n\n%s\n\nbut actual error log was:\n\n%s\n\n", expected, strPtr(actual));

    close(logHandleStdErr);
    logHandleStdErr = open(strPtr(stderrFile), O_WRONLY | O_CREAT | O_TRUNC, 0640);
}

/***********************************************************************************************************************************
Make sure nothing is left in the log after all tests have completed
***********************************************************************************************************************************/
void
testLogFinal()
{
    String *actual = strTrim(strNewBuf(storageGet(storageLocal(), stdoutFile, false)));

    if (!strEqZ(actual, ""))
        THROW(AssertError, "\n\nexpected log to be empty but actual log was:\n\n%s\n\n", strPtr(actual));

    actual = strTrim(strNewBuf(storageGet(storageLocal(), stderrFile, false)));

    if (!strEqZ(actual, ""))
        THROW(AssertError, "\n\nexpected error log to be empty but actual error log was:\n\n%s\n\n", strPtr(actual));
}
