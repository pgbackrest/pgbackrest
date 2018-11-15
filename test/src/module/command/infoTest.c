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
    if (testBegin("infoRender() - json"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--output=json");
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(infoRender()), "[]\n", "json - no stanzas");

        // Empty stanza
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), strCat(backupPath, "/stanza1")), "create stanza1 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), strCat(archivePath, "/stanza1")), "create stanza1 directory");
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [],\n"
            "        \"backup\" : [],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [],\n"
            "        \"name\" : \"stanza1\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 3,\n"
            "            \"message\" : \"missing stanza data\"\n"
            "        }\n"
            "    }\n"
            "]\n", "missing stanza data");


        // backup.info file exists, but archive.info does not
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

        TEST_ERROR_FMT(infoRender(), FileMissingError,
            "unable to open %s/archive.info or %s/archive.info.copy\n"
            "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            strPtr(archivePath), strPtr(archivePath));

        // backup.info/archive.info files exist, ignoreMissing=false, no backup:current section so no valid backups
        //--------------------------------------------------------------------------------------------------------------------------
// CSHANG something is wrong - when I remove the checksum line and sha1sum it I get b1ded37a3496b1ea37b14f36a997fdb320498209 but the infoHash is looking for 02142fe712035d2ea89e6d3c604a011316931ed9. Is this because the infoHash is recreating the entire file as a json and we're not exactly doing that here? If so, is that correct?
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"02142fe712035d2ea89e6d3c604a011316931ed9\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665666,\"db-version\":\"9.3\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strCat(archivePath, "/archive.info")), bufNewStr(content)),
            "put archive info to file");

        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 2\n"
            "                },\n"
            "                \"id\" : \"9.4-2\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            }\n"
            "        ],\n"
            "        \"backup\" : [],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6569239123849665679,\n"
            "                \"version\" : \"9.4\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6569239123849665666,\n"
            "                \"version\" : \"9.3\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza1\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 2,\n"
            "            \"message\" : \"no valid backups\"\n"
            "        }\n"
            "    }\n"
            "]\n", "single stanza, no valid backups");
// CSHANG Maybe add another history here and then have backups and archives
        // // backup.info/archive.info files exist, but db history mismatch
        // //--------------------------------------------------------------------------------------------------------------------------
        // content = strNew
        // (
        //     "[backrest]\n"
        //     "backrest-checksum=\"51774ffab293c5cfb07511d7d2e101e92416f4ed\"\n"
        //     "backrest-format=5\n"
        //     "backrest-version=\"2.04\"\n"
        //     "\n"
        //     "[db]\n"
        //     "db-catalog-version=201409291\n"
        //     "db-control-version=942\n"
        //     "db-id=1\n"
        //     "db-system-id=6569239123849665679\n"
        //     "db-version=\"9.4\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
        //         "\"db-version\":\"9.4\"}\n"
        // );
        //
        // TEST_RESULT_VOID(
        //     storagePutNP(storageNewWriteNP(storageLocalWrite(), strCat(backupPath, "/backup.info")), bufNewStr(content)),
        //     "create db history id missmatch in backup info file");

        // Stanza not found
        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--stanza=silly");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"backup\" : [],\n"
            "        \"db\" : [],\n"
            "        \"name\" : \"silly\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 1,\n"
            "            \"message\" : \"missing stanza path\"\n"
            "        }\n"
            "    }\n"
            "]\n", "missing stanza path");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoRender() - text"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--output=text");
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(infoRender()),
            strPtr(strNewFmt("No stanzas exist in %s\n", strPtr(storagePathNP(storageRepo(), NULL)))), "text - no stanzas");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
