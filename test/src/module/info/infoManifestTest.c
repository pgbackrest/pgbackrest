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
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true,\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false,\"checksum-page-error\":[1],\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true,\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true,\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/base/32768/33001={\"checksum\":\"6bf316f11d28c28914ea9be92c00de9bea6d9a6b\",\"checksum-page\":false,\"checksum-page-error\":[0,[3,5],7],\"size\":[SIZE],\"timestamp\":[TIMESTAMP-1]}\n"
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":[SIZE],\"timestamp\":[TIMESTAMP-2]}\n"
            "pg_data/special={\"master\":true,\"size\":0,\"timestamp\":[TIMESTAMP-1]}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "master=false\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
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
