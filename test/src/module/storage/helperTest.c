/***********************************************************************************************************************************
Test Storage Helper
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("storageLocal()"))
    {
        const Storage *storage = NULL;

        TEST_RESULT_PTR(storageLocalData, NULL, "local storage not cached");
        TEST_ASSIGN(storage, storageLocal(), "new storage");
        TEST_RESULT_PTR(storageLocalData, storage, "local storage cached");
        TEST_RESULT_PTR(storageLocal(), storage, "get cached storage");

        TEST_RESULT_STR(strPtr(storagePath(storage, NULL)), "/", "check base path");
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("storageSpool()"))
    {
        const Storage *storage = NULL;

        // Initialize config
        cfgInit();
        cfgOptionSet(cfgOptSpoolPath, cfgSourceConfig, varNewStr(strNew(testPath())));
        cfgOptionSet(cfgOptBufferSize, cfgSourceConfig, varNewInt(65536));
        cfgOptionSet(cfgOptStanza, cfgSourceConfig, varNewStr(strNew("db")));

        TEST_RESULT_PTR(storageSpoolData, NULL, "storage not cached");
        TEST_ASSIGN(storage, storageSpool(), "new storage");
        TEST_RESULT_PTR(storageSpoolData, storage, "storage cached");
        TEST_RESULT_PTR(storageSpool(), storage, "get cached storage");

        TEST_RESULT_STR(strPtr(storagePath(storage, NULL)), testPath(), "check base path");
        TEST_RESULT_STR(
            strPtr(storagePath(storage, strNew(STORAGE_SPOOL_ARCHIVE_OUT))), strPtr(strNewFmt("%s/archive/db/out", testPath())),
            "check spool out path");
        TEST_RESULT_STR(
            strPtr(storagePath(storage, strNewFmt("%s/%s", STORAGE_SPOOL_ARCHIVE_OUT, "file.ext"))),
            strPtr(strNewFmt("%s/archive/db/out/file.ext", testPath())), "check spool out path");

        TEST_ERROR(storagePath(storage, strNew("<" BOGUS_STR ">")), AssertError, "invalid expression '<BOGUS>'");
    }
}
