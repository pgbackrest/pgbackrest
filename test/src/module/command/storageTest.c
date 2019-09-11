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
        strLstAddZ(argList, "--sort=asc");
        strLstAddZ(argList, "ls");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Missing directory
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (text)");
        TEST_RESULT_STR(strNewBuf(output), "", "    check output");

        // output = bufNew(0);
        // cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        // TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "missing directory (json)");
        // TEST_RESULT_STR(strNewBuf(output), "[]", "    check output");

        // Empty directory
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageTest, strNew("repo"), .mode = 0700);

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
                "["
                    "{\"name\":\".\",\"type\":\"path\",\"mode\":0700,\"user\":\"%s\",\"group\":\"%s\"}"
                "]",
                testUser(), testGroup()),
            "    check output");

        // Add path and file
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageTest, strNew("repo/bbb"));
        ASSERT(system(strPtr(strNewFmt("sudo chown :77777 %s/repo/bbb", testPath()))) == 0);
        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/aaa")), BUFSTRDEF("TESTDATA"));
        ASSERT(system(strPtr(strNewFmt("sudo chown 77777 %s/repo/aaa", testPath()))) == 0);
        ASSERT(system(strPtr(strNewFmt("sudo chmod 000 %s/repo/aaa", testPath()))) == 0);
        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/bbb/ccc")), BUFSTRDEF("TESTDATA2"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), ".\naaa\nbbb\n", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_STR(
            strNewBuf(output),
            strNewFmt(
                "["
                    "{\"name\":\".\",\"type\":\"path\",\"mode\":0700,\"user\":\"%s\",\"group\":\"%s\"}"
                    "{\"name\":\"aaa\",\"type\":\"file\",\"size\":8,\"group\":\"%s\"}"
                    "{\"name\":\"bbb\",\"type\":\"path\",\"mode\":0750,\"user\":\"%s\"}"
                "]",
                testUser(), testGroup(), testGroup(), testUser()),
            "    check output");

        // Reverse sort
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionSet(cfgOptSort, cfgSourceParam, VARSTRDEF("desc"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), "bbb\naaa\n.\n", "    check output");

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("json"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "path and file (text)");
        TEST_RESULT_STR_STR(
            strNewBuf(output),
            strNewFmt(
                "["
                    "{\"name\":\"bbb\",\"type\":\"path\",\"mode\":0750,\"user\":\"%s\"}"
                    "{\"name\":\"aaa\",\"type\":\"file\",\"size\":8,\"group\":\"%s\"}"
                    "{\"name\":\".\",\"type\":\"path\",\"mode\":0700,\"user\":\"%s\",\"group\":\"%s\"}"
                "]",
                testUser(), testGroup(), testGroup(), testUser()),
            "    check output");

        // Filter
        // -------------------------------------------------------------------------------------------------------------------------
        cfgOptionValidSet(cfgOptFilter, true);
        cfgOptionSet(cfgOptFilter, cfgSourceParam, VARSTRDEF("^aaa$"));

        output = bufNew(0);
        cfgOptionSet(cfgOptOutput, cfgSourceParam, VARSTRDEF("text"));
        TEST_RESULT_VOID(storageListRender(ioBufferWriteNew(output)), "filter");
        TEST_RESULT_STR(strPtr(strNewBuf(output)), "aaa\n", "    check output");

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
