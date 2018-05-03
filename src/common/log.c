/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"
#include "common/time.h"

/***********************************************************************************************************************************
Module variables
***********************************************************************************************************************************/
// Log levels
DEBUG_UNIT_EXTERN LogLevel logLevelStdOut = logLevelError;
DEBUG_UNIT_EXTERN LogLevel logLevelStdErr = logLevelError;
DEBUG_UNIT_EXTERN LogLevel logLevelFile = logLevelOff;

// Log file handles
DEBUG_UNIT_EXTERN int logHandleStdOut = STDOUT_FILENO;
DEBUG_UNIT_EXTERN int logHandleStdErr = STDERR_FILENO;
DEBUG_UNIT_EXTERN int logHandleFile = -1;

// Has the log file banner been written yet?
static bool logFileBanner = false;

// Is the timestamp printed in the log?
static bool logTimestamp = false;

/***********************************************************************************************************************************
Debug Asserts
***********************************************************************************************************************************/
#define ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)                                                                             \
    ASSERT_DEBUG(logLevel > logLevelOff)

/***********************************************************************************************************************************
Log buffer
***********************************************************************************************************************************/
#define LOG_BUFFER_SIZE                                             32768

char logBuffer[LOG_BUFFER_SIZE];
char logFormat[LOG_BUFFER_SIZE];

/***********************************************************************************************************************************
Convert log level to string and vice versa
***********************************************************************************************************************************/
#define LOG_LEVEL_TOTAL                                             9

const char *logLevelList[LOG_LEVEL_TOTAL] =
{
    "OFF",
    "ASSERT",
    "ERROR",
    "PROTOCOL",
    "WARN",
    "INFO",
    "DETAIL",
    "DEBUG",
    "TRACE",
};

LogLevel
logLevelEnum(const char *logLevel)
{
    LogLevel result = logLevelOff;

    // Search for the log level
    for (; result < LOG_LEVEL_TOTAL; result++)
        if (strcasecmp(logLevel, logLevelList[result]) == 0)
            break;

    // If the log level was not found
    if (result == LOG_LEVEL_TOTAL)
        THROW_FMT(AssertError, "log level '%s' not found", logLevel);

    return result;
}

const char *
logLevelStr(LogLevel logLevel)
{
    if (logLevel >= LOG_LEVEL_TOTAL)
        THROW_FMT(AssertError, "invalid log level '%u'", logLevel);

    return logLevelList[logLevel];
}

/***********************************************************************************************************************************
Initialize the log system
***********************************************************************************************************************************/
void
logInit(LogLevel logLevelStdOutParam, LogLevel logLevelStdErrParam, LogLevel logLevelFileParam, bool logTimestampParam)
{
    logLevelStdOut = logLevelStdOutParam;
    logLevelStdErr = logLevelStdErrParam;
    logLevelFile = logLevelFileParam;
    logTimestamp = logTimestampParam;
}

/***********************************************************************************************************************************
Set the log file
***********************************************************************************************************************************/
void
logFileSet(const char *logFile)
{
    // Close the file handle if it is already open
    if (logHandleFile != -1)
    {
        close(logHandleFile);
        logHandleFile = -1;
    }

    // Only open the file if there is a chance to log something
    if (logLevelFile != logLevelOff)
    {
        // Open the file and handle errors
        logHandleFile = open(logFile, O_CREAT | O_APPEND | O_WRONLY, 0750);

        if (logHandleFile == -1)
        {
            int errNo = errno;
            LOG_WARN("unable to open log file '%s': %s\nNOTE: process will continue without log file.", logFile, strerror(errNo));
        };

        // Output the banner on first log message
        logFileBanner = false;
    }
}

/***********************************************************************************************************************************
Check if a log level will be logged to any output

This is useful for log messages that are expensive to generate and should be skipped if they will be discarded.
***********************************************************************************************************************************/
static bool
logWillFile(LogLevel logLevel)
{
    ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)
    return logLevel <= logLevelFile && logHandleFile != -1;
}

static bool
logWillStdErr(LogLevel logLevel)
{
    ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)
    return logLevel <= logLevelStdErr;
}

static bool
logWillStdOut(LogLevel logLevel)
{
    ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)
    return logLevel <= logLevelStdOut;
}

bool
logWill(LogLevel logLevel)
{
    ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)
    return logWillStdOut(logLevel) || logWillStdErr(logLevel) || logWillFile(logLevel);
}

