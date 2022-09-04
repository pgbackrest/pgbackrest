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
static const String *cmdTestExecLog;

static void
cmdTestExec(const String *const command)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
    FUNCTION_LOG_END();

    ASSERT(cmdTestExecLog != NULL);
    ASSERT(command != NULL);

    LOG_DETAIL_FMT("exec: %s", strZ(command));

    if (system(zNewFmt("%s > %s 2>&1", strZ(command), strZ(cmdTestExecLog))) != 0)
    {
        const Buffer *const buffer = storageGetP(
            storageNewReadP(storagePosixNewP(FSLASH_STR), cmdTestExecLog, .ignoreMissing = true));

        THROW_FMT(
            ExecuteError, "unable to execute: %s > %s 2>&1:%s", strZ(command), strZ(cmdTestExecLog),
            buffer == NULL || bufEmpty(buffer) ? " no log output" : zNewFmt("\n%s", strZ(strNewBuf(buffer))));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
cmdTestPathCreate(const Storage *const storage, const String *const path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);

    TRY_BEGIN()
    {
        storagePathRemoveP(storage, path, .recurse = true);
    }
    CATCH_ANY()
    {
        // Reset permissions
        cmdTestExec(strNewFmt("chmod -R 777 %s", strZ(storagePathP(storage, path))));

        // Try to remove again
        storagePathRemoveP(storage, path, .recurse = true);
    }
    TRY_END();

    storagePathCreateP(storage, path, .mode = 0770);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdTest(
    const String *const pathRepo, const String *const pathTest, const String *const vm, const unsigned int vmId,
    const StringList *moduleFilterList, const unsigned int test, const uint64_t scale, const LogLevel logLevel,
    const bool logTime, const String *const timeZone, const bool coverage, const bool profile, const bool optimize)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(UINT, vmId);
        FUNCTION_LOG_PARAM(STRING_LIST, moduleFilterList);
        FUNCTION_LOG_PARAM(UINT, test);
        FUNCTION_LOG_PARAM(UINT64, scale);
        FUNCTION_LOG_PARAM(ENUM, logLevel);
        FUNCTION_LOG_PARAM(BOOL, logTime);
        FUNCTION_LOG_PARAM(STRING, timeZone);
        FUNCTION_LOG_PARAM(BOOL, coverage);
        FUNCTION_LOG_PARAM(BOOL, profile);
        FUNCTION_LOG_PARAM(BOOL, optimize);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Log file name
        cmdTestExecLog = strNewFmt("%s/exec-%u.log", strZ(pathTest), vmId);

        // Find test
        ASSERT(!strLstEmpty(moduleFilterList));

        const TestDef testDef = testDefParse(storagePosixNewP(pathRepo));
        const String *const moduleName = strLstGet(moduleFilterList, 0);
        const TestDefModule *const module = lstFind(testDef.moduleList, &moduleName);

        if (module == NULL)
            THROW_FMT(ParamInvalidError, "'%s' is not a valid test", strZ(moduleName));

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
                    pathRepo, pathTest, vm, vmId, module, test, scale, logLevel, logTime, timeZone, coverage, profile, optimize);
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

                    cmdTestExec(
                        strNewFmt(
                            "meson setup -Dwerror=true -Dfatal-errors=true %s %s %s", strZ(mesonSetup), strZ(pathUnitBuild),
                            strZ(pathUnit)));
                }
                // Else reconfigure
                else
                {
                    LOG_DETAIL("meson configure");

                    cmdTestExec(strNewFmt("meson configure %s %s", strZ(mesonSetup), strZ(pathUnitBuild)));
                }

                // Remove old coverage data. Note that coverage can be in different paths depending on the meson version.
                const String *const pathCoverage = storagePathExistsP(storageUnitBuild, STRDEF("test-unit.p")) ?
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
                cmdTestExec(strNewFmt("ninja -C %s", strZ(pathUnitBuild)));
                buildRetry = false;
            }
            CATCH_ANY()
            {
                // If this is the first build failure then clean the build path a retry
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
