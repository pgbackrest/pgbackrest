/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "common/time.h"

/***********************************************************************************************************************************
Module variables
***********************************************************************************************************************************/
// Log levels
static LogLevel logLevelStdOut = logLevelError;
static LogLevel logLevelStdErr = logLevelError;
static LogLevel logLevelFile = logLevelOff;
static LogLevel logLevelAny = logLevelError;

// Log file descriptors
static int logFdStdOut = STDOUT_FILENO;
static int logFdStdErr = STDERR_FILENO;
static int logFdFile = -1;

// Has the log file banner been written yet?
static bool logFileBanner = false;

// Is the timestamp printed in the log?
static bool logTimestamp = false;

// Default process id if none is specified
static unsigned int logProcessId = 0;

// Size of the process id field
static int logProcessSize = 2;

// Prefix DRY-RUN to log messages
static bool logDryRun = false;

/***********************************************************************************************************************************
Dry run prefix
***********************************************************************************************************************************/
#define DRY_RUN_PREFIX                                              "[DRY-RUN] "

/***********************************************************************************************************************************
Test Asserts
***********************************************************************************************************************************/
#define ASSERT_LOG_LEVEL(logLevel)                                                                                                 \
    ASSERT(logLevel >= LOG_LEVEL_MIN && logLevel <= LOG_LEVEL_MAX)

/***********************************************************************************************************************************
Log buffer -- used to format log header and message
***********************************************************************************************************************************/
static char logBuffer[LOG_BUFFER_SIZE];

/**********************************************************************************************************************************/
#define LOG_LEVEL_TOTAL                                             (LOG_LEVEL_MAX + 1)

static const char *const logLevelList[LOG_LEVEL_TOTAL] =
{
    "OFF",
    "ASSERT",
    "ERROR",
    "WARN",
    "INFO",
    "DETAIL",
    "DEBUG",
    "TRACE",
};

LogLevel
logLevelEnum(const char *logLevel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, logLevel);
    FUNCTION_TEST_END();

    ASSERT(logLevel != NULL);

    LogLevel result = logLevelOff;

    // Search for the log level
    for (; result < LOG_LEVEL_TOTAL; result++)
        if (strcasecmp(logLevel, logLevelList[result]) == 0)
            break;

    // If the log level was not found
    if (result == LOG_LEVEL_TOTAL)
        THROW_FMT(AssertError, "log level '%s' not found", logLevel);

    FUNCTION_TEST_RETURN(result);
}

const char *
logLevelStr(LogLevel logLevel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
    FUNCTION_TEST_END();

    ASSERT(logLevel <= LOG_LEVEL_MAX);

    FUNCTION_TEST_RETURN(logLevelList[logLevel]);
}

/**********************************************************************************************************************************/
static void
logAnySet(void)
{
    FUNCTION_TEST_VOID();

    logLevelAny = logLevelStdOut;

    if (logLevelStdErr > logLevelAny)
        logLevelAny = logLevelStdErr;

    if (logLevelFile > logLevelAny && logFdFile != -1)
        logLevelAny = logLevelFile;

    FUNCTION_TEST_RETURN_VOID();
}

bool
logAny(LogLevel logLevel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
    FUNCTION_TEST_END();

    ASSERT_LOG_LEVEL(logLevel);

    FUNCTION_TEST_RETURN(logLevel <= logLevelAny);
}

/**********************************************************************************************************************************/
void
logInit(
    LogLevel logLevelStdOutParam, LogLevel logLevelStdErrParam, LogLevel logLevelFileParam, bool logTimestampParam,
    unsigned int processId, unsigned int logProcessMax, bool dryRunParam)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevelStdOutParam);
        FUNCTION_TEST_PARAM(ENUM, logLevelStdErrParam);
        FUNCTION_TEST_PARAM(ENUM, logLevelFileParam);
        FUNCTION_TEST_PARAM(BOOL, logTimestampParam);
        FUNCTION_TEST_PARAM(UINT, processId);
        FUNCTION_TEST_PARAM(UINT, logProcessMax);
        FUNCTION_TEST_PARAM(BOOL, dryRunParam);
    FUNCTION_TEST_END();

    ASSERT(logLevelStdOutParam <= LOG_LEVEL_MAX);
    ASSERT(logLevelStdErrParam <= LOG_LEVEL_MAX);
    ASSERT(logLevelFileParam <= LOG_LEVEL_MAX);
    ASSERT(processId <= 999);
    ASSERT(logProcessMax <= 999);

    logLevelStdOut = logLevelStdOutParam;
    logLevelStdErr = logLevelStdErrParam;
    logLevelFile = logLevelFileParam;
    logTimestamp = logTimestampParam;
    logProcessId = processId;
    logProcessSize = logProcessMax > 99 ? 3 : 2;
    logDryRun = dryRunParam;

    logAnySet();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the log file
