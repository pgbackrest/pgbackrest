/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdbool.h>

/***********************************************************************************************************************************
Log types
***********************************************************************************************************************************/
typedef enum
{
    logLevelOff,
    logLevelAssert,
    logLevelError,
    logLevelProtocol,
    logLevelWarn,
    logLevelInfo,
    logLevelDetail,
    logLevelDebug,
    logLevelTrace,
} LogLevel;

/***********************************************************************************************************************************
Expose internal data for unit testing/debugging
***********************************************************************************************************************************/
#ifdef DEBUG_UNIT
    extern LogLevel logLevelStdOut;
    extern LogLevel logLevelStdErr;
    extern LogLevel logLevelFile;

    extern int logHandleStdOut;
    extern int logHandleStdErr;
    extern int logHandleFile;
#endif

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void logInit(LogLevel logLevelStdOut, LogLevel logLevelStdErr, LogLevel logLevelFile, bool logTimestamp);
void logFileSet(const char *logFile);

bool logWill(LogLevel logLevel);

LogLevel logLevelEnum(const char *logLevel);
const char *logLevelStr(LogLevel logLevel);

/***********************************************************************************************************************************
Macros

Only call logInternal() if the message will be logged to one of the available outputs.
***********************************************************************************************************************************/
#define LOG_ANY(logLevel, code, ...)                                                                                               \
{                                                                                                                                  \
    if (logWill(logLevel))                                                                                                         \
        logInternal(logLevel, __FILE__, __func__, code, __VA_ARGS__);                                                              \
}

#define LOG_ASSERT(...)                                                                                                            \
    LOG_ANY(logLevelAssert, errorTypeCode(&AssertError), __VA_ARGS__)
#define LOG_ERROR(code, ...)                                                                                                       \
    LOG_ANY(logLevelError, code, __VA_ARGS__)
#define LOG_INFO(...)                                                                                                              \
    LOG_ANY(logLevelInfo, 0, __VA_ARGS__)
#define LOG_WARN(...)                                                                                                              \
    LOG_ANY(logLevelWarn, 0, __VA_ARGS__)

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
void logInternal(
    LogLevel logLevel, const char *fileName, const char *functionName, int code, const char *format, ...);

#endif
