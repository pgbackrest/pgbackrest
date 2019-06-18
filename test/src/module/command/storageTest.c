/***********************************************************************************************************************************
Test Storage Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("cmdStorageList() and storageListRender()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s/repo", testPath()));
        strLstAddZ(argList, "ls");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(strPtr(storageListRender()), "", "missing directory");

        storagePathCreateNP(storageTest, strNew("repo"));
        TEST_RESULT_STR(strPtr(storageListRender()), "", "empty directory");

        storagePathCreateNP(storageTest, strNew("repo/bbb"));
        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/aaa")), NULL);
        TEST_RESULT_STR(strPtr(storageListRender()), "aaa\nbbb\n", "list files");

        StringList *argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "--filter=^aaa$");
        harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        TEST_RESULT_STR(strPtr(storageListRender()), "aaa\n", "list files with filter");

        argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "--sort=desc");
        harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        TEST_RESULT_STR(strPtr(storageListRender()), "bbb\naaa\n", "list files with filter and sort");

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/bbb/ccc")), NULL);
        argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "bbb");
        harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        TEST_RESULT_STR(strPtr(storageListRender()), "ccc\n", "list files");

        // -------------------------------------------------------------------------------------------------------------------------
        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.txt", testPath());

        THROW_ON_SYS_ERROR(freopen(strPtr(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdStorageList();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        TEST_RESULT_STR(strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, stdoutFile)))), "ccc\n", "    check text");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
