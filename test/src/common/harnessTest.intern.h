/***********************************************************************************************************************************
C Test Harness Internal
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_INTERN_H
#define TEST_COMMON_HARNESS_INTERN_H

#include <stdbool.h>

#ifdef HRN_FEATURE_STRING
#include "common/type/stringList.h"
#endif

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
Getters/Setters
***********************************************************************************************************************************/
// Location of the data path were the harness can write data that won't be visible to the test
const char *hrnPath(void);

// Path to the source repository
const char *hrnPathRepo(void);

// Path where test data is written
const char *testPath(void);

// Location of the project exe
const char *testProjectExe(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void hrnInit(
    const char *testExe, const char *testProjectExe, bool testContainer, unsigned int testIdx, bool timing, const char *testPath,
    const char *testDataPath, const char *testRepoPath);
void hrnAdd(int run, bool selected);
void hrnComplete(void);

// Output test log title with line number
void hrnTestLogTitle(int lineNo);

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

#ifdef HRN_FEATURE_STRING
void hrnTestResultStringList(const StringList *actual, const char *expected, HarnessTestResultOperation operation);
#endif

void hrnTestResultUInt64(uint64_t actual, uint64_t expected, HarnessTestResultOperation operation);
void hrnTestResultUInt64Int64(uint64_t actual, int64_t expected, HarnessTestResultOperation operation);
void hrnTestResultZ(const char *actual, const char *expected, HarnessTestResultOperation operation);

#endif
