/***********************************************************************************************************************************
C Test Harness Internal
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_INTERN_H
#define TEST_COMMON_HARNESS_INTERN_H

#include <stdbool.h>

#include "common/harnessTest.h"

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

#endif
