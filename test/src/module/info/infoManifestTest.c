/***********************************************************************************************************************************
Test Manifest Info Handler
***********************************************************************************************************************************/
#include "storage/posix/storage.h"
#include "info/infoBackup.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("infoManifestNew()"))
    {
        // Make sure the size of structs doesn't change without us knowing about it
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(sizeof(InfoManifestPath), TEST_64BIT() ? 32 : 20, "check size of InfoManifestPath");
        TEST_RESULT_UINT(sizeof(InfoManifestFile), TEST_64BIT() ? 88 : 64, "check size of InfoManifestFile");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoManifestNewLoad()"))
    {
        InfoManifest *manifest = NULL;

        String *manifestStr = strNew
        (
            "[target:path]\n"
            "pg_data={\"user\":\"user2\"}\n"
            "pg_data/base={\"group\":\"group2\"}\n"
            "pg_data/base/16384={\"group\":null,\"mode\":\"0750\"}\n"
            "pg_data/base/32768={}\n"
            "pg_data/base/65536={\"user\":null}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNew(INFO_MANIFEST_FILE ".expected")), harnessInfoChecksum(manifestStr)),
            "write manifest");

        TEST_ASSIGN(
            manifest, infoManifestNewLoad(storageTest, strNew(INFO_MANIFEST_FILE ".expected"), cipherTypeNone, NULL),
            "load manifest");
        TEST_RESULT_VOID(
            infoManifestSave(manifest, storageTest, strNew(INFO_MANIFEST_FILE ".actual"), cipherTypeNone, NULL), "save manifest");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, strNew(INFO_MANIFEST_FILE ".actual"))))),
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storageTest, strNew(INFO_MANIFEST_FILE ".expected"))))),
            "compare manifests");
    }
}
