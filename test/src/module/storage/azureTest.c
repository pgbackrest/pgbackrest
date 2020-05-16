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

    const String *account = STRDEF("act");
    const String *container = STRDEF("cnt");
    const String *key = STRDEF("YXpLZXk=");

    // *****************************************************************************************************************************
    if (testBegin("storageAzure*()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("storage with required options");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, "--" CFGOPT_REPO1_TYPE "=" STORAGE_TYPE_AZURE);
        strLstAddZ(argList, "--" CFGOPT_REPO1_PATH "=/repo");
        strLstAdd(argList, strNewFmt("--" CFGOPT_REPO1_AZURE_CONTAINER "=%s", strPtr(container)));
        setenv("PGBACKREST_" CFGOPT_REPO1_AZURE_ACCOUNT, strPtr(account), true);
        setenv("PGBACKREST_" CFGOPT_REPO1_AZURE_KEY, strPtr(key), true);
        harnessCfgLoad(cfgCmdArchivePush, argList);

        Storage *storage = NULL;
        TEST_ASSIGN(storage, storageRepoGet(strNew(STORAGE_TYPE_AZURE), false), "get repo storage");
        TEST_RESULT_STR_Z(storage->path, "/repo", "    check path");
        TEST_RESULT_STR(((StorageAzure *)storage->driver)->container, container, "    check container");
        TEST_RESULT_STR(((StorageAzure *)storage->driver)->account, account, "    check account");
        TEST_RESULT_STR(((StorageAzure *)storage->driver)->key, key, "    check key");
        TEST_RESULT_STR(
            ((StorageAzure *)storage->driver)->host, strNewFmt("%s.blob.core.windows.net", strPtr(account)), "    check host");
        TEST_RESULT_STR(
            ((StorageAzure *)storage->driver)->uriPrefix, strNewFmt("/%s", strPtr(container)), "    check uri prefix");
        TEST_RESULT_UINT(
            ((StorageAzure *)storage->driver)->blockSize, STORAGE_AZURE_BLOCKSIZE_MIN, "    check block size");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeaturePath), false, "    check path feature");
        TEST_RESULT_BOOL(storageFeature(storage, storageFeatureCompress), false, "    check compress feature");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
