/***********************************************************************************************************************************
Test Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>

#include "build/common/exec.h"
#include "command/test/build.h"
#include "command/test/lint.h"
#include "command/test/test.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/posix/storage.h"

/**********************************************************************************************************************************/
static void
cmdTestPathCreate(const Storage *const storage, const String *const path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(storage != NULL);

    const String *const rmCommand = strNewFmt("rm -rf '%s'/*", strZ(storagePathP(storage, path)));

    TRY_BEGIN()
    {
        execOneP(rmCommand);
    }
    CATCH_ANY()
    {
        // Reset permissions
        execOneP(strNewFmt("chmod -R 777 '%s'", strZ(storagePathP(storage, path))));

        // Try to remove again
        execOneP(rmCommand);
    }
    TRY_END();

    storagePathCreateP(storage, path, .mode = 0770);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdTest(
    const String *const pathRepo, const String *const pathTest, const String *vm, const unsigned int vmId,
    const String *const pgVersion, const String *moduleName, const unsigned int test, const uint64_t scale, const LogLevel logLevel,
    const bool logTime, const String *const timeZone, const bool coverage, const bool profile, const bool optimize,
    const bool backTrace)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(UINT, vmId);
        FUNCTION_LOG_PARAM(STRING, pgVersion);
        FUNCTION_LOG_PARAM(STRING, moduleName);
        FUNCTION_LOG_PARAM(UINT, test);
        FUNCTION_LOG_PARAM(UINT64, scale);
        FUNCTION_LOG_PARAM(ENUM, logLevel);
        FUNCTION_LOG_PARAM(BOOL, logTime);
        FUNCTION_LOG_PARAM(STRING, timeZone);
        FUNCTION_LOG_PARAM(BOOL, coverage);
        FUNCTION_LOG_PARAM(BOOL, profile);
        FUNCTION_LOG_PARAM(BOOL, optimize);
        FUNCTION_LOG_PARAM(BOOL, backTrace);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Linter
        lintAll(pathRepo);

        // Find test
        ASSERT(moduleName != NULL);

        const TestDef testDef = testDefParse(storagePosixNewP(pathRepo));
        const TestDefModule *const module = lstFind(testDef.moduleList, &moduleName);

        CHECK_FMT(ParamInvalidError, module != NULL, "'%s' is not a valid test", strZ(moduleName));

        // Vm used for integration and the pgbackrest binary
        const String *const vmInt = vm;

        // If integration then the vm should be none. Integration tests do not run in containers but instead spawn their own.
        if (module->type == testDefTypeIntegration)
            vm = strNewZ("none");

        // Build test
        bool buildRetry = false;
        const String *const pathUnit = strNewFmt("%s/unit-%u/%s", strZ(pathTest), vmId, strZ(vm));
        const String *const pathUnitBuild = strNewFmt("%s/build", strZ(pathUnit));
        const Storage *const storageUnitBuild = storagePosixNewP(pathUnitBuild, .write = true);

        do
        {
            TRY_BEGIN()
            {
                // Build unit
                TestBuild *const testBld = testBldNew(
                    pathRepo, pathTest, vm, vmInt, vmId, pgVersion, module, test, scale, logLevel, logTime, timeZone, coverage,
                    profile, optimize, backTrace);
                testBldUnit(testBld);

                // Meson setup
                String *const mesonSetup = strCatZ(strNew(), "-Dbuildtype=");

                if (module->flag != NULL || profile || module->type == testDefTypePerformance)
                {
                    ASSERT(module->flag == NULL || strEqZ(module->flag, "-DNDEBUG"));
                    strCatZ(mesonSetup, "release");
                }
                else
                    strCatZ(mesonSetup, "debug");

                strCatFmt(mesonSetup, " -Db_coverage=%s", cvtBoolToConstZ(coverage));

                if (!storageExistsP(testBldStorageTest(testBld), strNewFmt("%s/build.ninja", strZ(pathUnitBuild))))
                {
                    LOG_DETAIL("meson setup");

                    execOneP(
                        strNewFmt(
                            "meson setup -Dwerror=true -Dfatal-errors=true %s '%s' '%s'", strZ(mesonSetup), strZ(pathUnitBuild),
                            strZ(pathUnit)));
                }
                // Else reconfigure
                else
                {
                    LOG_DETAIL("meson configure");

                    execOneP(strNewFmt("meson configure %s '%s'", strZ(mesonSetup), strZ(pathUnitBuild)));
                }

                // Remove old coverage data. Note that coverage can be in different paths depending on the meson version.
                const String *const pathCoverage =
                    storagePathExistsP(storageUnitBuild, STRDEF("test-unit.p")) ?
                        STRDEF("test-unit.p") : STRDEF("test-unit@exe");

                StorageIterator *const storageItr = storageNewItrP(
                    storageUnitBuild, pathCoverage, .expression = STRDEF("\\.gcda$"));

                while (storageItrMore(storageItr))
                {
                    storageRemoveP(
                        storageUnitBuild, strNewFmt("%s/%s", strZ(pathCoverage), strZ(storageItrNext(storageItr).name)));
                }

                // Remove old profile data
                storageRemoveP(storageUnitBuild, STRDEF("gmon.out"));

                // Ninja build
                execOneP(strNewFmt("ninja -C '%s'", strZ(pathUnitBuild)));
                buildRetry = false;
            }
            CATCH_ANY()
            {
                // If this is the first build failure then clean the build path and retry
                if (buildRetry == false)
                {
                    buildRetry = true;

                    LOG_WARN_FMT("build failed for unit %s -- will retry: %s", strZ(moduleName), errorMessage());
                    cmdTestPathCreate(storagePosixNewP(pathTest, .write = true), pathUnit);
                }
                // Else error
                else
                    THROWP_FMT(errorType(), "build failed for unit %s: %s", strZ(moduleName), errorMessage());
            }
            TRY_END();
        }
        while (buildRetry);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
