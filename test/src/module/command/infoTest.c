/***********************************************************************************************************************************
Test Info Command
***********************************************************************************************************************************/
#include "storage/driver/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create the repo directories
    String *repoPath = strNewFmt("%s/repo", testPath());
    String *archivePath = strNewFmt("%s/%s", strPtr(repoPath), STORAGE_PATH_ARCHIVE);
    String *backupPath = strNewFmt("%s/%s", strPtr(repoPath), STORAGE_PATH_BACKUP);
    storagePathCreateNP(storageLocalWrite(), archivePath);
    storagePathCreateNP(storageLocalWrite(), backupPath);

    // *****************************************************************************************************************************
    if (testBegin("infoRender()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--output=json");
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // No stanzas have been created
        TEST_RESULT_STR(strPtr(infoRender()), "[]\n", "empty json");

        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), strCat(backupPath, "/stanza1")), "create stanza1 directory");
        // TEST_RESULT_STR(strPtr(infoRender()),
        //     "[\n"
        //     "    {\n"
        //     "        \"name\" : \"stanza1\"\n"
        //     "    }\n"
        //     "]\n", "single stanza");  // CSHANG Flesh this test out for stanza without backup or archive info

        // File exists, ignoreMissing=false, no backup:current section
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"51774ffab293c5cfb07511d7d2e101e92416f4ed\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201306121,\"db-control-version\":937,\"db-system-id\":6569239123849665666,"
                "\"db-version\":\"9.3\"}\n"
            "2={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strCat(backupPath, "/backup.info")), bufNewStr(content)),
            "put backup info to file");

        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"name\" : \"stanza1\"\n"
            "    }\n"
            "]\n", "single stanza");  // CSHANG Flesh this test out for stanza without backup or archive info
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
