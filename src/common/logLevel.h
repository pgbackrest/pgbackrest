/***********************************************************************************************************************************
Log Levels
***********************************************************************************************************************************/
#ifndef COMMON_LOGLEVEL_H
#define COMMON_LOGLEVEL_H

/***********************************************************************************************************************************
Log types
***********************************************************************************************************************************/
typedef enum
{
    logLevelOff,
    logLevelAssert,
    logLevelError,
    logLevelWarn,
    logLevelInfo,
    logLevelDetail,
    logLevelDebug,
    logLevelTrace,
} LogLevel;

#define LOG_LEVEL_MIN                                               logLevelAssert
#define LOG_LEVEL_MAX                                               logLevelTrace

#endif