***********************************************************************************************************************************/
static void
logFileClose(void)
{
    FUNCTION_TEST_VOID();

    // Close the file descriptor if it is open
    if (logFdFile != -1)
    {
        close(logFdFile);
        logFdFile = -1;
    }

    logAnySet();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
logFileSet(const char *logFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, logFile);
    FUNCTION_TEST_END();

    ASSERT(logFile != NULL);

    // Close the log file if it is already open
    logFileClose();

    // Only open the file if there is a chance to log something
    bool result = true;

    if (logLevelFile != logLevelOff)
    {
        // Open the file and handle errors
        logFdFile = open(logFile, O_CREAT | O_APPEND | O_WRONLY, 0640);

        if (logFdFile == -1)
        {
            int errNo = errno;
            LOG_WARN_FMT(
                "unable to open log file '%s': %s\nNOTE: process will continue without log file.", logFile, strerror(errNo));
            result = false;
        }

        // Output the banner on first log message
        logFileBanner = false;

        logAnySet();
    }

    logAnySet();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
logClose(void)
{
    FUNCTION_TEST_VOID();

    // Disable all logging
    logInit(logLevelOff, logLevelOff, logLevelOff, false, 0, 1, false);

    // Close the log file if it is open
    logFileClose();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Determine if the log level is in the specified range
***********************************************************************************************************************************/
static bool
logRange(LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
        FUNCTION_TEST_PARAM(ENUM, logRangeMin);
        FUNCTION_TEST_PARAM(ENUM, logRangeMax);
    FUNCTION_TEST_END();

    ASSERT_LOG_LEVEL(logLevel);
    ASSERT_LOG_LEVEL(logRangeMin);
    ASSERT_LOG_LEVEL(logRangeMax);
    ASSERT(logRangeMin <= logRangeMax);

    FUNCTION_TEST_RETURN(logLevel >= logRangeMin && logLevel <= logRangeMax);
}

/***********************************************************************************************************************************
Internal write function that handles errors
***********************************************************************************************************************************/
static void
logWrite(int fd, const char *message, size_t messageSize, const char *errorDetail)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, fd);
        FUNCTION_TEST_PARAM(STRINGZ, message);
        FUNCTION_TEST_PARAM(SIZE, messageSize);
        FUNCTION_TEST_PARAM(STRINGZ, errorDetail);
    FUNCTION_TEST_END();

    ASSERT(fd != -1);
    ASSERT(message != NULL);
    ASSERT(messageSize != 0);
    ASSERT(errorDetail != NULL);

    if ((size_t)write(fd, message, messageSize) != messageSize)
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write %s", errorDetail);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Write out log message and indent subsequent lines
***********************************************************************************************************************************/
static void
logWriteIndent(int fd, const char *message, size_t indentSize, const char *errorDetail)
{
    // Indent buffer -- used to write out indent space without having to loop
    static const char indentBuffer[] = "                                                                                          ";

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, fd);
        FUNCTION_TEST_PARAM(STRINGZ, message);
        FUNCTION_TEST_PARAM(SIZE, indentSize);
        FUNCTION_TEST_PARAM(STRINGZ, errorDetail);
    FUNCTION_TEST_END();

    ASSERT(fd != -1);
    ASSERT(message != NULL);
    ASSERT(indentSize > 0 && indentSize < sizeof(indentBuffer));
    ASSERT(errorDetail != NULL);

    // Indent all lines after the first
    const char *linefeedPtr = strchr(message, '\n');
    bool first = true;

    while (linefeedPtr != NULL)
    {
        if (!first)
            logWrite(fd, indentBuffer, indentSize, errorDetail);
        else
            first = false;

        logWrite(fd, message, (size_t)(linefeedPtr - message + 1), errorDetail);
        message += (size_t)(linefeedPtr - message + 1);

        linefeedPtr = strchr(message, '\n');
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Generate the log header and anything else that needs to happen before the message is output
***********************************************************************************************************************************/
typedef struct LogPreResult
{
    size_t bufferPos;
    char *logBufferStdErr;
    size_t indentSize;
} LogPreResult;

static LogPreResult
logPre(LogLevel logLevel, unsigned int processId, const char *fileName, const char *functionName, int code)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
        FUNCTION_TEST_PARAM(UINT, processId);
        FUNCTION_TEST_PARAM(STRINGZ, fileName);
        FUNCTION_TEST_PARAM(STRINGZ, functionName);
        FUNCTION_TEST_PARAM(INT, code);
    FUNCTION_TEST_END();

    ASSERT_LOG_LEVEL(logLevel);
    ASSERT(fileName != NULL);
    ASSERT(functionName != NULL);
    ASSERT(
        (code == 0 && logLevel > logLevelError) || (logLevel == logLevelError && code != errorTypeCode(&AssertError)) ||
        (logLevel == logLevelAssert && code == errorTypeCode(&AssertError)));

    // Initialize buffer position
    LogPreResult result = {.bufferPos = 0};

    // Add time
    if (logTimestamp)
    {
        struct tm timePart;
        TimeMSec logTimeMSec = timeMSec();
        time_t logTimeSec = (time_t)(logTimeMSec / MSEC_PER_SEC);

        result.bufferPos += strftime(
            logBuffer + result.bufferPos, sizeof(logBuffer) - result.bufferPos, "%Y-%m-%d %H:%M:%S",
            localtime_r(&logTimeSec, &timePart));
        result.bufferPos += (size_t)snprintf(
            logBuffer + result.bufferPos, sizeof(logBuffer) - result.bufferPos, ".%03d ", (int)(logTimeMSec % 1000));
    }

    // Add process and aligned log level
    result.bufferPos += (size_t)snprintf(
        logBuffer + result.bufferPos, sizeof(logBuffer) - result.bufferPos, "P%0*u %*s: ", logProcessSize,
        processId == (unsigned int)-1 ? logProcessId : processId, 6, logLevelStr(logLevel));

    // When writing to stderr the timestamp, process, and log level alignment will be skipped
    result.logBufferStdErr = logBuffer + result.bufferPos - strlen(logLevelStr(logLevel)) - 2;

    // Set the indent size -- this will need to be adjusted for stderr
    result.indentSize = result.bufferPos;

    // Add error code
    if (code != 0)
        result.bufferPos += (size_t)snprintf(logBuffer + result.bufferPos, sizeof(logBuffer) - result.bufferPos, "[%03d]: ", code);

    // Add dry-run prefix
    if (logDryRun)
        result.bufferPos += (size_t)snprintf(logBuffer + result.bufferPos, sizeof(logBuffer) - result.bufferPos, DRY_RUN_PREFIX);

    // Add debug info
    if (logLevel >= logLevelDebug)
    {
        // Adding padding for debug and trace levels
        for (unsigned int paddingIdx = 0; paddingIdx < ((logLevel - logLevelDebug + 1) * 4); paddingIdx++)
        {
            logBuffer[result.bufferPos++] = ' ';
            result.indentSize++;
        }

        result.bufferPos += (size_t)snprintf(
            logBuffer + result.bufferPos, LOG_BUFFER_SIZE - result.bufferPos, "%.*s::%s: ", (int)strlen(fileName) - 2, fileName,
            functionName);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Finalize formatting and log after the message has been added to the buffer
***********************************************************************************************************************************/
static void
logPost(LogPreResult *logData, LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(SIZE, logData->bufferPos);
        FUNCTION_TEST_PARAM(STRINGZ, logData->logBufferStdErr);
        FUNCTION_TEST_PARAM(SIZE, logData->indentSize);
        FUNCTION_TEST_PARAM(ENUM, logLevel);
        FUNCTION_TEST_PARAM(ENUM, logRangeMin);
        FUNCTION_TEST_PARAM(ENUM, logRangeMax);
    FUNCTION_TEST_END();

    ASSERT_LOG_LEVEL(logLevel);
    ASSERT_LOG_LEVEL(logRangeMin);
    ASSERT_LOG_LEVEL(logRangeMax);
    ASSERT(logRangeMin <= logRangeMax);

    // Add linefeed
    logBuffer[logData->bufferPos++] = '\n';
    logBuffer[logData->bufferPos] = 0;

    // Determine where to log the message based on log-level-stderr
    if (logLevel <= logLevelStdErr)
    {
        if (logRange(logLevelStdErr, logRangeMin, logRangeMax))
        {
            logWriteIndent(
                logFdStdErr, logData->logBufferStdErr, logData->indentSize - (size_t)(logData->logBufferStdErr - logBuffer),
                "log to stderr");
        }
    }
    else if (logLevel <= logLevelStdOut && logRange(logLevelStdOut, logRangeMin, logRangeMax))
        logWriteIndent(logFdStdOut, logBuffer, logData->indentSize, "log to stdout");

    // Log to file
    if (logLevel <= logLevelFile && logFdFile != -1 && logRange(logLevelFile, logRangeMin, logRangeMax))
    {
        // If the banner has not been written
        if (!logFileBanner)
        {
            // Add a blank line if the file already has content
            if (lseek(logFdFile, 0, SEEK_END) > 0)
                logWrite(logFdFile, "\n", 1, "banner spacing to file");

            // Write process start banner
            const char *banner = "-------------------PROCESS START-------------------\n";

            logWrite(logFdFile, banner, strlen(banner), "banner to file");

            // Mark banner as written
            logFileBanner = true;
        }

        logWriteIndent(logFdFile, logBuffer, logData->indentSize, "log to file");
    }

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
logInternal(
    LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *message)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
        FUNCTION_TEST_PARAM(ENUM, logRangeMin);
        FUNCTION_TEST_PARAM(ENUM, logRangeMax);
        FUNCTION_TEST_PARAM(UINT, processId);
        FUNCTION_TEST_PARAM(STRINGZ, fileName);
        FUNCTION_TEST_PARAM(STRINGZ, functionName);
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(STRINGZ, message);
    FUNCTION_TEST_END();

    ASSERT(message != NULL);

    LogPreResult logData = logPre(logLevel, processId, fileName, functionName, code);

    // Copy message into buffer and update buffer position
    strncpy(logBuffer + logData.bufferPos, message, sizeof(logBuffer) - logData.bufferPos);
    logBuffer[sizeof(logBuffer) - 1] = 0;
    logData.bufferPos += strlen(logBuffer + logData.bufferPos);

    logPost(&logData, logLevel, logRangeMin, logRangeMax);

    FUNCTION_TEST_RETURN_VOID();
}

void
logInternalFmt(
    LogLevel logLevel, LogLevel logRangeMin, LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *format, ...)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, logLevel);
        FUNCTION_TEST_PARAM(ENUM, logRangeMin);
        FUNCTION_TEST_PARAM(ENUM, logRangeMax);
        FUNCTION_TEST_PARAM(UINT, processId);
        FUNCTION_TEST_PARAM(STRINGZ, fileName);
        FUNCTION_TEST_PARAM(STRINGZ, functionName);
        FUNCTION_TEST_PARAM(INT, code);
        FUNCTION_TEST_PARAM(STRINGZ, format);
    FUNCTION_TEST_END();

    ASSERT(format != NULL);

    LogPreResult logData = logPre(logLevel, processId, fileName, functionName, code);

    // Format message into buffer and update buffer position
    va_list argumentList;
    va_start(argumentList, format);
    logData.bufferPos += (size_t)vsnprintf(
        logBuffer + logData.bufferPos, LOG_BUFFER_SIZE - logData.bufferPos, format, argumentList);
    va_end(argumentList);

    logPost(&logData, logLevel, logRangeMin, logRangeMax);

    FUNCTION_TEST_RETURN_VOID();
}