/***********************************************************************************************************************************
General log function
***********************************************************************************************************************************/
void
logInternal(LogLevel logLevel, const char *fileName, const char *functionName, int code, const char *format, ...)
{
    ASSERT_DEBUG_MESSAGE_LOG_LEVEL_VALID(logLevel)

    size_t bufferPos = 0;   // Current position in the buffer

    // Add time
    if (logTimestamp)
    {
        TimeMSec logTimeMSec = timeMSec();
        time_t logTimeSec = (time_t)(logTimeMSec / MSEC_PER_SEC);

        bufferPos += strftime(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "%Y-%m-%d %H:%M:%S", localtime(&logTimeSec));
        bufferPos += (size_t)snprintf(
            logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, ".%03d ", (int)(logTimeMSec % 1000));
    }

    // Add process and aligned log level
    bufferPos += (size_t)snprintf(
        logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "P00 %*s: ", 6, logLevelStr(logLevel));

    // Position after the timestamp and process id for output to stderr
    size_t messageStdErrPos = bufferPos - strlen(logLevelStr(logLevel)) - 2;

    // Check that error code matches log level
    ASSERT_DEBUG(
        code == 0 || (logLevel == logLevelError && code != errorTypeCode(&AssertError)) ||
        (logLevel == logLevelAssert && code == errorTypeCode(&AssertError)));

    // Add code
    if (code != 0)
        bufferPos += (size_t)snprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "[%03d]: ", code);

    // Add debug info
    if (logLevel >= logLevelDebug)
    {
        bufferPos += (size_t)snprintf(
            logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "%s:%s(): ", fileName, functionName);
    }

    // Format message
    va_list argumentList;
    va_start(argumentList, format);

    if (logLevel <= logLevelStdErr || strchr(format, '\n') == NULL)
        bufferPos += (size_t)vsnprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, format, argumentList);
    else
    {
        vsnprintf(logFormat, LOG_BUFFER_SIZE, format, argumentList);

        // Indent all lines after the first
        const char *formatPtr = logFormat;
        const char *linefeedPtr = strchr(logFormat, '\n');
        int indentSize = 12;

        while (linefeedPtr != NULL)
        {
            strncpy(logBuffer + bufferPos, formatPtr, (size_t)(linefeedPtr - formatPtr + 1));
            bufferPos += (size_t)(linefeedPtr - formatPtr + 1);

            formatPtr = linefeedPtr + 1;
            linefeedPtr = strchr(formatPtr, '\n');

            for (int indentIdx = 0; indentIdx < indentSize; indentIdx++)
                logBuffer[bufferPos++] = ' ';
        }

        strcpy(logBuffer + bufferPos, formatPtr);
        bufferPos += strlen(formatPtr);
    }

    va_end(argumentList);

    // Add linefeed
    logBuffer[bufferPos++] = '\n';
    logBuffer[bufferPos] = 0;

    // Determine where to log the message based on log-level-stderr
    if (logWillStdErr(logLevel))
    {
        if (write(
            logHandleStdErr, logBuffer + messageStdErrPos, bufferPos - messageStdErrPos) != (int)(bufferPos - messageStdErrPos))
        {
            THROW_SYS_ERROR(FileWriteError, "unable to write log to stderr");
        }
    }
    else if (logWillStdOut(logLevel))
    {
        if (write(logHandleStdOut, logBuffer, bufferPos) != (int)bufferPos)                     // {uncovered - write does not fail}
            THROW_SYS_ERROR(FileWriteError, "unable to write log to stdout");                   // {uncovered+}
    }

    // Log to file
    if (logWillFile(logLevel))
    {
        // If the banner has not been written
        if (!logFileBanner)
        {
            // Add a blank line if the file already has content
            if (lseek(logHandleFile, 0, SEEK_END) > 0)
            {
                if (write(logHandleFile, "\n", 1) != 1)                                         // {uncovered - write does not fail}
                    THROW_SYS_ERROR(FileWriteError, "unable to write banner spacing to file");  // {uncovered +}
            }

            // Write process start banner
            const char *banner = "-------------------PROCESS START-------------------\n";

            if (write(logHandleFile, banner, strlen(banner)) != (int)strlen(banner))            // {uncovered - write does not fail}
                THROW_SYS_ERROR(FileWriteError, "unable to write banner to file");              // {uncovered+}

            // Mark banner as written
            logFileBanner = true;
        }

        if (write(logHandleFile, logBuffer, bufferPos) != (int)bufferPos)                       // {uncovered - write does not fail}
            THROW_SYS_ERROR(FileWriteError, "unable to write log to file");                     // {uncovered+}
    }
}
