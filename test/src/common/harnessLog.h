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
void harnessLogLevelReset(void);
void harnessLogLevelSet(LogLevel logLevel);

/***********************************************************************************************************************************
Internal Setters
***********************************************************************************************************************************/
// Set dry-run on or off.  This is usually called only from the config harness.
void harnessLogDryRunSet(bool dryRun);

#endif
