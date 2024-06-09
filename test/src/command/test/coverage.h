/***********************************************************************************************************************************
Coverage Testing and Reporting
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_COVERAGE_H
#define TEST_COMMAND_TEST_COVERAGE_H

#include "common/type/stringList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Generate coverage summary/report
int testCvgGenerate(
    const String *repoPath, const String *pathTest, const String *vm, bool coverageSummary, const StringList *moduleList);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TEST_COVERAGE_TYPE                                                                                            \
    TestCoverage *
#define FUNCTION_LOG_TEST_COVERAGE_FORMAT(value, buffer, bufferSize)                                                               \
    objNameToLog(value, "TestCoverage", buffer, bufferSize)

#endif
