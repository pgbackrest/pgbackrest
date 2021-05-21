/***********************************************************************************************************************************
Log Test Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESSLOG_H
#define TEST_COMMON_HARNESSLOG_H

#include <stdbool.h>

#include "common/logLevel.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add log replacement
void hrnLogReplaceAdd(const char *expression, const char *expressionSub, const char *replacement, bool version);

// Clear (remove) all log replacements
void hrnLogReplaceClear(void);

void harnessLogResult(const char *expected);
void harnessLogResultRegExp(const char *expression);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Get/set log levels
unsigned int hrnLogLevelFile(void);
void hrnLogLevelFileSet(unsigned int logLevel);
unsigned int hrnLogLevelStdOut(void);
void hrnLogLevelStdOutSet(unsigned int logLevel);
unsigned int hrnLogLevelStdErr(void);
void hrnLogLevelStdErrSet(unsigned int logLevel);

// Get/set log timestamp
bool hrnLogTimestamp(void);
void hrnLogTimestampSet(bool log);

void harnessLogLevelReset(void);
void harnessLogLevelSet(LogLevel logLevel);

// Set the process id used for logging. Ignore the request if the logging module is not active yet.
#ifdef HRN_FEATURE_LOG
    void hrnLogProcessIdSet(unsigned int processId);
#else
    #define hrnLogProcessIdSet(processId)
#endif

/***********************************************************************************************************************************
Internal Setters
***********************************************************************************************************************************/
// Set dry-run on or off.  This is usually called only from the config harness.
void harnessLogDryRunSet(bool dryRun);

#endif
