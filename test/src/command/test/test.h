/***********************************************************************************************************************************
Test Command

Perform a test.
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_TEST_H
#define TEST_COMMAND_TEST_TEST_H

#include "common/logLevel.h"
#include "common/type/stringList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cmdTest(
    const String *pathRepo, const String *pathTest, const String *const vm, unsigned int vmId, const StringList *moduleFilterList,
    unsigned int test, uint64_t scale, LogLevel logLevel, bool logTime, const String *timeZone, bool repoCopy);

#endif
