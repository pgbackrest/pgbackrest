/***********************************************************************************************************************************
Log Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "build/common/regExp.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/stringList.h"

#include "common/harnessDebug.h"
#include "common/harnessLog.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/***********************************************************************************************************************************
Log settings for testing
***********************************************************************************************************************************/
static LogLevel logLevelTest = logLevelInfo;
static LogLevel logLevelTestDefault = logLevelOff;
static bool logDryRunTest = false;

/***********************************************************************************************************************************
Name of file where logs are stored for testing
***********************************************************************************************************************************/
static char logFile[1024];

/***********************************************************************************************************************************
Buffer where log results are loaded for comparison purposes
***********************************************************************************************************************************/
static char harnessLogBuffer[256 * 1024];

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

    FUNCTION_HARNESS_RETURN(INT, result);
}

/***********************************************************************************************************************************
Initialize log for testing
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_LOG

void
harnessLogInit(void)
{
    FUNCTION_HARNESS_VOID();

    logInit(logLevelTestDefault, logLevelOff, logLevelInfo, false, logProcessId, 99, false);
    logFileBanner = true;

    snprintf(logFile, sizeof(logFile), "%s/expect.log", hrnPath());
    logFdFile = harnessLogOpen(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    logAnySet();

    FUNCTION_HARNESS_RETURN_VOID();
}

#endif

/**********************************************************************************************************************************/
void
harnessLogDryRunSet(bool dryRun)
{
    logDryRunTest = dryRun;

    logInit(logLevelTestDefault, logLevelOff, logLevelTest, false, logProcessId, 99, logDryRunTest);
}

/**********************************************************************************************************************************/
unsigned int
hrnLogLevelFile(void)
{
    return logLevelFile;
}

void
hrnLogLevelFileSet(unsigned int logLevel)
{
    logLevelFile = logLevel;
}

unsigned int
hrnLogLevelStdOut(void)
{
    return logLevelStdOut;
}

void
hrnLogLevelStdOutSet(unsigned int logLevel)
{
    logLevelStdOut = logLevel;
}

unsigned int
hrnLogLevelStdErr(void)
{
    return logLevelStdErr;
}

void
hrnLogLevelStdErrSet(unsigned int logLevel)
{
    logLevelStdErr = logLevel;
}

/**********************************************************************************************************************************/
bool
hrnLogTimestamp(void)
{
    return logTimestamp;
}

void
hrnLogTimestampSet(bool log)
{
    logTimestamp = log;
}

/***********************************************************************************************************************************
Change test log level

This is info by default but it can sometimes be useful to set the log level to something else.
***********************************************************************************************************************************/
void
harnessLogLevelSet(LogLevel logLevel)
{
    logLevelTest = logLevel;

    logInit(logLevelTestDefault, logLevelOff, logLevelTest, false, logProcessId, 99, logDryRunTest);
}

/***********************************************************************************************************************************
Reset test log level

Set back to info
***********************************************************************************************************************************/
void
harnessLogLevelReset(void)
{
    logLevelTest = logLevelInfo;

    logInit(logLevelTestDefault, logLevelOff, logLevelTest, false, logProcessId, 99, logDryRunTest);
}

/***********************************************************************************************************************************
Change default test log level

Set the default log level for output to the console (for testing).
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_LOG

void
harnessLogLevelDefaultSet(LogLevel logLevel)
{
    logLevelTestDefault = logLevel;
}

/**********************************************************************************************************************************/
void
hrnLogProcessIdSet(unsigned int processId)
{
    logProcessId = processId;
}

#endif

