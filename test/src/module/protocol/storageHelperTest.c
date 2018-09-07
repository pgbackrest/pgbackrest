/***********************************************************************************************************************************
Test Protocol Storage Helper
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    String *writeFile = strNewFmt("%s/writefile", testPath());

    // *****************************************************************************************************************************
    if (testBegin("protocolStorageHelperInit()"))
    {
        TEST_RESULT_PTR(protocolStorageHelper.memContext, NULL, "mem context not created");
        TEST_RESULT_VOID(protocolStorageHelperInit(), "create mem context");
        TEST_RESULT_BOOL(protocolStorageHelper.memContext != NULL, true, "mem context created");
        TEST_RESULT_VOID(protocolStorageHelperInit(), "reinit does nothing");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageRepo()"))
    {
        const Storage *storage = NULL;

        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_PTR(protocolStorageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(protocolStorageHelper.storageRepo, storage, "repo storage cached");
        TEST_RESULT_PTR(storageRepo(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(storagePathNP(storage, strNew("<BOGUS>/path")), AssertError, "invalid expression '<BOGUS>'");
        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), testPath(), "check base path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE))), strPtr(strNewFmt("%s/archive/db", testPath())),
            "check archive path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/simple"))),
            strPtr(strNewFmt("%s/archive/db/simple", testPath())), "check simple path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/700000007000000070000000"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/7000000070000000/700000007000000070000000", testPath())), "check segment path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/00000008.history"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/00000008.history", testPath())), "check history path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_REPO_ARCHIVE "/9.4-1/000000010000014C0000001A.00000028.backup"))),
            strPtr(strNewFmt("%s/archive/db/9.4-1/000000010000014C/000000010000014C0000001A.00000028.backup", testPath())),
            "check backup path");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
