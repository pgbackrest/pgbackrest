/***********************************************************************************************************************************
Test Storage Helper
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
    if (testBegin("storageLocal() and storageLocalWrite()"))
    {
        const Storage *storage = NULL;

        TEST_RESULT_PTR(storageHelper.storageLocal, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocal(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocal, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocal(), storage, "get cached storage");

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), "/", "check base path");

        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocalWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageLocalWrite, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocalWrite(), storage, "get cached storage");

        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), "/", "check base path");

        TEST_RESULT_VOID(storageNewWriteNP(storage, writeFile), "writes are allowed");
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

        TEST_RESULT_PTR(storageHelper.storageRepo, NULL, "repo storage not cached");
        TEST_ASSIGN(storage, storageRepo(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageRepo, storage, "repo storage cached");
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

        // Change the stanza name and make sure helper fails
        // -------------------------------------------------------------------------------------------------------------------------
        storageHelper.storageRepo = NULL;

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=other");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(storageRepo(), AssertError, "stanza has changed from 'db' to 'other'");
    }

    // *****************************************************************************************************************************
    if (testBegin("storageSpool() and storageSpoolWrite()"))
    {
        const Storage *storage = NULL;

        // Load configuration to set spool-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_PTR(storageHelper.storageSpool, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpool, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(storagePathNP(storage, NULL)), testPath(), "check base path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_SPOOL_ARCHIVE_OUT))), strPtr(strNewFmt("%s/archive/db/out", testPath())),
            "check spool out path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, "file.ext"))),
            strPtr(strNewFmt("%s/archive/db/out/file.ext", testPath())), "check spool out file");

        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNew(STORAGE_SPOOL_ARCHIVE_IN))), strPtr(strNewFmt("%s/archive/db/in", testPath())),
            "check spool in path");
        TEST_RESULT_STR(
            strPtr(storagePathNP(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_IN, "file.ext"))),
            strPtr(strNewFmt("%s/archive/db/in/file.ext", testPath())), "check spool in file");

        TEST_ERROR(storagePathNP(storage, strNew("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");

        TEST_ERROR(storageNewWriteNP(storage, writeFile), AssertError, "function debug assertion 'this->write' failed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpoolWrite(), "new storage");
        TEST_RESULT_PTR(storageHelper.storageSpoolWrite, storage, "storage cached");
        TEST_RESULT_PTR(storageSpoolWrite(), storage, "get cached storage");

        TEST_RESULT_VOID(storageNewWriteNP(storage, writeFile), "writes are allowed");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