/***********************************************************************************************************************************
Load log result from file into the log buffer
***********************************************************************************************************************************/
static void
harnessLogLoad(const char *logFile)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, logFile);

        FUNCTION_HARNESS_ASSERT(logFile != NULL);
    FUNCTION_HARNESS_END();

    harnessLogBuffer[0] = 0;

    int fd = harnessLogOpen(logFile, O_RDONLY, 0);

    size_t totalBytes = 0;
    ssize_t actualBytes = 0;

    do
    {
        actualBytes = read(fd, harnessLogBuffer, sizeof(harnessLogBuffer) - totalBytes);

        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to read log file '%s'", logFile);

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    if (close(fd) == -1)
        THROW_SYS_ERROR_FMT(FileOpenError, "unable to close log file '%s'", logFile);

    // Remove final linefeed
    if (totalBytes > 0)
        harnessLogBuffer[totalBytes - 1] = 0;

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
static struct
{
    MemContext *memContext;                                         // Mem context for log harness
    List *replaceList;                                              // List of replacements
} harnessLog;

typedef struct HarnessLogReplace
{
    const String *expression;
    RegExp *regExp;
    const String *expressionSub;
    RegExp *regExpSub;
    const String *replacement;
    StringList *matchList;
    bool version;
} HarnessLogReplace;

void
hrnLogReplaceAdd(const char *expression, const char *expressionSub, const char *replacement, bool version)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expression);
        FUNCTION_HARNESS_PARAM(STRINGZ, expressionSub);
        FUNCTION_HARNESS_PARAM(STRINGZ, replacement);
        FUNCTION_HARNESS_PARAM(BOOL, version);
    FUNCTION_HARNESS_END();

    FUNCTION_HARNESS_ASSERT(expression != NULL);
    FUNCTION_HARNESS_ASSERT(replacement != NULL);

    if (harnessLog.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(HarnessLog, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                harnessLog.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    if (harnessLog.replaceList == NULL)
    {
        MEM_CONTEXT_BEGIN(harnessLog.memContext)
        {
            harnessLog.replaceList = lstNewP(sizeof(HarnessLogReplace));
        }
        MEM_CONTEXT_END();
    }

    MEM_CONTEXT_BEGIN(lstMemContext(harnessLog.replaceList))
    {
        HarnessLogReplace logReplace =
        {
            .expression = strNewZ(expression),
            .regExp = regExpNew(STR(expression)),
            .expressionSub = expressionSub == NULL ? NULL : strNewZ(expressionSub),
            .regExpSub = expressionSub == NULL ? NULL : regExpNew(STR(expressionSub)),
            .replacement = strNewZ(replacement),
            .matchList = strLstNew(),
            .version = version,
        };

        lstAdd(harnessLog.replaceList, &logReplace);
    }
    MEM_CONTEXT_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnLogReplaceRemove(const char *const expression)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expression);
    FUNCTION_HARNESS_END();

    unsigned int replaceIdx = 0;

    for (; replaceIdx < lstSize(harnessLog.replaceList); replaceIdx++)
    {
        const HarnessLogReplace *const logReplace = lstGet(harnessLog.replaceList, replaceIdx);

        if (strEqZ(logReplace->expression, expression))
        {
            lstRemoveIdx(harnessLog.replaceList, replaceIdx);
            break;
        }
    }

    if (replaceIdx == lstSize(harnessLog.replaceList))
        THROW_FMT(AssertError, "expression '%s' not found in replace list", expression);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnLogReplaceClear(void)
{
    FUNCTION_HARNESS_VOID();

    if (harnessLog.replaceList != NULL)
        lstClear(harnessLog.replaceList);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Perform log replacements
***********************************************************************************************************************************/
static void
hrnLogReplace(void)
{
    FUNCTION_HARNESS_VOID();

    // Proceed only if replacements have been defined
    if (harnessLog.replaceList != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Loop through all replacements
            for (unsigned int replaceIdx = 0; replaceIdx < lstSize(harnessLog.replaceList); replaceIdx++)
            {
                HarnessLogReplace *logReplace = lstGet(harnessLog.replaceList, replaceIdx);

                // Check for matches
                while (regExpMatch(logReplace->regExp, STRDEF(harnessLogBuffer)))
                {
                    // Get the match
                    String *match = regExpMatchStr(logReplace->regExp, STRDEF(harnessLogBuffer));

                    // Find beginning of match
                    char *begin =
                        harnessLogBuffer + (regExpMatchPtr(logReplace->regExp, STRDEF(harnessLogBuffer)) - harnessLogBuffer);

                    // If there is a sub expression then evaluate it
                    if (logReplace->regExpSub != NULL)
                    {
                        // The sub expression must match
                        if (!regExpMatch(logReplace->regExpSub, match))
                        {
                            THROW_FMT(
                                AssertError, "unable to find sub expression '%s' in '%s' extracted with expression '%s'",
                                strZ(logReplace->expressionSub), strZ(match), strZ(logReplace->expression));
                        }

                        // Find beginning of match
                        begin += regExpMatchPtr(logReplace->regExpSub, match) - strZ(match);

                        // Get the match
                        match = regExpMatchStr(logReplace->regExpSub, match);
                    }

                    // Build replacement string. If versioned then append the version number.
                    String *replace = strCatFmt(strNew(), "[%s", strZ(logReplace->replacement));

                    if (logReplace->version)
                    {
                        unsigned int index = strLstFindIdxP(logReplace->matchList, match);

                        if (index == LIST_NOT_FOUND)
                        {
                            index = strLstSize(logReplace->matchList);
                            strLstAdd(logReplace->matchList, match);
                        }

                        strCatFmt(replace, "-%u", index + 1);
                    }

                    strCatZ(replace, "]");

                    // Find end of match and calculate size difference from replacement
                    char *end = begin + strSize(match);
                    int diff = (int)strSize(replace) - (int)strSize(match);

                    // Make sure we won't overflow the buffer
                    ASSERT((size_t)((int)strlen(harnessLogBuffer) + diff) < sizeof(harnessLogBuffer) - 1);

                    // Move data from end of string enough to make room for the replacement and copy replacement
                    memmove(end + diff, end, strlen(end) + 1);
                    memcpy(begin, strZ(replace), strSize(replace));
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
harnessLogResult(const char *expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, expected);
    FUNCTION_HARNESS_END();

    ASSERT(expected != NULL);

    harnessLogLoad(logFile);
    hrnLogReplace();

    if (strcmp(harnessLogBuffer, expected) != 0)
    {
        THROW_FMT(
            AssertError, "\nACTUAL LOG:\n\n%s\n\nBUT DIFF FROM EXPECTED IS (- remove from expected, + add to expected):\n\n%s",
            harnessLogBuffer, hrnDiff(expected, harnessLogBuffer));
    }

    close(logFdFile);
    logFdFile = harnessLogOpen(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
harnessLogResultEmptyOrContains(const char *const contains)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRINGZ, contains);
    FUNCTION_HARNESS_END();

    ASSERT(contains != NULL);

    harnessLogLoad(logFile);
    hrnLogReplace();

    if (strlen(harnessLogBuffer) != 0 && strstr(harnessLogBuffer, contains) == NULL)
    {
        THROW_FMT(
            AssertError, "\nLOG MUST CONTAIN:\n\n%s\n\nBUT WAS ACTUALLY:\n\n%s",
            contains, harnessLogBuffer);
    }

    close(logFdFile);
    logFdFile = harnessLogOpen(logFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Make sure nothing is left in the log after all tests have completed
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_LOG

void
harnessLogFinal(void)
{
    FUNCTION_HARNESS_VOID();

    harnessLogLoad(logFile);
    hrnLogReplace();

    // Close expect log file
    close(logFdFile);

    if (strcmp(harnessLogBuffer, "") != 0)
        THROW_FMT(AssertError, "\n\nexpected log to be empty but actual log was:\n\n%s\n\n", harnessLogBuffer);

    FUNCTION_HARNESS_RETURN_VOID();
}

#endif
