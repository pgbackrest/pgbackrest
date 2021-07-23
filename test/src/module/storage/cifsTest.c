/***********************************************************************************************************************************
Test CIFS Storage
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
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoType, "cifs");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage configuration");

        const Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(0, true), "get cifs repo storage");
        TEST_RESULT_UINT(storageType(storage), STORAGE_CIFS_TYPE, "check storage type");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), true, "check path feature");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeatureCompress), true, "check compress feature");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write object path sync false");

        // Create a FileWrite object with path sync enabled and ensure that path sync is false in the write object
        StorageWrite *file = NULL;
        TEST_ASSIGN(file, storageNewWriteP(storage, STRDEF("somefile"), .noSyncPath = false), "new file write");

        TEST_RESULT_BOOL(storageWriteSyncPath(file), false, "path sync is disabled");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path sync result is noop");

        // Test the path sync function -- pass a bogus path to ensure that this is a noop
        TEST_RESULT_VOID(storagePathSyncP(storage, STRDEF(BOGUS_STR)), "path sync is a noop");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
