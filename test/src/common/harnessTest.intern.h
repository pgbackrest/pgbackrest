/***********************************************************************************************************************************
C Test Harness Internal
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_INTERN_H
#define TEST_COMMON_HARNESS_INTERN_H

#include <stdbool.h>

#include "common/type/stringList.h"

#include "common/harnessTest.h"

/***********************************************************************************************************************************
Test result operations
***********************************************************************************************************************************/
typedef enum
{
    harnessTestResultOperationEq,
    harnessTestResultOperationNe,
} HarnessTestResultOperation;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void hrnInit(
    const char *testExe, const char *testProjectExe, bool testContainer, unsigned int testIdx, bool timing, uint64_t testScale,
    const char *testPath, const char *testDataPath, const char *testRepoPath);
void hrnAdd(int run, bool selected);
void hrnComplete(void);

// Output test log prefix with timing, line number, and optional padding
void hrnTestLogPrefix(int lineNo, bool padding);

// Begin/end result test so an exception during the test will give a useful message about what happened and where
void hrnTestResultBegin(const char *statement, int lineNo, bool result);
bool hrnTestResultException(void);
void hrnTestResultEnd(void);

// Test results for various types
void hrnTestResultBool(int actual, int expected);
void hrnTestResultDouble(double actual, double expected);
void hrnTestResultInt64(int64_t actual, int64_t expected, HarnessTestResultOperation operation);
void hrnTestResultPtr(const void *actual, const void *expected, HarnessTestResultOperation operation);
void hrnTestResultStringList(const StringList *actual, const void *expected, HarnessTestResultOperation operation);
void hrnTestResultUInt64(uint64_t actual, uint64_t expected, HarnessTestResultOperation operation);
void hrnTestResultUInt64Int64(uint64_t actual, int64_t expected, HarnessTestResultOperation operation);
void hrnTestResultZ(const char *actual, const char *expected, HarnessTestResultOperation operation);

#endif
