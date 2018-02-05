/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "common/error.h"
#include "common/log.h"
#include "common/time.h"

/***********************************************************************************************************************************
Module variables
***********************************************************************************************************************************/
// Log levels
LogLevel logLevelStdOut = logLevelError;
LogLevel logLevelStdErr = logLevelError;

// Log file handles
int logHandleStdOut = STDOUT_FILENO;
int logHandleStdErr = STDERR_FILENO;

// Is the timestamp printed in the log?
bool logTimestamp = false;

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
        THROW(AssertError, "log level '%s' not found", logLevel);

    return result;
}

const char *
logLevelStr(LogLevel logLevel)
{
    if (logLevel >= LOG_LEVEL_TOTAL)
        THROW(AssertError, "invalid log level '%d'", logLevel);

    return logLevelList[logLevel];
}

/***********************************************************************************************************************************
Initialize the log system
***********************************************************************************************************************************/
void
logInit(LogLevel logLevelStdOutParam, LogLevel logLevelStdErrParam, bool logTimestampParam)
{
    logLevelStdOut = logLevelStdOutParam;
    logLevelStdErr = logLevelStdErrParam;
    logTimestamp = logTimestampParam;
}

/***********************************************************************************************************************************
General log function
***********************************************************************************************************************************/
void
logInternal(LogLevel logLevel, const char *fileName, const char *functionName, int fileLine, int code, const char *format, ...)
{
    // Should this entry be logged?
    if (logLevel <= logLevelStdOut || logLevel <= logLevelStdErr)
    {
        int bufferPos = 0;

        // Add time
        if (logTimestamp && logLevel > logLevelStdErr)
        {
            TimeUSec logTimeUSec = timeUSec();
            time_t logTime = (time_t)(logTimeUSec / USEC_PER_SEC);

            bufferPos += strftime(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "%Y-%m-%d %H:%M:%S", localtime(&logTime));
            bufferPos += snprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, ".%03d ", (int)(logTimeUSec / 1000 % 1000));
        }

        // Add process and aligned log level
        if (logLevel > logLevelStdErr)
            bufferPos += snprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "P00 %*s: ", 6, logLevelStr(logLevel));
        // Else just the log level with no alignment
        else
            bufferPos += snprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "%s: ", logLevelStr(logLevel));

        // Add error code
        if (code != 0)
            bufferPos += snprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "[%03d]: ", code);

        // Add debug info
        if (logLevelStdOut >= logLevelDebug)
        {
            bufferPos += snprintf(
                logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, "%s:%s():%d: ", fileName, functionName, fileLine);
        }

        // Format message
        va_list argumentList;
        va_start(argumentList, format);

        if (logLevel <= logLevelStdErr || strchr(format, '\n') == NULL)
            bufferPos += vsnprintf(logBuffer + bufferPos, LOG_BUFFER_SIZE - bufferPos, format, argumentList);
        else
        {
            vsnprintf(logFormat, LOG_BUFFER_SIZE, format, argumentList);

            // Indent all lines after the first
            const char *formatPtr = logFormat;
            const char *linefeedPtr = strchr(logFormat, '\n');
            int indentSize = 12;

            while (linefeedPtr != NULL)
            {
                strncpy(logBuffer + bufferPos, formatPtr, linefeedPtr - formatPtr + 1);
                bufferPos += linefeedPtr - formatPtr + 1;

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
        int stream = logHandleStdOut;

        if (logLevel <= logLevelStdErr)
            stream = logHandleStdErr;

        THROW_ON_SYS_ERROR(
            write(stream, logBuffer, bufferPos) != bufferPos, FileWriteError, "unable to write log to console");
    }
}
