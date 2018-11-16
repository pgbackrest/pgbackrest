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
    String *archiveStanza1Path = strNewFmt("%s/stanza1", strPtr(archivePath));
    String *backupStanza1Path = strNewFmt("%s/stanza1", strPtr(backupPath));
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
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza1 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza1 directory");
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
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanza1Path))),
                bufNewStr(content)), "put backup info to file");

        TEST_ERROR_FMT(infoRender(), FileMissingError,
            "unable to open %s/archive.info or %s/archive.info.copy\n"
            "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            strPtr(archiveStanza1Path), strPtr(archiveStanza1Path));

        // backup.info/archive.info files exist, mismatched db ids, no backup:current section so no valid backups
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"0da11608456bae64c42cc1dc8df4ae79b953d597\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6569239123849665666,\"db-version\":\"9.3\"}\n"
            "3={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/archive.info", strPtr(archiveStanza1Path))),
                bufNewStr(content)), "put archive info to file");

        // archive section will reference backup db-id 2 but archive db-id 3
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 2\n"
            "                },\n"
            "                \"id\" : \"9.4-3\",\n"
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

        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb1 = strNewFmt("%s/9.4-1/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1), "create db1 archive WAL directories");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000001-a208321193820b409f6120496e76a0a4a6e19975.gz",
            strPtr(archiveDb1)))))), 0, "touch WAL file");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000002-a208321193820b409f6120496e76a0a4a6e19975.gz",
            strPtr(archiveDb1)))))), 0, "touch WAL file");

        String *archiveDb3 = strNewFmt("%s/9.4-3/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb3), "create db3 archive WAL directories");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000003-a208321193820b409f6120496e76a0a4a6e19975.gz",
            strPtr(archiveDb3)))))), 0, "touch WAL file");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"439b20775a642eb01c592b7d571d4013b362fcf2\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.04\"\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=3\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20181116-154756F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":2369190,\"backup-info-repo-size-delta\":2369190,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542383276,\"backup-timestamp-stop\":1542383289,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201306121,\"db-control-version\":937,\"db-system-id\":6569239123849665666,"
                "\"db-version\":\"9.3\"}\n"
            "3={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanza1Path))),
                bufNewStr(content)), "put backup info to file");


// CSHANG Need to figure out what the proper result should be

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
        //     "backrest-checksum=\"02142fe712035d2ea89e6d3c604a011316931ed9\"\n"
        //     "backrest-format=5\n"
        //     "backrest-version=\"2.04\"\n"
        //     "\n"
        //     "[db]\n"
        //     "db-id=2\n"
        //     "db-system-id=6569239123849665679\n"
        //     "db-version=\"9.4\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-id\":6569239123849665666,\"db-version\":\"9.3\"}\n"
        //     "2={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
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
