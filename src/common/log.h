/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdbool.h>

#include "common/logLevel.h"

/***********************************************************************************************************************************
Max size allowed for a single log message including header
***********************************************************************************************************************************/
#ifndef LOG_BUFFER_SIZE
    #define LOG_BUFFER_SIZE                                         ((size_t)(32 * 1024))
#endif

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void logInit(LogLevel logLevelStdOut, LogLevel logLevelStdErr, LogLevel logLevelFile, bool logTimestamp);
bool logFileSet(const char *logFile);

bool logWill(LogLevel logLevel);

LogLevel logLevelEnum(const char *logLevel);
const char *logLevelStr(LogLevel logLevel);

/***********************************************************************************************************************************
Macros

Only call logInternal() if the message will be logged to one of the available outputs.
***********************************************************************************************************************************/
#define LOG_INTERNAL(logLevel, logRangeMin, logRangeMax, code, ...)                                                                \
    logInternal(logLevel, logRangeMin, logRangeMax, __FILE__, __func__, code, __VA_ARGS__)

#define LOG(logLevel, code, ...)                                                                                                   \
    LOG_INTERNAL(logLevel, LOG_LEVEL_MIN, LOG_LEVEL_MAX, code, __VA_ARGS__)

#define LOG_WILL(logLevel, code, ...)                                                                                              \
do                                                                                                                                 \
{                                                                                                                                  \
    if (logWill(logLevel))                                                                                                         \
        LOG(logLevel, code, __VA_ARGS__);                                                                                          \
} while(0)

#define LOG_ASSERT(...)                                                                                                            \
    LOG_WILL(logLevelAssert, errorTypeCode(&AssertError), __VA_ARGS__)
#define LOG_ERROR(code, ...)                                                                                                       \
    LOG_WILL(logLevelError, code, __VA_ARGS__)
#define LOG_WARN(...)                                                                                                              \
    LOG_WILL(logLevelWarn, 0, __VA_ARGS__)
#define LOG_INFO(...)                                                                                                              \
    LOG_WILL(logLevelInfo, 0, __VA_ARGS__)
#define LOG_DETAIL(...)                                                                                                            \
    LOG_WILL(logLevelDetail, 0, __VA_ARGS__)
#define LOG_TRACE(...)                                                                                                             \
    LOG_WILL(logLevelTrace, 0, __VA_ARGS__)

/***********************************************************************************************************************************
Internal Functions
***********************************************************************************************************************************/
void logInternal(
    LogLevel logLevel, LogLevel logRangeMin,  LogLevel logRangeMax, const char *fileName, const char *functionName,
    int code, const char *format, ...);

#endif
