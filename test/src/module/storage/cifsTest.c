/***********************************************************************************************************************************
Test CIFS Storage Driver
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("storageRepoGet() and StorageDriverCifs"))
    {
        // Load configuration
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--repo1-type=cifs");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        const Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_CIFS), true), "get cifs repo storage");
        TEST_RESULT_STR(strPtr(storage->type), "cifs", "check storage type");

        // Create a FileWrite object with path sync enabled and ensure that path sync is false in the write object
        // -------------------------------------------------------------------------------------------------------------------------
        StorageFileWrite *file = NULL;
        TEST_ASSIGN(file, storageNewWriteP(storage, strNew("somefile"), .noSyncPath = false), "new file write");

        TEST_RESULT_BOOL(storageFileWriteSyncPath(file), false, "path sync is disabled");

        // Test the path sync function -- pass a bogus path to ensure that this is a noop
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathSyncNP(storage, strNew(BOGUS_STR)), "path sync is a noop");

        // Free the storage object
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageDriverCifsFree((StorageDriverCifs *)storageDriver(storage)), "free cifs driver");
        TEST_RESULT_VOID(storageDriverCifsFree(NULL), "free null cifs driver");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
