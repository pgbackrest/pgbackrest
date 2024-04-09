/***********************************************************************************************************************************
Test Build Handler
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_BUILD_H
#define TEST_COMMAND_TEST_BUILD_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TestBuild TestBuild;

#include "build/common/string.h"
#include "command/test/define.h"
#include "common/logLevel.h"
#include "common/type/object.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
TestBuild *testBldNew(
    const String *pathRepo, const String *pathTest, const String *const vm, const String *const vmInt, unsigned int vmId,
    const String *pgVersion, const TestDefModule *module, unsigned int test, uint64_t scale, LogLevel logLevel, bool logTime,
    const String *timeZone, bool coverage, bool profile, bool optimize, bool backTrace);

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
    const String *vmInt;                                            // Vm to run the test on for integration
    unsigned int vmId;                                              // Vm id (0-based) to run the test on
    const String *pgVersion;                                        // Pg version to run the test on
    const TestDefModule *module;                                    // Module definition
    unsigned int test;                                              // Specific test to run (0 if all)
    LogLevel logLevel;                                              // Test log level
    bool logTime;                                                   // Log times/timestamps
    uint64_t scale;                                                 // Scale performance test
    const String *timeZone;                                         // Test in timezone
    bool coverage;                                                  // Generate coverage?
    bool profile;                                                   // Generate profile report?
    bool optimize;                                                  // Optimize code?
    bool backTrace;                                                 // Run with back trace?
    TestDef tstDef;                                                 // Test definitions
} TestBuildPub;

// Repository path
FN_INLINE_ALWAYS const String *
testBldPathRepo(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->pathRepo;
}

// Test path
FN_INLINE_ALWAYS const String *
testBldPathTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->pathTest;
}

// Repository storage
FN_INLINE_ALWAYS const Storage *
testBldStorageRepo(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->storageRepo;
}

// Test storage
FN_INLINE_ALWAYS const Storage *
testBldStorageTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->storageTest;
}

// Vm
FN_INLINE_ALWAYS const String *
testBldVm(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->vm;
}

// Vm integration
FN_INLINE_ALWAYS const String *
testBldVmInt(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->vmInt;
}

// Vm id
FN_INLINE_ALWAYS unsigned int
testBldVmId(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->vmId;
}

// Pg version
FN_INLINE_ALWAYS const String *
testBldPgVersion(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->pgVersion;
}

// Test Definition
FN_INLINE_ALWAYS const TestDefModule *
testBldModule(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->module;
}

// Specific test to run
FN_INLINE_ALWAYS unsigned int
testBldTest(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->test;
}

// Log level
FN_INLINE_ALWAYS LogLevel
testBldLogLevel(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->logLevel;
}

// Log time/timestamps
FN_INLINE_ALWAYS bool
testBldLogTime(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->logTime;
}

// Test in timezone
FN_INLINE_ALWAYS const String *
testBldTimeZone(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->timeZone;
}

// Generate coverage?
FN_INLINE_ALWAYS bool
testBldCoverage(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->coverage;
}

// Generate profile repo?
FN_INLINE_ALWAYS bool
testBldProfile(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->profile;
}

// Optimize code?
FN_INLINE_ALWAYS bool
testBldOptimize(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->optimize;
}

// Scale
FN_INLINE_ALWAYS uint64_t
testBldScale(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->scale;
}

// Run with back trace?
FN_INLINE_ALWAYS bool
testBldBackTrace(const TestBuild *const this)
{
    return THIS_PUB(TestBuild)->backTrace;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void testBldUnit(TestBuild *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
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
    objNameToLog(value, "TestBuild", buffer, bufferSize)

#endif
