/***********************************************************************************************************************************
Log Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <limits.h>
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
// Initialize the log system
void logInit(
    LogLevel logLevelStdOutParam, LogLevel logLevelStdErrParam, LogLevel logLevelFileParam, bool logTimestampParam,
    unsigned int processId, unsigned int logProcessMax, bool dryRun);

// Close the log system
void logClose(void);

// Set the log file. Returns true if file logging is off or the log file was successfully opened, false if the log file could not be
// opened.
bool logFileSet(const char *logFile);

// Check if a log level will be logged to any output. This is useful for log messages that are expensive to generate and should be
// skipped if they will be discarded.
bool logAny(LogLevel logLevel);

// Convert log level to string and vice versa
LogLevel logLevelEnum(const char *logLevel);
const char *logLevelStr(LogLevel logLevel);

/***********************************************************************************************************************************
Macros

Macros only call logInternal()/logInternalFmt() if the message will be logged to one of the available outputs. Also simplify each
call site by supplying commonly-used values.

Note that it's possible that that not all the macros below will appear in the code.  In particular the ERROR and ASSERT macros
should not be used directly.  They are included for completeness and future usage.
***********************************************************************************************************************************/
// Define a macro to test logAny() that can be removed when performing coverage testing.  Checking logAny() saves a function call
// for logging calls that won't be output anywhere, but since the macro contains a branch it causes coverage problems.
#ifdef DEBUG_COVERAGE
    #define IF_LOG_ANY(logLevel)
#else
    #define IF_LOG_ANY(logLevel)                                                                                                   \
        if (logAny(logLevel))
#endif

#define LOG_INTERNAL(logLevel, logRangeMin, logRangeMax, processId, code, message)                                                 \
do                                                                                                                                 \
{                                                                                                                                  \
    IF_LOG_ANY(logLevel)                                                                                                           \
    {                                                                                                                              \
        logInternal(logLevel, logRangeMin, logRangeMax, processId, __FILE__, __func__, code, message);                             \
    }                                                                                                                              \
} while (0)

#define LOG_INTERNAL_FMT(logLevel, logRangeMin, logRangeMax, processId, code, ...)                                                 \
do                                                                                                                                 \
{                                                                                                                                  \
    IF_LOG_ANY(logLevel)                                                                                                           \
    {                                                                                                                              \
        logInternalFmt(logLevel, logRangeMin, logRangeMax, processId, __FILE__, __func__, code, __VA_ARGS__);                      \
    }                                                                                                                              \
} while (0)

#define LOG_PID(logLevel, processId, code, message)                                                                                \
    LOG_INTERNAL(logLevel, LOG_LEVEL_MIN, LOG_LEVEL_MAX, processId, code, message)

#define LOG_ASSERT_PID(processId, message)                                                                                         \
    LOG_PID(logLevelAssert, processId, errorTypeCode(&AssertError), message)
#define LOG_ERROR_PID(processId, code, message)                                                                                    \
    LOG_PID(logLevelError, processId, code, message)
#define LOG_WARN_PID(processId, message)                                                                                           \
    LOG_PID(logLevelWarn, processId, 0, message)
#define LOG_INFO_PID(processId, message)                                                                                           \
    LOG_PID(logLevelInfo, processId, 0, message)
#define LOG_DETAIL_PID(processId, message)                                                                                         \
    LOG_PID(logLevelDetail, processId, 0, message)
#define LOG_DEBUG_PID(processId, message)                                                                                          \
    LOG_PID(logLevelDebug, processId, 0, message)
#define LOG_TRACE_PID(processId, message)                                                                                          \
    LOG_PID(logLevelTrace, processId, 0, message)

#define LOG_PID_FMT(logLevel, processId, code, ...)                                                                                \
    LOG_INTERNAL_FMT(logLevel, LOG_LEVEL_MIN, LOG_LEVEL_MAX, processId, code, __VA_ARGS__)

