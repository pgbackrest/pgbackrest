/***********************************************************************************************************************************
Test Build Handler
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_BUILD_H
#define TEST_COMMAND_TEST_BUILD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TestBuild TestBuild;

#include "command/test/define.h"
#include "common/logLevel.h"
#include "common/type/object.h"
#include "common/type/string.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
TestBuild *testBldNew(
    const String *pathRepo, const String *pathTest, const String *const vm, unsigned int vmId, const TestDefModule *module,
    unsigned int test, uint64_t scale, LogLevel logLevel, bool logTime, const String *timeZone, bool coverage);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct TestBuildPub
{
    const String *pathRepo;                                         // Code repository path
    const String *pathTest;                                         // Test path
    const Storage *storageRepo;                                     // Repository storage
    const Storage *storageTest;                                     // Test storage
    const String *vm;                                               // Vm to run the test on
    unsigned int vmId;                                              // Vm id (0-based) to run the test on
    const TestDefModule *module;                                    // Module definition
    unsigned int test;                                              // Specific test to run (0 if all)
    LogLevel logLevel;                                              // Test log level
    bool logTime;                                                   // Log times/timestamps
    uint64_t scale;                                                 // Scale performance test
    const String *timeZone;                                         // Test in timezone
    bool coverage;                                                  // Generate coverage?
    TestDef tstDef;                                                 // Test definitions
} TestBuildPub;

// Repository path
__attribute__((always_inline)) static inline const String *
testBldPathRepo(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->pathRepo;
}

// Test path
__attribute__((always_inline)) static inline const String *
testBldPathTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->pathTest;
}

// Repository storage
__attribute__((always_inline)) static inline const Storage *
testBldStorageRepo(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->storageRepo;
}

// Test storage
__attribute__((always_inline)) static inline const Storage *
testBldStorageTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->storageTest;
}

// Vm
__attribute__((always_inline)) static inline const String *
testBldVm(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->vm;
}

// Vm id
__attribute__((always_inline)) static inline unsigned int
testBldVmId(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->vmId;
}


// Test Definition
__attribute__((always_inline)) static inline const TestDefModule *
testBldModule(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->module;
}

// Specific test to run
__attribute__((always_inline)) static inline unsigned int
testBldTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->test;
}

// Log level
__attribute__((always_inline)) static inline LogLevel
testBldLogLevel(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->logLevel;
}

// Log time/timestamps
__attribute__((always_inline)) static inline bool
testBldLogTime(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->logTime;
}

// Test in timezone
__attribute__((always_inline)) static inline const String *
testBldTimeZone(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->timeZone;
}

// Generate coverage?
__attribute__((always_inline)) static inline bool
testBldCoverage(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->coverage;
}

// Scale
__attribute__((always_inline)) static inline uint64_t
testBldScale(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->scale;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void testBldUnit(TestBuild *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
testBuildFree(TestBuild *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_TEST_BUILD_TYPE                                                                                               \
    TestBuild *
#define FUNCTION_LOG_TEST_BUILD_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "TestBuild", buffer, bufferSize)

#endif
