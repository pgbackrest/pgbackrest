/***********************************************************************************************************************************
Test Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>

#include "command/test/build.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/posix/storage.h"

/**********************************************************************************************************************************/
void
cmdTest(
    const String *const pathRepo, const String *const pathTest, const String *const vm, const unsigned int vmId,
    const String *const moduleName, const unsigned int test, const uint64_t scale, const LogLevel logLevel, const bool logTime,
    const String *const timeZone)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(UINT, vmId);
        FUNCTION_LOG_PARAM(STRING, moduleName);
        FUNCTION_LOG_PARAM(UINT, test);
        FUNCTION_LOG_PARAM(UINT64, scale);
        FUNCTION_LOG_PARAM(ENUM, logLevel);
        FUNCTION_LOG_PARAM(BOOL, logTime);
        FUNCTION_LOG_PARAM(STRING, timeZone);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build unit test
        TestBuild *const testBld = testBldNew(pathRepo, pathTest, vm, vmId, moduleName, test, scale, logLevel, logTime, timeZone);
        testBldUnit(testBld);

        // Remove and recreate the test path
        const Storage *const storageTestId = storagePosixNewP(
            strNewFmt("%s/test-%u", strZ(testBldPathTest(testBld)), testBldVmId(testBld)), .write = true);
        const char *const permReset = zNewFmt("chmod -R 777 %s", strZ(storagePathP(storageTestId, NULL)));

        if (system(permReset) != 0)
            THROW_FMT(ExecuteError, "unable to execute: %s", permReset);

        storagePathRemoveP(storageTestId, NULL, .recurse = true);
        storagePathCreateP(storageTestId, NULL, .mode = 0770);

        // Meson setup
        const String *const pathUnit = strNewFmt("%s/unit-%u", strZ(testBldPathTest(testBld)), testBldVmId(testBld));
        const String *const pathUnitBuild = strNewFmt("%s/build", strZ(pathUnit));

        if (!storageExistsP(testBldStorageTest(testBld), strNewFmt("%s/build.ninja", strZ(pathUnitBuild))))
        {
            const char *const mesonSetup = zNewFmt(
                "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug %s %s", strZ(pathUnitBuild), strZ(pathUnit));

            if (system(mesonSetup) != 0)
                THROW_FMT(ExecuteError, "unable to execute: %s", mesonSetup);
        }

        // Ninja build
        const char *const ninjaBuild = zNewFmt("ninja -C %s", strZ(pathUnitBuild));

        if (system(ninjaBuild) != 0)
            THROW_FMT(ExecuteError, "unable to execute: %s", ninjaBuild);

        // Unit test
        const char *const unitTest = zNewFmt("%s/test-unit", strZ(pathUnitBuild));

        if (system(unitTest) != 0)
            THROW_FMT(ExecuteError, "unable to execute: %s", unitTest);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
