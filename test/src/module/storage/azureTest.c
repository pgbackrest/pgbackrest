/***********************************************************************************************************************************
Test Azure Storage
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
    if (testBegin("storageAzureNew() and storageRepoGet()"))
    {
        // Only required options
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, "--" CFGOPT_REPO1_TYPE "=" STORAGE_TYPE_AZURE);
        strLstAddZ(argList, "--" CFGOPT_REPO1_PATH "=/repo");
        strLstAddZ(argList, "--" CFGOPT_REPO1_AZURE_CONTAINER "=test");
        setenv("PGBACKREST_" CFGOPT_REPO1_AZURE_ACCOUNT, "account", true);
        setenv("PGBACKREST_" CFGOPT_REPO1_AZURE_KEY, "key", true);
        harnessCfgLoad(cfgCmdArchivePush, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_AZURE), false), "getaAzure repo storage");
        TEST_RESULT_STR_Z(storage->path, "/repo", "    check path");
        // TEST_RESULT_STR(((StorageS3 *)storage->driver)->bucket, bucket, "    check bucket");
        // TEST_RESULT_STR(((StorageS3 *)storage->driver)->region, region, "    check region");
        // TEST_RESULT_STR(
        //     ((StorageS3 *)storage->driver)->bucketEndpoint, strNewFmt("%s.%s", strPtr(bucket), strPtr(endPoint)), "    check host");
        // TEST_RESULT_STR(((StorageS3 *)storage->driver)->accessKey, accessKey, "    check access key");
        // TEST_RESULT_STR(((StorageS3 *)storage->driver)->secretAccessKey, secretAccessKey, "    check secret access key");
        // TEST_RESULT_PTR(((StorageS3 *)storage->driver)->securityToken, NULL, "    check security token");
        // TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "    check path feature");
        // TEST_RESULT_BOOL(storageFeature(storage, storageFeatureCompress), false, "    check compress feature");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
