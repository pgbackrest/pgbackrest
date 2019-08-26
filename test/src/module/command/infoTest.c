/***********************************************************************************************************************************
Test Info Command
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create the repo directories
    String *repoPath = strNewFmt("%s/repo", testPath());
    String *archivePath = strNewFmt("%s/%s", strPtr(repoPath), "archive");
    String *backupPath = strNewFmt("%s/%s", strPtr(repoPath), "backup");
    String *archiveStanza1Path = strNewFmt("%s/stanza1", strPtr(archivePath));
    String *backupStanza1Path = strNewFmt("%s/stanza1", strPtr(backupPath));

    // *****************************************************************************************************************************
    if (testBegin("infoRender()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s/", strPtr(repoPath)));
        strLstAddZ(argList, "info");
        StringList *argListText = strLstDup(argList);

        strLstAddZ(argList, "--output=json");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(infoRender()), "[]\n", "json - repo but no stanzas");

        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_RESULT_STR(strPtr(infoRender()), "No stanzas exist in the repository.\n", "text - no stanzas");

        storagePathCreateNP(storageLocalWrite(), archivePath);
        storagePathCreateNP(storageLocalWrite(), backupPath);

        // Empty stanza
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza1 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza1 directory");
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: error (missing stanza data)\n"
            "    cipher: none\n", "text - missing stanza data");

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
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
            "]\n", "json - missing stanza data");

        // backup.info file exists, but archive.info does not
        //--------------------------------------------------------------------------------------------------------------------------
        String *content = strNew
        (
            "[cipher]\n"
            "cipher-pass=\"12345\"\n"
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
                harnessInfoChecksum(content)), "put backup info to file");

        TEST_ERROR_FMT(infoRender(), FileMissingError,
            "unable to load info file '%s/archive.info' or '%s/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            strPtr(archiveStanza1Path), strPtr(archiveStanza1Path),
            strPtr(strNewFmt("%s/archive.info", strPtr(archiveStanza1Path))),
            strPtr(strNewFmt("%s/archive.info.copy", strPtr(archiveStanza1Path))));

        // backup.info/archive.info files exist, mismatched db ids, no backup:current section so no valid backups
        // Only the current db information from the db:history will be processed.
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
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
                harnessInfoChecksum(content)), "put archive info to file");

        // archive section will cross reference backup db-id 2 to archive db-id 3 but db section will only use the db-ids from
        // backup.info
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
            "        \"cipher\" : \"aes-256-cbc\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6569239123849665666,\n"
            "                \"version\" : \"9.3\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6569239123849665679,\n"
            "                \"version\" : \"9.4\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza1\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 2,\n"
            "            \"message\" : \"no valid backups\"\n"
            "        }\n"
            "    }\n"
            "]\n", "json - single stanza, no valid backups");

        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: error (no valid backups)\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-3): none present\n",
            "text - single stanza, no valid backups");

        // Add WAL segments
        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb3 = strNewFmt("%s/9.4-3/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb3), "create db3 archive WAL1 directory");

        String *archiveDb3Wal = strNewFmt(
            "%s/000000010000000000000004-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz", strPtr(archiveDb3));
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageLocalWrite(), archiveDb3Wal), bufNew(0)), "touch WAL3 file");

        StringList *argList2 = strLstDup(argListText);
        strLstAddZ(argList2, "--stanza=stanza1");
        harnessCfgLoad(strLstSize(argList2), strLstPtr(argList2));

        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: error (no valid backups)\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-3): 000000010000000000000004/000000010000000000000004\n",
            "text - single stanza, one wal segment");

        TEST_RESULT_VOID(storageRemoveP(storageLocalWrite(), archiveDb3Wal, .errorOnMissing = true), "remove WAL file");

        // Coverage for stanzaStatus branches
        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb1_1 = strNewFmt("%s/9.4-1/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_1), "create db1 archive WAL1 directory");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000002-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz",
            strPtr(archiveDb1_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strPtr(archiveDb1_1)))))), 0, "touch WAL1 file");

        String *archiveDb1_2 = strNewFmt("%s/9.4-1/0000000200000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_2), "create db1 archive WAL2 directory");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000020000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strPtr(archiveDb1_2)))))), 0, "touch WAL2 file");

        String *archiveDb1_3 = strNewFmt("%s/9.4-1/0000000300000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_3), "create db1 archive WAL3 directory");

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        content = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=3\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20181116-154756F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":null,\"backup-archive-stop\":null,"
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1542383276,\"backup-timestamp-stop\":1542383289,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
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
                harnessInfoChecksum(content)), "put backup info to file");

        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"id\" : \"9.4-1\",\n"
            "                \"max\" : \"000000020000000000000003\",\n"
            "                \"min\" : \"000000010000000000000002\"\n"
            "            },\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 3\n"
            "                },\n"
            "                \"id\" : \"9.4-3\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            }\n"
            "        ],\n"
            "        \"backup\" : [\n"
            "            {\n"
            "                \"archive\" : {\n"
            "                    \"start\" : null,\n"
            "                    \"stop\" : null\n"
            "                },\n"
            "                \"backrest\" : {\n"
            "                    \"format\" : 5,\n"
            "                    \"version\" : \"2.04\"\n"
            "                },\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"info\" : {\n"
            "                    \"delta\" : 26897030,\n"
            "                    \"repository\" : {\n"
            "                        \"delta\" : 3159,\n"
            "                        \"size\" : 3159776\n"
            "                    },\n"
            "                    \"size\" : 26897030\n"
            "                },\n"
            "                \"label\" : \"20181116-154756F\",\n"
            "                \"prior\" : null,\n"
            "                \"reference\" : null,\n"
            "                \"timestamp\" : {\n"
            "                    \"start\" : 1542383276,\n"
            "                    \"stop\" : 1542383289\n"
            "                },\n"
            "                \"type\" : \"full\"\n"
            "            }\n"
            "        ],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6569239123849665679,\n"
            "                \"version\" : \"9.4\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6569239123849665666,\n"
            "                \"version\" : \"9.3\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 3,\n"
            "                \"system-id\" : 6569239123849665679,\n"
            "                \"version\" : \"9.4\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza1\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 0,\n"
            "            \"message\" : \"ok\"\n"
            "        }\n"
            "    }\n"
            "]\n", "json - single stanza, valid backup, no priors, no archives in latest DB");

        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        full backup: 20181116-154756F\n"
            "            timestamp start/stop: 2018-11-16 15:47:56 / 2018-11-16 15:48:09\n"
            "            wal start/stop: n/a\n"
            "            database size: 25.7MB, backup size: 25.7MB\n"
            "            repository size: 3MB, repository backup size: 3KB\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-3): none present\n"
            ,"text - single stanza, valid backup, no priors, no archives in latest DB");

        // backup.info/archive.info files exist, backups exist, archives exist
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/archive.info", strPtr(archiveStanza1Path))),
                harnessInfoChecksum(content)), "put archive info to file - stanza1");

        content = strNew
        (
            "[backup:current]\n"
            "20181119-152138F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640911,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152152D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-archive-stop\":\"000000010000000000000003\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152152I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F_20181119-152152D\","
            "\"backup-reference\":[\"20181119-152138F\",\"20181119-152138F_20181119-152152D\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"incr\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625592122879095702,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.5\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanza1Path))),
                harnessInfoChecksum(content)), "put backup info to file - stanza1");

        String *archiveStanza2Path = strNewFmt("%s/stanza2", strPtr(archivePath));
        String *backupStanza2Path = strNewFmt("%s/stanza2", strPtr(backupPath));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza2 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza2 directory");

        content = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6625633699176220261\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625633699176220261,\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/archive.info", strPtr(archiveStanza2Path))),
                harnessInfoChecksum(content)), "put archive info to file - stanza2");

        content = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625633699176220261\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625633699176220261,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanza2Path))),
                harnessInfoChecksum(content)), "put backup info to file - stanza2");

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"id\" : \"9.4-1\",\n"
            "                \"max\" : \"000000020000000000000003\",\n"
            "                \"min\" : \"000000010000000000000002\"\n"
            "            },\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 2\n"
            "                },\n"
            "                \"id\" : \"9.5-2\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            }\n"
            "        ],\n"
            "        \"backup\" : [\n"
            "            {\n"
            "                \"archive\" : {\n"
            "                    \"start\" : \"000000010000000000000002\",\n"
            "                    \"stop\" : \"000000010000000000000002\"\n"
            "                },\n"
            "                \"backrest\" : {\n"
            "                    \"format\" : 5,\n"
            "                    \"version\" : \"2.08dev\"\n"
            "                },\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"info\" : {\n"
            "                    \"delta\" : 20162900,\n"
            "                    \"repository\" : {\n"
            "                        \"delta\" : 2369186,\n"
            "                        \"size\" : 2369186\n"
            "                    },\n"
            "                    \"size\" : 20162900\n"
            "                },\n"
            "                \"label\" : \"20181119-152138F\",\n"
            "                \"prior\" : null,\n"
            "                \"reference\" : null,\n"
            "                \"timestamp\" : {\n"
            "                    \"start\" : 1542640898,\n"
            "                    \"stop\" : 1542640911\n"
            "                },\n"
            "                \"type\" : \"full\"\n"
            "            },\n"
            "            {\n"
            "                \"archive\" : {\n"
            "                    \"start\" : \"000000010000000000000003\",\n"
            "                    \"stop\" : \"000000010000000000000003\"\n"
            "                },\n"
            "                \"backrest\" : {\n"
            "                    \"format\" : 5,\n"
            "                    \"version\" : \"2.08dev\"\n"
            "                },\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"info\" : {\n"
            "                    \"delta\" : 8428,\n"
            "                    \"repository\" : {\n"
            "                        \"delta\" : 346,\n"
            "                        \"size\" : 2369186\n"
            "                    },\n"
            "                    \"size\" : 20162900\n"
            "                },\n"
            "                \"label\" : \"20181119-152138F_20181119-152152D\",\n"
            "                \"prior\" : \"20181119-152138F\",\n"
            "                \"reference\" : [\n"
            "                    \"20181119-152138F\"\n"
            "                ],\n"
            "                \"timestamp\" : {\n"
            "                    \"start\" : 1542640912,\n"
            "                    \"stop\" : 1542640915\n"
            "                },\n"
            "                \"type\" : \"diff\"\n"
            "            },\n"
            "            {\n"
            "                \"archive\" : {\n"
            "                    \"start\" : \"000000010000000000000003\",\n"
            "                    \"stop\" : null\n"
            "                },\n"
            "                \"backrest\" : {\n"
            "                    \"format\" : 5,\n"
            "                    \"version\" : \"2.08dev\"\n"
            "                },\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"info\" : {\n"
            "                    \"delta\" : 8428,\n"
            "                    \"repository\" : {\n"
            "                        \"delta\" : 346,\n"
            "                        \"size\" : 2369186\n"
            "                    },\n"
            "                    \"size\" : 20162900\n"
            "                },\n"
            "                \"label\" : \"20181119-152138F_20181119-152152I\",\n"
            "                \"prior\" : \"20181119-152138F_20181119-152152D\",\n"
            "                \"reference\" : [\n"
            "                    \"20181119-152138F\",\n"
            "                    \"20181119-152138F_20181119-152152D\"\n"
            "                ],\n"
            "                \"timestamp\" : {\n"
            "                    \"start\" : 1542640912,\n"
            "                    \"stop\" : 1542640915\n"
            "                },\n"
            "                \"type\" : \"incr\"\n"
            "            }\n"
            "        ],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6625592122879095702,\n"
            "                \"version\" : \"9.4\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6626363367545678089,\n"
            "                \"version\" : \"9.5\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza1\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 0,\n"
            "            \"message\" : \"ok\"\n"
            "        }\n"
            "    },\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"id\" : \"9.4-1\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            }\n"
            "        ],\n"
            "        \"backup\" : [],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6625633699176220261,\n"
            "                \"version\" : \"9.4\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza2\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 2,\n"
            "            \"message\" : \"no valid backups\"\n"
            "        }\n"
            "    }\n"
            "]\n", "json - multiple stanzas, one with valid backups, archives in latest DB");

        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 2018-11-19 15:21:38 / 2018-11-19 15:21:51\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n"
            "            database size: 19.2MB, backup size: 19.2MB\n"
            "            repository size: 2.3MB, repository backup size: 2.3MB\n"
            "\n"
            "        diff backup: 20181119-152138F_20181119-152152D\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: 000000010000000000000003 / 000000010000000000000003\n"
            "            database size: 19.2MB, backup size: 8.2KB\n"
            "            repository size: 2.3MB, repository backup size: 346B\n"
            "            backup reference list: 20181119-152138F\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152152I\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, backup size: 8.2KB\n"
            "            repository size: 2.3MB, repository backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5-2): none present\n"
            "\n"
            "stanza: stanza2\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-1): none present\n"
            , "text - multiple stanzas, one with valid backups, archives in latest DB");

        // Stanza not found
        //--------------------------------------------------------------------------------------------------------------------------
        argList2 = strLstDup(argList);
        strLstAddZ(argList2, "--stanza=silly");
        harnessCfgLoad(strLstSize(argList2), strLstPtr(argList2));
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
            "]\n", "json - missing stanza path");

        StringList *argListText2 = strLstDup(argListText);
        strLstAddZ(argListText2, "--stanza=silly");
        harnessCfgLoad(strLstSize(argListText2), strLstPtr(argListText2));
        TEST_RESULT_STR(strPtr(infoRender()),
        "stanza: silly\n"
        "    status: error (missing stanza path)\n"
        , "text - missing stanza path");

        // Stanza found
        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--stanza=stanza2");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"id\" : \"9.4-1\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            }\n"
            "        ],\n"
            "        \"backup\" : [],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6625633699176220261,\n"
            "                \"version\" : \"9.4\"\n"
            "            }\n"
            "        ],\n"
            "        \"name\" : \"stanza2\",\n"
            "        \"status\" : {\n"
            "            \"code\" : 2,\n"
            "            \"message\" : \"no valid backups\"\n"
            "        }\n"
            "    }\n"
            "]\n"
            , "json - multiple stanzas - selected found");

        strLstAddZ(argListText, "--stanza=stanza2");
        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza2\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-1): none present\n"
            ,"text - multiple stanzas - selected found");

        // Crypto error
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[global]\n"
            "repo-cipher-pass=123abc\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/pgbackrest.conf", testPath())),
                BUFSTR(content)), "put pgbackrest.conf file");
        strLstAddZ(argListText, "--repo-cipher-type=aes-256-cbc");
        strLstAdd(argListText, strNewFmt("--config=%s/pgbackrest.conf", testPath()));
        harnessCfgLoad(strLstSize(argListText), strLstPtr(argListText));
        TEST_ERROR_FMT(
            infoRender(), CryptoError,
            "unable to load info file '%s/backup.info' or '%s/backup.info.copy':\n"
            "CryptoError: '%s/backup.info' cipher header invalid\n"
            "HINT: Is or was the repo encrypted?\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use option --stanza if encryption settings are different for the stanza than the global settings",
            strPtr(backupStanza2Path), strPtr(backupStanza2Path), strPtr(backupStanza2Path),
            strPtr(strNewFmt("%s/backup.info.copy", strPtr(backupStanza2Path))));
    }

    //******************************************************************************************************************************
    if (testBegin("formatTextDb()"))
    {
        // These tests cover branches not covered in other tests
        KeyValue *stanzaInfo = kvNew();
        VariantList *dbSection = varLstNew();
        Variant *pgInfo = varNewKv(kvNew());
        kvPut(varKv(pgInfo), DB_KEY_ID_VAR, varNewUInt(1));
        kvPut(varKv(pgInfo), DB_KEY_SYSTEM_ID_VAR, varNewUInt64(6625633699176220261));
        kvPut(varKv(pgInfo), DB_KEY_VERSION_VAR, VARSTR(pgVersionToStr(90500)));

        varLstAdd(dbSection, pgInfo);

        // Add the database history, backup and archive sections to the stanza info
        kvPut(stanzaInfo, STANZA_KEY_DB_VAR, varNewVarLst(dbSection));

        VariantList *backupSection = varLstNew();
        Variant *backupInfo = varNewKv(kvNew());

        kvPut(varKv(backupInfo), BACKUP_KEY_LABEL_VAR, VARSTRDEF("20181119-152138F"));
        kvPut(varKv(backupInfo), BACKUP_KEY_TYPE_VAR, VARSTRDEF("full"));
        kvPutKv(varKv(backupInfo), KEY_ARCHIVE_VAR);
        KeyValue *infoInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_INFO_VAR);
        kvPut(infoInfo, KEY_SIZE_VAR, varNewUInt64(0));
        kvPut(infoInfo, KEY_DELTA_VAR, varNewUInt64(0));
        KeyValue *repoInfo = kvPutKv(infoInfo, INFO_KEY_REPOSITORY_VAR);
        kvAdd(repoInfo, KEY_SIZE_VAR, varNewUInt64(0));
        kvAdd(repoInfo, KEY_DELTA_VAR, varNewUInt64(0));
        KeyValue *databaseInfo = kvPutKv(varKv(backupInfo), KEY_DATABASE_VAR);
        kvAdd(databaseInfo, DB_KEY_ID_VAR, varNewUInt(1));
        KeyValue *timeInfo = kvPutKv(varKv(backupInfo), BACKUP_KEY_TIMESTAMP_VAR);
        kvAdd(timeInfo, KEY_START_VAR, varNewUInt64(1542383276));
        kvAdd(timeInfo, KEY_STOP_VAR, varNewUInt64(1542383289));

        varLstAdd(backupSection, backupInfo);

        kvPut(stanzaInfo, STANZA_KEY_BACKUP_VAR, varNewVarLst(backupSection));
        kvPut(stanzaInfo, KEY_ARCHIVE_VAR, varNewVarLst(varLstNew()));

        String *result = strNew("");
        formatTextDb(stanzaInfo, result);

        TEST_RESULT_STR(strPtr(result),
            "\n"
            "    db (current)\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 2018-11-16 15:47:56 / 2018-11-16 15:48:09\n"
            "            wal start/stop: n/a\n"
            "            database size: 0B, backup size: 0B\n"
            "            repository size: 0B, repository backup size: 0B\n"
            ,"formatTextDb only backup section (code coverage only)");
    }

    //******************************************************************************************************************************
    if (testBegin("cmdInfo()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePathCreateNP(storageLocalWrite(), archivePath);
        storagePathCreateNP(storageLocalWrite(), backupPath);

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.info", testPath());

        THROW_ON_SYS_ERROR(freopen(strPtr(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdInfo();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        Storage *storage = storagePosixNew(
            strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL);
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storage, stdoutFile)))), "No stanzas exist in the repository.\n",
            "    check text");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