#define LOG_ASSERT_PID_FMT(processId, ...)                                                                                         \
    LOG_PID_FMT(logLevelAssert, processId, errorTypeCode(&AssertError), __VA_ARGS__)
#define LOG_ERROR_PID_FMT(processId, code, ...)                                                                                    \
    LOG_PID_FMT(logLevelError, processId, code, __VA_ARGS__)
#define LOG_WARN_PID_FMT(processId, ...)                                                                                           \
    LOG_PID_FMT(logLevelWarn, processId, 0, __VA_ARGS__)
#define LOG_INFO_PID_FMT(processId, ...)                                                                                           \
    LOG_PID_FMT(logLevelInfo, processId, 0, __VA_ARGS__)
#define LOG_DETAIL_PID_FMT(processId, ...)                                                                                         \
    LOG_PID_FMT(logLevelDetail, processId, 0, __VA_ARGS__)
#define LOG_DEBUG_PID_FMT(processId, ...)                                                                                          \
    LOG_PID_FMT(logLevelDebug, processId, 0, __VA_ARGS__)
#define LOG_TRACE_PID_FMT(processId, ...)                                                                                          \
    LOG_PID_FMT(logLevelTrace, processId, 0, __VA_ARGS__)

#define LOG(logLevel, code, message)                                                                                               \
    LOG_PID(logLevel, UINT_MAX, code, message)

#define LOG_ASSERT(message)                                                                                                        \
    LOG(logLevelAssert, errorTypeCode(&AssertError), message)
#define LOG_ERROR(code, message)                                                                                                   \
    LOG(logLevelError, code, message)
#define LOG_WARN(message)                                                                                                          \
    LOG(logLevelWarn, 0, message)
#define LOG_INFO(message)                                                                                                          \
    LOG(logLevelInfo, 0, message)
#define LOG_DETAIL(message)                                                                                                        \
    LOG(logLevelDetail, 0, message)
#define LOG_DEBUG(message)                                                                                                         \
    LOG(logLevelDebug, 0, message)
#define LOG_TRACE(message)                                                                                                         \
    LOG(logLevelTrace, 0, message)

#define LOG_FMT(logLevel, code, ...)                                                                                               \
    LOG_PID_FMT(logLevel, UINT_MAX, code, __VA_ARGS__)

#define LOG_ASSERT_FMT(...)                                                                                                        \
    LOG_FMT(logLevelAssert, errorTypeCode(&AssertError), __VA_ARGS__)
#define LOG_ERROR_FMT(code, ...)                                                                                                   \
    LOG_FMT(logLevelError, code, __VA_ARGS__)
#define LOG_WARN_FMT(...)                                                                                                          \
    LOG_FMT(logLevelWarn, 0, __VA_ARGS__)
#define LOG_INFO_FMT(...)                                                                                                          \
    LOG_FMT(logLevelInfo, 0, __VA_ARGS__)
#define LOG_DETAIL_FMT(...)                                                                                                        \
    LOG_FMT(logLevelDetail, 0, __VA_ARGS__)
#define LOG_DEBUG_FMT(...)                                                                                                         \
    LOG_FMT(logLevelDebug, 0, __VA_ARGS__)
#define LOG_TRACE_FMT(...)                                                                                                         \
    LOG_FMT(logLevelTrace, 0, __VA_ARGS__)

/***********************************************************************************************************************************
Internal Functions (in virtually all cases the macros above should be used in preference to these functions)
***********************************************************************************************************************************/
// Log function
void logInternal(
    LogLevel logLevel, LogLevel logRangeMin,  LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *message);

// Log function with formatting
void logInternalFmt(
    LogLevel logLevel, LogLevel logRangeMin,  LogLevel logRangeMax, unsigned int processId, const char *fileName,
    const char *functionName, int code, const char *format, ...) __attribute__((format(printf, 8, 9)));

#endif
