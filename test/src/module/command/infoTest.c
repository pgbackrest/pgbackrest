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

    // *****************************************************************************************************************************
    if (testBegin("infoRender() - json"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--repo-path=%s", strPtr(repoPath)));
        strLstAddZ(argList, "--output=json");
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // No repo path
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoRender(), PathOpenError,
            "unable to open path '%s' for read: [2] No such file or directory", strPtr(backupPath));

        storagePathCreateNP(storageLocalWrite(), archivePath);
        storagePathCreateNP(storageLocalWrite(), backupPath);

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(infoRender()), "[]\n", "json - repo but no stanzas");

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
        // Only the current db information from the db:history will be processed.
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

        // Coverage for stanzaStatus branches
        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb1_1 = strNewFmt("%s/9.4-1/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_1), "create db1 archive WAL1 directory");

        String *archiveDb1_2 = strNewFmt("%s/9.4-1/0000000200000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_2), "create db1 archive WAL2 directory");

        String *archiveDb1_3 = strNewFmt("%s/9.4-1/0000000300000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_3), "create db1 archive WAL3 directory");

        String *archiveDb3 = strNewFmt("%s/9.4-3/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb3), "create db3 archive WAL1 directory");

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

        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
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
            "                    \"start\" : \"000000010000000000000001\",\n"
            "                    \"stop\" : \"000000010000000000000002\"\n"
            "                },\n"
            "                \"backrest\" : {\n"
            "                    \"format\" : 5,\n"
            "                    \"version\" : \"2.04\"\n"
            "                },\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"info\" : {\n"
            "                    \"delta\" : 20162900,\n"
            "                    \"repository\" : {\n"
            "                        \"delta\" : 2369190,\n"
            "                        \"size\" : 2369190\n"
            "                    },\n"
            "                    \"size\" : 20162900\n"
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
            "                \"id\" : 3,\n"
            "                \"system-id\" : 6569239123849665679,\n"
            "                \"version\" : \"9.4\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6569239123849665666,\n"
            "                \"version\" : \"9.3\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 1,\n"
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
            "]\n", "single stanza, valid backup, no priors, no archives in latest DB");

        // backup.info/archive.info files exist, backups exist, archives exist
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"075a202d42c3b6a0257da5f73a68fa77b342f777\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put archive info to file - stanza1");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"15bc3f8186a8d52a9d5549ace7bb69779bb39a5f\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put backup info to file - stanza1");

        String *archiveStanza2Path = strNewFmt("%s/stanza2", strPtr(archivePath));
        String *backupStanza2Path = strNewFmt("%s/stanza2", strPtr(backupPath));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza2 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza2 directory");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"6779d476833114925a73e058ef9ff04e5a8c7bd2\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put archive info to file - stanza2");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"2393c52cb48aff2d6c6e87e21a34a3e28200f42e\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put backup info to file - stanza2");

        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000002-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz",
            strPtr(archiveDb1_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strPtr(archiveDb1_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strPtr(strNewFmt("touch %s", strPtr(strNewFmt("%s/000000020000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strPtr(archiveDb1_2)))))), 0, "touch WAL2 file");

        TEST_RESULT_STR(strPtr(infoRender()),
            "[\n"
            "    {\n"
            "        \"archive\" : [\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 2\n"
            "                },\n"
            "                \"id\" : \"9.5-2\",\n"
            "                \"max\" : null,\n"
            "                \"min\" : null\n"
            "            },\n"
            "            {\n"
            "                \"database\" : {\n"
            "                    \"id\" : 1\n"
            "                },\n"
            "                \"id\" : \"9.4-1\",\n"
            "                \"max\" : \"000000020000000000000003\",\n"
            "                \"min\" : \"000000010000000000000002\"\n"
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
            "            }\n"
            "        ],\n"
            "        \"cipher\" : \"none\",\n"
            "        \"db\" : [\n"
            "            {\n"
            "                \"id\" : 2,\n"
            "                \"system-id\" : 6626363367545678089,\n"
            "                \"version\" : \"9.5\"\n"
            "            },\n"
            "            {\n"
            "                \"id\" : 1,\n"
            "                \"system-id\" : 6625592122879095702,\n"
            "                \"version\" : \"9.4\"\n"
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
            "]\n", "multiple stanzas, one with valid backups, archives in latest DB");

        // Stanza not found
        //--------------------------------------------------------------------------------------------------------------------------
        StringList *argList2 = strLstDup(argList);
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
            "]\n", "missing stanza path");

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
            "]\n", "multiple stanzas - one selected");
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

        // No repo path
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            infoRender(), PathOpenError,
            "unable to open path '%s' for read: [2] No such file or directory", strPtr(backupPath));

        storagePathCreateNP(storageLocalWrite(), archivePath);
        storagePathCreateNP(storageLocalWrite(), backupPath);

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(infoRender()),
            strPtr(strNewFmt("No stanzas exist in %s\n", strPtr(storagePathNP(storageRepo(), NULL)))), "text - no stanzas");

        // Empty stanza
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza1 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza1 directory");
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: error (missing stanza data)\n"
            "    cipher: none\n", "empty stanza");

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
        // Only the current db information from the db:history will be processed.
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

        // archive section will cross reference backup db-id 2 to archive db-id 3 but db section will only use the db-ids from
        // backup.info
        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-3): none present\n",
            "no backups");

        // Coverage test certain branches for formatTextDb
        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb1_1 = strNewFmt("%s/9.4-1/0000000100000000", strPtr(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveDb1_1), "create db1 archive WAL1 directory");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"73282374597bb031d4a436824df1290f667741b1\"\n"
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
            "\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542383276,\"backup-timestamp-stop\":1542383289,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181116-154799F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"000000010000000000000001\","
            "\"backup-info-repo-size\":2369190,\"backup-info-repo-size-delta\":2369190,"
            "\"backup-timestamp-start\":1542383276,\"backup-timestamp-stop\":1542383289,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageLocalWrite(), strNewFmt("%s/backup.info", strPtr(backupStanza1Path))),
                bufNewStr(content)), "put backup info to file");

        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-1): none present\n"
            "\n"
            "        full backup: 20181116-154756F\n"
            "            timestamp start/stop: 1542383276 / 1542383289\n"
            "            wal start/stop: n/a\n"
            "            database size: 20162900, backup size: 20162900\n"
            "            repository size: , repository backup size: \n"
            "\n"
            "        full backup: 20181116-154799F\n"
            "            timestamp start/stop: 1542383276 / 1542383289\n"
            "            wal start/stop: n/a\n"
            "            database size: , backup size: \n"
            "            repository size: 2369190, repository backup size: 2369190\n"
            ,"single stanza, valid backup, no priors, no archives in latest DB");

        // backup.info/archive.info files exist, backups exist, archives exist
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"075a202d42c3b6a0257da5f73a68fa77b342f777\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put archive info to file - stanza1");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"3ead4e7474d4e5b86bbd6dcc8d2143cf665442bd\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
            "\"backup-archive-stop\":\"000000010000000000000003\",\"backup-info-repo-size\":2369186,"
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
                bufNewStr(content)), "put backup info to file - stanza1");

        String *archiveStanza2Path = strNewFmt("%s/stanza2", strPtr(archivePath));
        String *backupStanza2Path = strNewFmt("%s/stanza2", strPtr(backupPath));
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), backupStanza1Path), "backup stanza2 directory");
        TEST_RESULT_VOID(storagePathCreateNP(storageLocalWrite(), archiveStanza1Path), "archive stanza2 directory");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"6779d476833114925a73e058ef9ff04e5a8c7bd2\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put archive info to file - stanza2");

        content = strNew
        (
            "[backrest]\n"
            "backrest-checksum=\"2393c52cb48aff2d6c6e87e21a34a3e28200f42e\"\n"
            "backrest-format=5\n"
            "backrest-version=\"2.08dev\"\n"
            "\n"
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
                bufNewStr(content)), "put backup info to file - stanza2");

        // Create WAL files
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

        TEST_RESULT_STR(strPtr(infoRender()),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5-2): none present\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 1542640898 / 1542640911\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n"
            "            database size: 20162900, backup size: 20162900\n"
            "            repository size: 2369186, repository backup size: 2369186\n"
            "\n"
            "        diff backup: 20181119-152138F_20181119-152152D\n"
            "            timestamp start/stop: 1542640912 / 1542640915\n"
            "            wal start/stop: 000000010000000000000003 / 000000010000000000000003\n"
            "            database size: 20162900, backup size: 8428\n"
            "            repository size: 2369186, repository backup size: 346\n"
            "            backup reference list: 20181119-152138F\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152152I\n"
            "            timestamp start/stop: 1542640912 / 1542640915\n"
            "            wal start/stop: 000000010000000000000003 / 000000010000000000000003\n"
            "            database size: 20162900, backup size: 8428\n"
            "            repository size: 2369186, repository backup size: 346\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "\n"
            "stanza: stanza2\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-1): none present\n"
            , "multiple stanzas, one with valid backups, archives in latest DB");
    }

    /*****************************************************************************************************************************/
    if (testBegin("formatTextDb()"))
    {
        // These tests cover branches not covered in other tests
        KeyValue *stanzaInfo = kvNew();
        VariantList *dbSection = varLstNew();
        Variant *pgInfo = varNewKv();
        kvPut(varKv(pgInfo), varNewStr(DB_KEY_ID_STR), varNewUInt64(1));
        kvPut(varKv(pgInfo), varNewStr(DB_KEY_SYSTEM_ID_STR), varNewUInt64(6625633699176220261));
        kvPut(varKv(pgInfo), varNewStr(DB_KEY_VERSION_STR), varNewStr(pgVersionToStr(90500)));

        varLstAdd(dbSection, pgInfo);

        // Add the database history, backup and archive sections to the stanza info
        kvPut(stanzaInfo, varNewStr(STANZA_KEY_DB_STR), varNewVarLst(dbSection));
        kvPut(stanzaInfo, varNewStr(STANZA_KEY_BACKUP_STR), varNewVarLst(varLstNew()));
        kvPut(stanzaInfo, varNewStr(KEY_ARCHIVE_STR), varNewVarLst(varLstNew()));

        String *result = strNew("");
        formatTextDb(stanzaInfo, result);

        TEST_RESULT_STR(strPtr(result), "\n    db (current)", "formatTextDb");
    }
    // CSHANG Not sure how this works
    // if (testBegin("cmdInfo()"))
    // {
    //     StringList *argList = strLstNew();
    //     strLstAddZ(argList, "/path/to/pgbackrest");
    //     TEST_RESULT_VOID(configParse(strLstSize(argList), strLstPtr(argList), false), "parse help from empty command line");
    //
    //     // Redirect stdout to a file
    //     int stdoutSave = dup(STDOUT_FILENO);
    //     String *stdoutFile = strNewFmt("%s/stdout.info", testPath());
    //
    //     if (freopen(strPtr(stdoutFile), "w", stdout) == NULL)                                       // {uncoverable - does not fail}
    //         THROW_SYS_ERROR(FileWriteError, "unable to reopen stdout");                             // {uncoverable+}
    //
    //     // Not in a test wrapper to avoid writing to stdout
    //     cmdInfo();
    //
    //     // Restore normal stdout
    //     dup2(stdoutSave, STDOUT_FILENO);
    //
    //     Storage *storage = storageDriverPosixInterface(
    //         storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL));
    //     TEST_RESULT_STR(strPtr(strNewBuf(storageGetNP(storageNewReadNP(storage, stdoutFile)))), generalHelp, "    check text");
    // }

    FUNCTION_HARNESS_RESULT_VOID();
}
