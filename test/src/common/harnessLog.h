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
void hrnLogReplaceAdd(const char *expression, const char *expressionSub, const char *replacement, bool version);

void harnessLogResult(const char *expected);
void harnessLogResultRegExp(const char *expression);

/***********************************************************************************************************************************
Setters
***********************************************************************************************************************************/
void harnessLogLevelReset(void);
void harnessLogLevelSet(LogLevel logLevel);

#endif
