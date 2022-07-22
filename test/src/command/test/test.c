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
    const bool logTime, const String *const timeZone, bool repoCopy)
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
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create the data path
        const Storage *const storageHrnId = storagePosixNewP(strNewFmt("%s/data-%u", strZ(pathTest), vmId), .write = true);
        cmdTestExecLog = storagePathP(storageHrnId, STRDEF("exec.log"));

        cmdTestPathCreate(storageHrnId, NULL);

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
        cmdTestExec(strNewFmt("%s/build/none/src/build-code postgres %s/extra", strZ(pathTest), strZ(pathRepoCopy)));

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

                    // ??? These test types don't run yet
                    if (module->flag != NULL || module->containerRequired)
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
        if (binRequired)
        {
            LOG_INFO("build pgbackrest");
            cmdTestExec(strNewFmt("ninja -C %s/build/none src/pgbackrest", strZ(pathTest)));
        }

        // Process test list
        unsigned int errorTotal = 0;

        for (unsigned int moduleIdx = 0; moduleIdx < strLstSize(moduleList); moduleIdx++)
        {
            const String *const moduleName = strLstGet(moduleList, moduleIdx);
            const TestDefModule *const module = lstFind(testDef.moduleList, &moduleName);
            CHECK(AssertError, module != NULL, "unable to find module");

            TRY_BEGIN()
            {
                // Build unit test
                const TimeMSec buildTimeBegin = timeMSec();
                TestBuild *const testBld = testBldNew(
                    pathRepoCopy, pathTest, vm, vmId, module, test, scale, logLevel, logTime, timeZone);
                testBldUnit(testBld);

                // Create the test path
                const Storage *const storageTestId = storagePosixNewP(
                    strNewFmt("%s/test-%u", strZ(testBldPathTest(testBld)), testBldVmId(testBld)), .write = true);

                cmdTestPathCreate(storageTestId, NULL);

                // Meson setup
                const String *const pathUnit = strNewFmt("%s/unit-%u", strZ(pathTest), vmId);
                const String *const pathUnitBuild = strNewFmt("%s/build", strZ(pathUnit));

                if (!storageExistsP(testBldStorageTest(testBld), strNewFmt("%s/build.ninja", strZ(pathUnitBuild))))
                {
                    LOG_DETAIL("meson setup");
                    cmdTestExec(
                        strNewFmt(
                            "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug %s %s", strZ(pathUnitBuild),
                            strZ(pathUnit)));
                }

                // Ninja build
                cmdTestExec(strNewFmt("ninja -C %s", strZ(pathUnitBuild)));
                const TimeMSec buildTimeEnd = timeMSec();

                // Unit test
                const TimeMSec runTimeBegin = timeMSec();
                cmdTestExec(strNewFmt("%s/test-unit", strZ(pathUnitBuild)));
                const TimeMSec runTimeEnd = timeMSec();

                LOG_INFO_FMT(
                    "test unit %s (bld=%.3fs, run=%.3fs)", strZ(moduleName),
                    (double)(buildTimeEnd - buildTimeBegin) / (double)MSEC_PER_SEC,
                    (double)(runTimeEnd - runTimeBegin) / (double)MSEC_PER_SEC);
            }
            CATCH_ANY()
            {
                LOG_INFO_FMT("test unit %s", strZ(moduleName));
                LOG_ERROR(errorCode(), errorMessage());
                errorTotal++;
            }
            TRY_END();
        }

        // Return error
        if (errorTotal > 0)
            THROW_FMT(CommandError, "%u test failure(s)", errorTotal);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
