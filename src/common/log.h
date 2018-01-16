/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include "common/type.h"

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
Expose internal data for debugging/testing
***********************************************************************************************************************************/
#ifndef NDEBUG
    extern LogLevel logLevelStdOut;
    extern LogLevel logLevelStdErr;

    extern int logHandleStdOut;
    extern int logHandleStdErr;

    extern bool logTimestamp;
#endif

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void logInit(LogLevel logLevelStdOut, LogLevel logLevelStdErr, bool logTimestamp);

LogLevel logLevelEnum(const char *logLevel);
const char *logLevelStr(LogLevel logLevel);

/***********************************************************************************************************************************
Macros
***********************************************************************************************************************************/
#define LOG_ANY(logLevel, code, ...)                                                                                               \
    logInternal(logLevel, __FILE__, __func__, __LINE__, code, __VA_ARGS__)
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
    LogLevel logLevel, const char *fileName, const char *functionName, int fileLine, int code, const char *format, ...);

#endif
