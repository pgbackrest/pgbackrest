/***********************************************************************************************************************************
Test Command

Perform a test.
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_TEST_H
#define TEST_COMMAND_TEST_TEST_H

#include "common/logLevel.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cmdTest(
    const String *pathRepo, const String *pathTest, const String *const vm, unsigned int vmId, const String *moduleName,
    unsigned int test, uint64_t scale, LogLevel logLevel, bool logTime, const String *timeZone);

#endif
