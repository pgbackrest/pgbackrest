/***********************************************************************************************************************************
Parse Define Yaml
***********************************************************************************************************************************/
#ifndef TEST_COMMAND_TEST_DEFINE_H
#define TEST_COMMAND_TEST_DEFINE_H

#include "common/type/list.h"
#include "common/type/string.h"
#include "storage/storage.h"

// Covered code modules
typedef struct TestDefCoverage
{
    const String *name;                                             // Code module name
    bool coverable;                                                 // Does this code module include coverable code?
    bool include;                                                   // Is this module included in another module?
} TestDefCoverage;

// Harness modules
typedef struct TestDefHarness
{
    const String *name;                                             // Harness module name
    const StringList *includeList;                                  // List of modules to include directly in harness
} TestDefHarness;

// Shimmed code modules
typedef struct TestDefShim
{
    const String *name;                                             // Shim module name
    const StringList *functionList;                                 // List of functions to shim
} TestDefShim;

typedef struct TestDefModule
{
    const String *name;                                             // Test module name
    unsigned int total;                                             // Total sub-tests
    bool binRequired;                                               // Is a binary required to run this test?
    bool containerRequired;                                         // Is a container required to run this test?
    const String *flag;                                             // Compilation flags
    const String *feature;                                          // Does this module introduce a feature?
    const StringList *featureList;                                  // Features to include in this module
    const List *coverageList;                                       // Code modules covered by this test module
    const StringList *dependList;                                   // Code modules that this test module depends on
    const StringList *includeList;                                  // Additional code modules to include in the test module
    const List *harnessList;                                        // Harnesses to use for this test
    const List *shimList;                                           // Shims for this test
} TestDefModule;

typedef struct TestDef
{
    const List *moduleList;                                         // Module list
} TestDef;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Parse define.yaml
TestDef testDefParse(const Storage *const storageRepo);

#endif
