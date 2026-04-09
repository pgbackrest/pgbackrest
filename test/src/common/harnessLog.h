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
// Init a finalize log
#ifdef HRN_FEATURE_LOG
void harnessLogInit(void);
void harnessLogFinal(void);
#endif

// Add log replacement
void hrnLogReplaceAdd(const char *expression, const char *expressionSub, const char *replacement, bool version);

// Remove a log replacement
void hrnLogReplaceRemove(const char *expression);

// Clear (remove) all log replacements
void hrnLogReplaceClear(void);

// Compare log to a static string
void harnessLogResult(const char *expected);

// Check that log contains a substring
void harnessLogResultEmptyOrContains(const char *const contains);

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
void harnessLogLevelDefaultSet(LogLevel logLevel);
#else
#define hrnLogProcessIdSet(processId)
#endif

/***********************************************************************************************************************************
Internal Setters
***********************************************************************************************************************************/
// Set dry-run on or off. This is usually called only from the config harness.
void harnessLogDryRunSet(bool dryRun);

#endif
