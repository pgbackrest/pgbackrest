/***********************************************************************************************************************************
Test Storage Commands
***********************************************************************************************************************************/
#include "common/io/bufferWrite.h"
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
        strLstAddZ(argList, "--output=text");
        strLstAddZ(argList, "ls");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Missing directory
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (text)");
        TEST_RESULT_STR(strNewBuf(output), "", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (json)");
        TEST_RESULT_STR(strNewBuf(output), "[]", "    check output");

        // Empty directory
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageTest, strNew("repo"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (text)");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), ".\n", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "empty directory (json)");
        TEST_RESULT_STR_STR(
            strNewBuf(output),
            strNewFmt(
                "[{\"name\":\".\",\"type\":\"path\",\"mode\":0750,\"user\":\"%s\",\"group\":\"%s\"}]", testUser(), testGroup()),
            "    check output");
        //
        // storagePathCreateNP(storageTest, strNew("repo/bbb"));
        // storagePutNP(storageNewWriteNP(storageTest, strNew("repo/aaa")), NULL);
        // TEST_RESULT_STR(strPtr(storageListRender()), "aaa\nbbb\n", "list files");
        //
        // StringList *argListTmp = strLstDup(argList);
        // strLstAddZ(argListTmp, "--filter=^aaa$");
        // harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        // TEST_RESULT_STR(strPtr(storageListRender()), "aaa\n", "list files with filter");
        //
        // argListTmp = strLstDup(argList);
        // strLstAddZ(argListTmp, "--sort=desc");
        // harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        // TEST_RESULT_STR(strPtr(storageListRender()), "bbb\naaa\n", "list files with filter and sort");
        //
        // storagePutNP(storageNewWriteNP(storageTest, strNew("repo/bbb/ccc")), NULL);
        // argListTmp = strLstDup(argList);
        // strLstAddZ(argListTmp, "bbb");
        // harnessCfgLoad(strLstSize(argListTmp), strLstPtr(argListTmp));
        // TEST_RESULT_STR(strPtr(storageListRender()), "ccc\n", "list files");

        // -------------------------------------------------------------------------------------------------------------------------
        // // Redirect stdout to a file
        // int stdoutSave = dup(STDOUT_FILENO);
        // String *stdoutFile = strNewFmt("%s/stdout.txt", testPath());
        //
        // THROW_ON_SYS_ERROR(freopen(strPtr(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");
        //
        // // Not in a test wrapper to avoid writing to stdout
        // cmdStorageList();
        //
        // // Restore normal stdout
        // dup2(stdoutSave, STDOUT_FILENO);
        //
        // TEST_RESULT_STR(strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, stdoutFile)))), "ccc\n", "    check text");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
