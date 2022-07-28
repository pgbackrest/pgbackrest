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
    const bool logTime, const String *const timeZone, const bool repoCopy, const bool valgrind, const bool coverage, const bool run)
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
        FUNCTION_LOG_PARAM(BOOL, repoCopy);
        FUNCTION_LOG_PARAM(BOOL, valgrind);
        FUNCTION_LOG_PARAM(BOOL, coverage);
        FUNCTION_LOG_PARAM(BOOL, run);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Log file name
        cmdTestExecLog = strNewFmt("%s/exec-%u.log", strZ(pathTest), vmId);

        // Create data path
        if (run)
        {
            const Storage *const storageHrnId = storagePosixNewP(strNewFmt("%s/data-%u", strZ(pathTest), vmId), .write = true);
            cmdTestPathCreate(storageHrnId, NULL);
        }

        // Copy the source repository if requested (otherwise defaults to source code repository)
        const String *pathRepoCopy = pathRepo;

        if (repoCopy)
        {
            pathRepoCopy = strNewFmt("%s/repo", strZ(pathTest));
            const Storage *const storageRepoCopy = storagePosixNewP(pathRepoCopy, .write = true);

            LOG_DETAIL_FMT("sync repo to %s", strZ(pathRepoCopy));
            storagePathCreateP(storageRepoCopy, NULL, .mode = 0770);

            cmdTestExec(
                strNewFmt(
                    "git -C %s ls-files -c --others --exclude-standard | rsync -LtW --files-from=- %s/ %s", strZ(pathRepo),
                    strZ(pathRepo), strZ(pathRepoCopy)));
        }

        // Build code (??? better to do this only when it is needed)
        if (run)
            cmdTestExec(strNewFmt("%s/build/%s/src/build-code postgres %s/extra", strZ(pathTest), strZ(vm), strZ(pathRepoCopy)));

        // Build test list
        const TestDef testDef = testDefParse(storagePosixNewP(pathRepoCopy));
        StringList *const moduleList = strLstNew();
        bool binRequired = false;

        if (strLstEmpty(moduleFilterList))
        {
            StringList *const moduleFilterListEmpty = strLstNew();
            strLstAddZ(moduleFilterListEmpty, "");

            moduleFilterList = moduleFilterListEmpty;
        }

        for (unsigned int moduleFilterIdx = 0; moduleFilterIdx < strLstSize(moduleFilterList); moduleFilterIdx++)
        {
            const String *const moduleFilter = strLstGet(moduleFilterList, moduleFilterIdx);

            if (strEmpty(moduleFilter) || strEndsWithZ(moduleFilter, "/"))
            {
                bool found = false;

                for (unsigned int moduleIdx = 0; moduleIdx < lstSize(testDef.moduleList); moduleIdx++)
                {
                    const TestDefModule *const module = lstGet(testDef.moduleList, moduleIdx);

                    // ??? Container tests don not run yet
                    if (module->containerRequired)
                        continue;

                    if (strEmpty(moduleFilter) || strBeginsWith(module->name, moduleFilter))
                    {
                        strLstAddIfMissing(moduleList, module->name);
                        found = true;

                        if (module->binRequired)
                            binRequired = true;
                    }
                }

                if (!found)
                    THROW_FMT(ParamInvalidError, "'%s' prefix does not match any tests", strZ(moduleFilter));
            }
            else
            {
                const TestDefModule *const module = lstFind(testDef.moduleList, &moduleFilter);

                if (module == NULL)
                    THROW_FMT(ParamInvalidError, "'%s' is not a valid test", strZ(moduleFilter));

                strLstAddIfMissing(moduleList, module->name);
            }
        }

        // Build pgbackrest exe
        if (run && binRequired)
        {
            LOG_INFO("build pgbackrest");
            cmdTestExec(strNewFmt("ninja -C %s/build/none src/pgbackrest", strZ(pathTest)));
        }

        // Process test list
        unsigned int errorTotal = 0;
        bool buildRetry = false;
        String *const mesonSetupLast = strNew();

        for (unsigned int moduleIdx = 0; moduleIdx < strLstSize(moduleList); moduleIdx++)
        {
            const String *const moduleName = strLstGet(moduleList, moduleIdx);
            const TestDefModule *const module = lstFind(testDef.moduleList, &moduleName);
            CHECK(AssertError, module != NULL, "unable to find module");

            TRY_BEGIN()
            {
                // Build unit test
                const String *const pathUnit = strNewFmt("%s/unit-%u/%s", strZ(pathTest), vmId, strZ(vm));
                const String *const pathUnitBuild = strNewFmt("%s/build", strZ(pathUnit));
                const Storage *const storageUnitBuild = storagePosixNewP(pathUnitBuild, .write = true);
                const TimeMSec buildTimeBegin = timeMSec();
                TimeMSec buildTimeEnd;
                TestBuild *testBld;

                TRY_BEGIN()
                {
                    // Build unit
                    testBld = testBldNew(
                        pathRepoCopy, pathTest, vm, vmId, module, test, scale, logLevel, logTime, timeZone, coverage);
                    testBldUnit(testBld);

                    // Meson setup
                    String *const mesonSetup = strCatZ(strNew(), "-Dbuildtype=");

                    if (module->flag != NULL)
                    {
                        ASSERT(strEqZ(module->flag, "-DNDEBUG"));
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
                    // Else reconfigure as needed
                    else if (!strEq(mesonSetup, mesonSetupLast))
                    {
                        LOG_DETAIL("meson configure");

                        cmdTestExec(strNewFmt("meson configure %s %s", strZ(mesonSetup), strZ(pathUnitBuild)));
                    }

                    strCat(strTrunc(mesonSetupLast), mesonSetup);

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

                    // Ninja build
                    cmdTestExec(strNewFmt("ninja -C %s", strZ(pathUnitBuild)));
                    buildTimeEnd = timeMSec();

                    buildRetry = false;
                }
                CATCH_ANY()
                {
                    // If this is the first build failure then clean the build path a retry
                    if (buildRetry == false)
                    {
                        buildRetry = true;
                        moduleIdx--;

                        LOG_WARN_FMT("build failed for unit %s -- will retry: %s", strZ(moduleName), errorMessage());
                        cmdTestPathCreate(storagePosixNewP(pathTest, .write = true), pathUnit);
                    }
                    // Else error
                    else
                    {
                        buildRetry = false;
                        RETHROW();
                    }
                }
                TRY_END();

                // Skip test if build needs to be retried
                if (run && !buildRetry)
                {
                    // Create test path
                    const Storage *const storageTestId = storagePosixNewP(
                        strNewFmt("%s/test-%u", strZ(testBldPathTest(testBld)), testBldVmId(testBld)), .write = true);

                    cmdTestPathCreate(storageTestId, NULL);

                    // Unit test
                    const TimeMSec runTimeBegin = timeMSec();
                    String *const command = strNew();

                    if (valgrind)
                        strCatZ(command, "valgrind -q ");

                    strCatFmt(command, "%s/test-unit", strZ(pathUnitBuild));
                    cmdTestExec(command);

                    const TimeMSec runTimeEnd = timeMSec();

                    LOG_INFO_FMT(
                        "test unit %s (bld=%.3fs, run=%.3fs)", strZ(moduleName),
                        (double)(buildTimeEnd - buildTimeBegin) / (double)MSEC_PER_SEC,
                        (double)(runTimeEnd - runTimeBegin) / (double)MSEC_PER_SEC);
                }
            }
            CATCH_ANY()
            {
                LOG_ERROR_FMT(errorCode(), "test unit %s failed: %s", strZ(moduleName), errorMessage());
                errorTotal++;
            }
            TRY_END();
        }

        // Report errors
        if (errorTotal > 0)
            THROW_FMT(CommandError, "%u test failure(s)", errorTotal);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
