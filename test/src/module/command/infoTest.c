/***********************************************************************************************************************************
Test Info Command
***********************************************************************************************************************************/
#include "common/crypto/cipherBlock.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create storage object for writing to test locations when a stanza is not set
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // The tests expect the timezone to be UTC
    setenv("TZ", "UTC", true);

    // *****************************************************************************************************************************
    if (testBegin("infoRender()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        StringList *argListText = strLstDup(argList);

        hrnCfgArgRawZ(argList, cfgOptOutput, "json");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no stanzas have been created");

        TEST_RESULT_STR_Z(infoRender(), "[]", "json - repo but no stanzas");

        HRN_CFG_LOAD(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(infoRender(), "No stanzas exist in the repository.\n", "text - no stanzas");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo is still empty but stanza option is specified");

        StringList *argListStanzaOpt = strLstDup(argList);
        hrnCfgArgRawZ(argListStanzaOpt, cfgOptStanza, "stanza1");
        HRN_CFG_LOAD(cfgCmdInfo, argListStanzaOpt);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":[],"
                    "\"backup\":[],"
                    "\"cipher\":\"none\","
                    "\"db\":[],"
                    "\"name\":\"stanza1\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":1,"
                                "\"message\":\"missing stanza path\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":1,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"missing stanza path\""
                        "}"
                "}"
            "]",
            "json - empty repo, stanza option specified");

        StringList *argListTextStanzaOpt = strLstDup(argListText);
        hrnCfgArgRawZ(argListTextStanzaOpt, cfgOptStanza, "stanza1");
        HRN_CFG_LOAD(cfgCmdInfo, argListTextStanzaOpt);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing stanza path)\n",
            "text - empty repo, stanza option specified");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza path exists but is empty");

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .comment = "create repo stanza archive path");
        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP, .comment = "create repo stanza backup path");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing stanza data)\n"
            "    cipher: none\n",
            "text - missing stanza data");

        HRN_CFG_LOAD(cfgCmdInfo, argList);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":[],"
                    "\"backup\":[],"
                    "\"cipher\":\"none\","
                    "\"db\":[],"
                    "\"name\":\"stanza1\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":3,"
                                "\"message\":\"missing stanza data\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":3,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"missing stanza data\""
                        "}"
                "}"
            "]",
            "json - missing stanza data");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup.info file exists, but archive.info does not");

        // Put backup info to file
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/stanza1/" INFO_BACKUP_FILE,
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
                "\"db-version\":\"9.4\"}\n");

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":[],"
                    "\"backup\":[],"
                    "\"cipher\":\"none\","
                    "\"db\":[],"
                    "\"name\":\"stanza1\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":99,"
                                "\"message\":\"[FileMissingError] unable to load info file '"
                                TEST_PATH "/repo/archive/stanza1/archive.info' or '"
                                TEST_PATH "/repo/archive/stanza1/archive.info.copy':\\n"
                                "FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/stanza1/archive.info'"
                                " for read\\n"
                                "FileMissingError: unable to open missing file '" TEST_PATH
                                "/repo/archive/stanza1/archive.info.copy' for read\\n"
                                "HINT: archive.info cannot be opened but is required to push/get WAL segments.\\n"
                                "HINT: is archive_command configured correctly in postgresql.conf?\\n"
                                "HINT: has a stanza-create been performed?\\n"
                                "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate"
                                " archiving scheme.\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":99,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"other\""
                        "}"
                "}"
            "]",
            "json - other error, single repo");

        HRN_CFG_LOAD(cfgCmdInfo, argListTextStanzaOpt);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (other)\n"
            "            [FileMissingError] unable to load info file '" TEST_PATH "/repo/archive/stanza1/archive.info' or '"
                         TEST_PATH "/repo/archive/stanza1/archive.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/stanza1/archive.info' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/stanza1/archive.info.copy'"
                         " for read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
            " scheme.\n"
            "    cipher: none\n",
            "text - other error, single repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("info files exist with mismatched db-ids and no current backups - lock detected");

        // Only the current db information from the db:history will be processed.
        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=3\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6569239123849665666,\"db-version\":\"9.3\"}\n"
            "3={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n");

        // Create a WAL directory in 9.3-2 but since there are no WAL files or backups it will not show
        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/9.3-2/0000000100000000",
            .comment = "create empty db2 archive WAL1 directory");

        // archive section will cross reference backup db-id 2 to archive db-id 3 but db section will only use the db-ids from
        // backup.info. Execute while a backup lock is held.
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), STRDEF("stanza1"), STRDEF("999-ffffffff"), lockTypeBackup, 0, true),
                    -1, "create backup/expire lock");

                // Notify parent that lock has been acquired
                HRN_FORK_CHILD_NOTIFY_PUT();

                // Wait for parent to allow release lock
                HRN_FORK_CHILD_NOTIFY_GET();

                lockRelease(true);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Wait for child to acquire lock
                HRN_FORK_PARENT_NOTIFY_GET(0);

                HRN_CFG_LOAD(cfgCmdInfo, argList);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "["
                        "{"
                            "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":2,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.4-3\","
                                    "\"max\":null,"
                                    "\"min\":null"
                                "}"
                            "],"
                             "\"backup\":[],"
                             "\"cipher\":\"none\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6569239123849665666,"
                                    "\"version\":\"9.3\""
                                "},"
                                "{"
                                    "\"id\":2,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                            "\"name\":\"stanza1\","
                            "\"repo\":["
                                "{"
                                    "\"cipher\":\"none\","
                                    "\"key\":1,"
                                    "\"status\":{"
                                        "\"code\":2,"
                                        "\"message\":\"no valid backups\""
                                    "}"
                                "}"
                            "],"
                            "\"status\":{"
                                "\"code\":2,"
                                "\"lock\":{\"backup\":{\"held\":true}},"
                                "\"message\":\"no valid backups\""
                            "}"
                        "}"
                    "]",
                    "json - single stanza, no valid backups, backup/expire lock detected");

                HRN_CFG_LOAD(cfgCmdInfo, argListText);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "stanza: stanza1\n"
                    "    status: error (no valid backups, backup/expire running)\n"
                    "    cipher: none\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.4): none present\n",
                    "text - single stanza, no valid backups, backup/expire lock detected");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - stanza missing on specified repo");

        StringList *argList2 = strLstDup(argListTextStanzaOpt);
        hrnCfgArgKeyRawZ(argList2, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList2, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing stanza path)\n",
            "text - multi-repo, requested stanza missing on selected repo");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo - WAL segment on repo1");

        argList2 = strLstDup(argListTextStanzaOpt);
        hrnCfgArgKeyRawZ(argList2, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList2, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        // Add WAL segment
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/9.4-3/0000000300000000/000000030000000000000001-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            .comment = "write WAL db3 timeline 3 repo1");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): 000000030000000000000001/000000030000000000000001\n",
            "text - multi-repo, single stanza, one wal segment");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("coverage for stanzaStatus branches");

        // Db1 and Db3 (from above) have same system-id and db-version so consider them the same for WAL reporting
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000002-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz",
            .comment = "write WAL db1 timeline 1 repo1");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000100000000/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            .comment = "write WAL db1 timeline 1 repo1");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/9.4-1/0000000200000000/000000020000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            .comment = "write WAL db1 timeline 2 repo1");
        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/9.4-1/0000000300000000",
            .comment = "create empty db1 timeline 3 directory");

        // Create a WAL file in 9.3-2 so that a prior will show
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/9.3-2/0000000100000000/000000010000000000000001-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz",
            .comment = "write WAL db2 timeline 1 repo1");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
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
            "20201116-154900F={\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000030000000000000001\",\"backup-archive-stop\":\"000000030000000000000001\","
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159,\"backup-info-size\":26897033,"
            "\"backup-info-size-delta\":26897033,\"backup-timestamp-start\":1605541676,\"backup-timestamp-stop\":1605541680,"
            "\"backup-type\":\"full\",\"db-id\":3,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201306121,\"db-control-version\":937,\"db-system-id\":6569239123849665666,"
                "\"db-version\":\"9.3\"}\n"
            "3={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n");

        // Execute while a backup lock is held
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), STRDEF("stanza1"), STRDEF("777-afafafaf"), lockTypeBackup, 0, true),
                    -1, "create backup/expire lock");

                // Notify parent that lock has been acquired
                HRN_FORK_CHILD_NOTIFY_PUT();

                // Wait for parent to allow release lock
                HRN_FORK_CHILD_NOTIFY_GET();

                lockRelease(true);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Wait for child to acquire lock
                HRN_FORK_PARENT_NOTIFY_GET(0);

                HRN_CFG_LOAD(cfgCmdInfo, argList);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "["
                        "{"
                             "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.4-1\","
                                    "\"max\":\"000000020000000000000003\","
                                    "\"min\":\"000000010000000000000002\""
                                "},"
                                "{"
                                    "\"database\":{"
                                        "\"id\":2,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.3-2\","
                                    "\"max\":\"000000010000000000000001\","
                                    "\"min\":\"000000010000000000000001\""
                                "},"
                                "{"
                                    "\"database\":{"
                                        "\"id\":3,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.4-3\","
                                    "\"max\":\"000000030000000000000001\","
                                    "\"min\":\"000000030000000000000001\""
                                "}"
                            "],"
                             "\"backup\":["
                                "{"
                                    "\"archive\":{"
                                        "\"start\":null,"
                                        "\"stop\":null"
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.04\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":26897030,"
                                        "\"repository\":{"
                                            "\"delta\":3159,"
                                            "\"size\":3159776"
                                        "},"
                                        "\"size\":26897030"
                                    "},"
                                    "\"label\":\"20181116-154756F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1542383276,"
                                        "\"stop\":1542383289"
                                    "},"
                                    "\"type\":\"full\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000030000000000000001\","
                                        "\"stop\":\"000000030000000000000001\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.30\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":3,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":26897033,"
                                        "\"repository\":{"
                                            "\"delta\":3159,"
                                            "\"size\":3159776"
                                        "},"
                                        "\"size\":26897033"
                                    "},"
                                    "\"label\":\"20201116-154900F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1605541676,"
                                        "\"stop\":1605541680"
                                    "},"
                                    "\"type\":\"full\""
                                "}"
                            "],"
                             "\"cipher\":\"none\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "},"
                                "{"
                                    "\"id\":2,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6569239123849665666,"
                                    "\"version\":\"9.3\""
                                "},"
                                "{"
                                    "\"id\":3,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                            "\"name\":\"stanza1\","
                            "\"repo\":["
                                "{"
                                    "\"cipher\":\"none\","
                                    "\"key\":1,"
                                    "\"status\":{"
                                        "\"code\":0,"
                                        "\"message\":\"ok\""
                                    "}"
                                "}"
                            "],"
                            "\"status\":{"
                                "\"code\":0,"
                                "\"lock\":{\"backup\":{\"held\":true}},"
                                "\"message\":\"ok\""
                            "}"
                        "}"
                    "]",
                    "json - single stanza, valid backup, no priors, no archives in latest DB, backup/expire lock detected");

                HRN_CFG_LOAD(cfgCmdInfo, argListText);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "stanza: stanza1\n"
                    "    status: ok (backup/expire running)\n"
                    "    cipher: none\n"
                    "\n"
                    "    db (prior)\n"
                    "        wal archive min/max (9.3): 000000010000000000000001/000000010000000000000001\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.4): 000000010000000000000002/000000030000000000000001\n"
                    "\n"
                    "        full backup: 20181116-154756F\n"
                    "            timestamp start/stop: 2018-11-16 15:47:56 / 2018-11-16 15:48:09\n"
                    "            wal start/stop: n/a\n"
                    "            database size: 25.7MB, database backup size: 25.7MB\n"
                    "            repo1: backup set size: 3MB, backup size: 3KB\n"
                    "\n"
                    "        full backup: 20201116-154900F\n"
                    "            timestamp start/stop: 2020-11-16 15:47:56 / 2020-11-16 15:48:00\n"
                    "            wal start/stop: 000000030000000000000001 / 000000030000000000000001\n"
                    "            database size: 25.7MB, database backup size: 25.7MB\n"
                    "            repo1: backup set size: 3MB, backup size: 3KB\n",
                    "text - single stanza, valid backup, no priors, no archives in latest DB, backup/expire lock detected");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // Cleanup
        HRN_STORAGE_PATH_REMOVE(storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE "/stanza1/9.3-2", .recurse = true);
        HRN_STORAGE_PATH_REMOVE(storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE "/stanza1/9.4-3", .recurse = true);

        // backup.info/archive.info files exist, backups exist, archives exist, multi-repo (mixed) with one stanza existing on both
        // repos and the db history is different between the repos
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mixed multi-repo");

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE "/stanza1/" INFO_ARCHIVE_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625592122879095702,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678089,\"db-version\":\"9.5\"}\n",
            .comment = "put archive info to file - stanza1, repo1");

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/stanza1/" INFO_BACKUP_FILE,
            "[backup:current]\n"
            "20181119-152138F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":2369186,\"backup-info-repo-size-delta\":2369186,"
            "\"backup-info-size\":20162900,\"backup-info-size-delta\":20162900,"
            "\"backup-timestamp-start\":1542640898,\"backup-timestamp-stop\":1542640899,\"backup-type\":\"full\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152152D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-archive-stop\":\"000000020000000000000003\",\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F\",\"backup-reference\":[\"20181119-152138F\"],"
            "\"backup-timestamp-start\":1542640912,\"backup-timestamp-stop\":1542640915,\"backup-type\":\"diff\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20181119-152138F_20181119-152155I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.08dev\",\"backup-archive-start\":\"000000010000000000000003\","
            "\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20181119-152138F_20181119-152152D\","
            "\"backup-reference\":[\"20181119-152138F\",\"20181119-152138F_20181119-152152D\"],"
            "\"backup-timestamp-start\":1542640915,\"backup-timestamp-stop\":1542640917,\"backup-type\":\"incr\","
            "\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20201116-155000F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000003\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605541800,\"backup-timestamp-stop\":1605541802,"
            "\"backup-type\":\"full\",\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "20201116-155000F_20201119-152100I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000005\",\"backup-archive-stop\":\"000000010000000000000005\","
            "\"backup-info-repo-size\":2369186,"
            "\"backup-info-repo-size-delta\":346,\"backup-info-size\":20162900,\"backup-info-size-delta\":8428,"
            "\"backup-prior\":\"20201116-155000F\",\"backup-reference\":[\"20201116-155000F\"],"
            "\"backup-timestamp-start\":1605799260,\"backup-timestamp-stop\":1605799263,\"backup-type\":\"incr\","
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
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
                "\"db-version\":\"9.5\"}\n",
            .comment = "put backup info to file - stanza1, repo1");

        // Manifest with all features
        #define TEST_MANIFEST_HEADER                                                                                               \
            "[backup]\n"                                                                                                           \
            "backup-archive-start=\"000000030000028500000089\"\n"                                                                  \
            "backup-archive-stop=\"000000030000028500000089\"\n"                                                                   \
            "backup-label=\"20190818-084502F_20190820-084502D\"\n"                                                                 \
            "backup-lsn-start=\"285/89000028\"\n"                                                                                  \
            "backup-lsn-stop=\"285/89001F88\"\n"                                                                                   \
            "backup-prior=\"20190818-084502F\"\n"                                                                                  \
            "backup-timestamp-copy-start=1565282141\n"                                                                             \
            "backup-timestamp-start=1565282140\n"                                                                                  \
            "backup-timestamp-stop=1565282142\n"                                                                                   \
            "backup-type=\"full\"\n"                                                                                               \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201409291\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=1\n"                                                                                                            \
            "db-system-id=1000000000000000094\n"                                                                                   \
            "db-version=\"9.4\"\n"                                                                                                 \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=true\n"                                                                                          \
            "option-archive-copy=true\n"                                                                                           \
            "option-backup-standby=false\n"                                                                                        \
            "option-buffer-size=16384\n"                                                                                           \
            "option-checksum-page=true\n"                                                                                          \
            "option-compress=false\n"                                                                                              \
            "option-compress-level=3\n"                                                                                            \
            "option-compress-level-network=3\n"                                                                                    \
            "option-delta=false\n"                                                                                                 \
            "option-hardlink=false\n"                                                                                              \
            "option-online=false\n"                                                                                                \
            "option-process-max=32\n"

        #define TEST_MANIFEST_TARGET                                                                                               \
            "\n"                                                                                                                   \
            "[backup:target]\n"                                                                                                    \
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"                                                                  \
            "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"                         \
            "pg_data/pg_stat={\"path\":\"../pg_stat\",\"type\":\"link\"}\n"                                                        \
            "pg_tblspc/1={\"path\":\"/tblspc/ts1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"       \
            "pg_tblspc/12={\"path\":\"/tblspc/ts12\",\"tablespace-id\":\"12\",\"tablespace-name\":\"ts12\",\"type\":\"link\"}\n"

        #define TEST_MANIFEST_DB                                                                                                   \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"                                                                 \
            "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"                                                             \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        #define TEST_MANIFEST_FILE                                                                                                 \
            "\n"                                                                                                                   \
            "[target:file]\n"                                                                                                      \
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"                        \
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"                      \
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"          \
                ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"                        \
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"  \
                ",\"timestamp\":1565282115}\n"                                                                                     \
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"           \
                ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"                              \
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"     \
                ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"                                   \
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"     \
                ",\"timestamp\":1565282114}\n"                                                                                     \
            "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"

        #define TEST_MANIFEST_FILE_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "master=false\n"                                                                                                       \
            "mode=\"0600\"\n"                                                                                                      \
            "user=\"user1\"\n"

        #define TEST_MANIFEST_LINK                                                                                                 \
            "\n"                                                                                                                   \
            "[target:link]\n"                                                                                                      \
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"                                                                   \
            "pg_data/postgresql.conf={\"destination\":\"../pg_config/postgresql.conf\",\"group\":false,\"user\":\"user1\"}\n"

        #define TEST_MANIFEST_LINK_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:link:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "user=false\n"

        #define TEST_MANIFEST_PATH                                                                                                 \
            "\n"                                                                                                                   \
            "[target:path]\n"                                                                                                      \
            "pg_data={\"user\":\"user2\"}\n"                                                                                       \
            "pg_data/base={\"group\":\"group2\"}\n"                                                                                \
            "pg_data/base/16384={\"mode\":\"0750\"}\n"                                                                             \
            "pg_data/base/32768={}\n"                                                                                              \
            "pg_data/base/65536={\"user\":false}\n"

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=false\n"                                                                                                        \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"user1\"\n"

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/stanza1/20181119-152138F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "write manifest - stanza1, repo1");

        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE "/stanza2/" INFO_ARCHIVE_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6625633699176220261\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6625633699176220261,\"db-version\":\"9.4\"}\n",
            .comment = "put archive info to file - stanza2, repo1");
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_BACKUP "/stanza2/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6625633699176220261\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6625633699176220261,"
                "\"db-version\":\"9.4\"}\n",
            .comment = "put backup info to file - stanza2, repo1");

        // Write encrypted info files to encrypted repo2
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE "/stanza1/" INFO_ARCHIVE_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE "\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545678089,\"db-version\":\"9.5\"}\n",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS,
            .comment = "write encrypted archive.info, stanza1, repo2");
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_BACKUP "/stanza1/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[backup:current]\n"
            "20201116-200000F={\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000004\",\"backup-archive-stop\":\"000000010000000000000004\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605556800,\"backup-timestamp-stop\":1605556805,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":true,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":true,"
            "\"option-online\":true}\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.5\"}\n",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS,
            .comment = "write encrypted backup.info, stanza1, repo2");

        // Add WAL on repo1 and encrypted repo2 for stanza1
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-2/0000000100000000/000000010000000000000002-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-2/0000000100000000/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-2/0000000100000000/000000010000000000000004-ee61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-2/0000000100000000/000000010000000000000005-abc123f1ec7b1e6c3eaee9345214595eb7daa9a1.gz");

        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-1/0000000100000000/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE
            "/stanza1/9.5-1/0000000100000000/000000010000000000000004-ff61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz");

        // Add a manifest on the encrypted repo2
        #define TEST_MANIFEST_HEADER2                                                                                              \
            "[backup]\n"                                                                                                           \
            "backup-archive-start=\"000000010000000000000004\"\n"                                                                  \
            "backup-archive-stop=\"000000010000000000000004\"\n"                                                                   \
            "backup-label=\"20201116-200000F\"\n"                                                                                  \
            "backup-timestamp-copy-start=1605556800\n"                                                                             \
            "backup-timestamp-start=1605556800\n"                                                                                  \
            "backup-timestamp-stop=1605556802\n"                                                                                   \
            "backup-type=\"full\"\n"                                                                                               \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201510051\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=1\n"                                                                                                            \
            "db-system-id=6626363367545678089\n"                                                                                   \
            "db-version=\"9.5\"\n"                                                                                                 \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=true\n"                                                                                          \
            "option-archive-copy=true\n"                                                                                           \
            "option-backup-standby=true\n"                                                                                         \
            "option-buffer-size=16384\n"                                                                                           \
            "option-checksum-page=false\n"                                                                                         \
            "option-compress=false\n"                                                                                              \
            "option-compress-level=3\n"                                                                                            \
            "option-compress-level-network=3\n"                                                                                    \
            "option-delta=false\n"                                                                                                 \
            "option-hardlink=true\n"                                                                                               \
            "option-online=true\n"                                                                                                 \
            "option-process-max=32\n"                                                                                              \

        // Create encrypted manifest file
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_BACKUP "/stanza1/20201116-200000F/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER2
            TEST_MANIFEST_TARGET
            "\n"
            "[cipher]\n"
            "cipher-pass=\"someotherpass\"\n"
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .cipherType = cipherTypeAes256Cbc, .cipherPass = "somepass",
            .comment = "write encrypted manifest, stanza1, repo2");

        // Create a stanza on repo2 that is not on repo1
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE "/stanza3/" INFO_ARCHIVE_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE "\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545678089,\"db-version\":\"9.4\"}\n",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS,
            .comment = "write encrypted archive.info, repo2, stanza3");
        HRN_INFO_PUT(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_BACKUP "/stanza3/" INFO_BACKUP_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20201110-100000F={\"backrest-format\":5,\"backrest-version\":\"2.25\","
            "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605002400,\"backup-timestamp-stop\":1605002402,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":true,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":true,"
            "\"option-online\":true}\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.4\"}\n",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS,
            .comment = "write encrypted backup.info, repo2, stanza3");

        // Store some WAL in stanza on repo2 that is not on repo1
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE
            "/stanza3/9.4-1/0000000100000000/000000010000000000000001-11dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageTest, TEST_PATH "/repo2/" STORAGE_PATH_ARCHIVE
            "/stanza3/9.4-1/0000000100000000/000000010000000000000002-2261b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz");

        // Set up the configuration
        StringList *argListMultiRepo = strLstNew();
        hrnCfgArgRawZ(argListMultiRepo, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argListMultiRepo, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawStrId(argListMultiRepo, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);

        StringList *argListMultiRepoJson = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argListMultiRepoJson, cfgOptOutput, "json");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), STRDEF("stanza2"), STRDEF("999-ffffffff"), lockTypeBackup, 0, true),
                    -1, "create backup/expire lock");

                // Notify parent that lock has been acquired
                HRN_FORK_CHILD_NOTIFY_PUT();

                // Wait for parent to allow release lock
                HRN_FORK_CHILD_NOTIFY_GET();

                lockRelease(true);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Wait for child to acquire lock
                HRN_FORK_PARENT_NOTIFY_GET(0);

                HRN_CFG_LOAD(cfgCmdInfo, argListMultiRepoJson);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "["
                        "{"
                             "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.4-1\","
                                    "\"max\":\"000000020000000000000003\","
                                    "\"min\":\"000000010000000000000002\""
                                "},"
                                "{"
                                    "\"database\":{"
                                        "\"id\":2,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.5-2\","
                                    "\"max\":\"000000010000000000000005\","
                                    "\"min\":\"000000010000000000000002\""
                                "},"
                                "{"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":2"
                                    "},"
                                    "\"id\":\"9.5-1\","
                                    "\"max\":\"000000010000000000000004\","
                                    "\"min\":\"000000010000000000000003\""
                                "}"
                            "],"
                            "\"backup\":["
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000002\","
                                        "\"stop\":\"000000010000000000000002\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.08dev\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":20162900,"
                                        "\"repository\":{"
                                            "\"delta\":2369186,"
                                            "\"size\":2369186"
                                        "},"
                                        "\"size\":20162900"
                                    "},"
                                    "\"label\":\"20181119-152138F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1542640898,"
                                        "\"stop\":1542640899"
                                    "},"
                                    "\"type\":\"full\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000003\","
                                        "\"stop\":\"000000020000000000000003\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.08dev\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":8428,"
                                        "\"repository\":{"
                                            "\"delta\":346,"
                                            "\"size\":2369186"
                                        "},"
                                        "\"size\":20162900"
                                    "},"
                                    "\"label\":\"20181119-152138F_20181119-152152D\","
                                    "\"prior\":\"20181119-152138F\","
                                    "\"reference\":["
                                        "\"20181119-152138F\""
                                    "],"
                                    "\"timestamp\":{"
                                        "\"start\":1542640912,"
                                        "\"stop\":1542640915"
                                    "},"
                                    "\"type\":\"diff\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000003\","
                                        "\"stop\":null"
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.08dev\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":8428,"
                                        "\"repository\":{"
                                            "\"delta\":346,"
                                            "\"size\":2369186"
                                        "},"
                                        "\"size\":20162900"
                                    "},"
                                    "\"label\":\"20181119-152138F_20181119-152155I\","
                                    "\"prior\":\"20181119-152138F_20181119-152152D\","
                                    "\"reference\":["
                                        "\"20181119-152138F\","
                                        "\"20181119-152138F_20181119-152152D\""
                                    "],"
                                    "\"timestamp\":{"
                                        "\"start\":1542640915,"
                                        "\"stop\":1542640917"
                                    "},"
                                    "\"type\":\"incr\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000002\","
                                        "\"stop\":\"000000010000000000000003\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.30\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":2,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":26897020,"
                                        "\"repository\":{"
                                            "\"delta\":3100,"
                                            "\"size\":3159000"
                                        "},"
                                        "\"size\":26897000"
                                    "},"
                                    "\"label\":\"20201116-155000F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1605541800,"
                                        "\"stop\":1605541802"
                                    "},"
                                    "\"type\":\"full\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000004\","
                                        "\"stop\":\"000000010000000000000004\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.30\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":2"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":26897020,"
                                        "\"repository\":{"
                                            "\"delta\":3100,"
                                            "\"size\":3159000"
                                        "},"
                                        "\"size\":26897000"
                                    "},"
                                    "\"label\":\"20201116-200000F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1605556800,"
                                        "\"stop\":1605556805"
                                    "},"
                                    "\"type\":\"full\""
                                "},"
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000005\","
                                        "\"stop\":\"000000010000000000000005\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.30\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":2,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":8428,"
                                        "\"repository\":{"
                                            "\"delta\":346,"
                                            "\"size\":2369186"
                                        "},"
                                        "\"size\":20162900"
                                    "},"
                                    "\"label\":\"20201116-155000F_20201119-152100I\","
                                    "\"prior\":\"20201116-155000F\","
                                    "\"reference\":["
                                        "\"20201116-155000F\""
                                    "],"
                                    "\"timestamp\":{"
                                        "\"start\":1605799260,"
                                        "\"stop\":1605799263"
                                    "},"
                                    "\"type\":\"incr\""
                                "}"
                            "],"
                             "\"cipher\":\"mixed\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6625592122879095702,"
                                    "\"version\":\"9.4\""
                                "},"
                                "{"
                                    "\"id\":2,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6626363367545678089,"
                                    "\"version\":\"9.5\""
                                "},"
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":2,"
                                    "\"system-id\":6626363367545678089,"
                                    "\"version\":\"9.5\""
                                "}"
                            "],"
                            "\"name\":\"stanza1\","
                            "\"repo\":["
                                "{"
                                    "\"cipher\":\"none\","
                                    "\"key\":1,"
                                    "\"status\":{"
                                        "\"code\":0,"
                                        "\"message\":\"ok\""
                                    "}"
                                "},"
                                "{"
                                    "\"cipher\":\"aes-256-cbc\","
                                    "\"key\":2,"
                                    "\"status\":{"
                                        "\"code\":0,"
                                        "\"message\":\"ok\""
                                    "}"
                                "}"
                            "],"
                            "\"status\":{"
                                "\"code\":0,"
                                "\"lock\":{\"backup\":{\"held\":false}},"
                                "\"message\":\"ok\""
                            "}"
                        "},"
                        "{"
                             "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":1"
                                    "},"
                                    "\"id\":\"9.4-1\","
                                    "\"max\":null,"
                                    "\"min\":null"
                                "}"
                            "],"
                             "\"backup\":[],"
                             "\"cipher\":\"mixed\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":1,"
                                    "\"system-id\":6625633699176220261,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                            "\"name\":\"stanza2\","
                            "\"repo\":["
                                "{"
                                    "\"cipher\":\"none\","
                                    "\"key\":1,"
                                    "\"status\":{"
                                        "\"code\":2,"
                                        "\"message\":\"no valid backups\""
                                    "}"
                                "},"
                                "{"
                                    "\"cipher\":\"aes-256-cbc\","
                                    "\"key\":2,"
                                    "\"status\":{"
                                        "\"code\":1,"
                                        "\"message\":\"missing stanza path\""
                                    "}"
                                "}"
                            "],"
                            "\"status\":{"
                                "\"code\":4,"
                                "\"lock\":{\"backup\":{\"held\":true}},"
                                "\"message\":\"different across repos\""
                            "}"
                        "},"
                        "{"
                             "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":2"
                                    "},"
                                    "\"id\":\"9.4-1\","
                                    "\"max\":\"000000010000000000000002\","
                                    "\"min\":\"000000010000000000000001\""
                                "}"
                            "],"
                            "\"backup\":["
                                "{"
                                    "\"archive\":{"
                                        "\"start\":\"000000010000000000000001\","
                                        "\"stop\":\"000000010000000000000002\""
                                    "},"
                                    "\"backrest\":{"
                                        "\"format\":5,"
                                        "\"version\":\"2.25\""
                                    "},"
                                    "\"database\":{"
                                        "\"id\":1,"
                                        "\"repo-key\":2"
                                    "},"
                                    "\"info\":{"
                                        "\"delta\":26897020,"
                                        "\"repository\":{"
                                            "\"delta\":3100,"
                                            "\"size\":3159000"
                                        "},"
                                        "\"size\":26897000"
                                    "},"
                                    "\"label\":\"20201110-100000F\","
                                    "\"prior\":null,"
                                    "\"reference\":null,"
                                    "\"timestamp\":{"
                                        "\"start\":1605002400,"
                                        "\"stop\":1605002402"
                                    "},"
                                    "\"type\":\"full\""
                                "}"
                            "],"
                             "\"cipher\":\"mixed\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"repo-key\":2,"
                                    "\"system-id\":6626363367545678089,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                            "\"name\":\"stanza3\","
                            "\"repo\":["
                                "{"
                                    "\"cipher\":\"none\","
                                    "\"key\":1,"
                                    "\"status\":{"
                                        "\"code\":1,"
                                        "\"message\":\"missing stanza path\""
                                    "}"
                                "},"
                                "{"
                                    "\"cipher\":\"aes-256-cbc\","
                                    "\"key\":2,"
                                    "\"status\":{"
                                        "\"code\":0,"
                                        "\"message\":\"ok\""
                                    "}"
                                "}"
                            "],"
                            "\"status\":{"
                                "\"code\":4,"
                                "\"lock\":{\"backup\":{\"held\":false}},"
                                "\"message\":\"different across repos\""
                            "}"
                        "}"
                    "]",
                    "json - multiple stanzas, some with valid backups, archives in latest DB, backup lock held on one stanza");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), STRDEF("stanza2"), STRDEF("999-ffffffff"), lockTypeBackup, 0, true),
                    -1, "create backup/expire lock");

                // Notify parent that lock has been acquired
                HRN_FORK_CHILD_NOTIFY_PUT();

                // Wait for parent to allow release lock
                HRN_FORK_CHILD_NOTIFY_GET();

                lockRelease(true);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Wait for child to acquire lock
                HRN_FORK_PARENT_NOTIFY_GET(0);

                HRN_CFG_LOAD(cfgCmdInfo, argListMultiRepo);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "stanza: stanza1\n"
                    "    status: ok\n"
                    "    cipher: mixed\n"
                    "        repo1: none\n"
                    "        repo2: aes-256-cbc\n"
                    "\n"
                    "    db (prior)\n"
                    "        wal archive min/max (9.4): 000000010000000000000002/000000020000000000000003\n"
                    "\n"
                    "        full backup: 20181119-152138F\n"
                    "            timestamp start/stop: 2018-11-19 15:21:38 / 2018-11-19 15:21:39\n"
                    "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n"
                    "            database size: 19.2MB, database backup size: 19.2MB\n"
                    "            repo1: backup set size: 2.3MB, backup size: 2.3MB\n"
                    "\n"
                    "        diff backup: 20181119-152138F_20181119-152152D\n"
                    "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
                    "            wal start/stop: 000000010000000000000003 / 000000020000000000000003\n"
                    "            database size: 19.2MB, database backup size: 8.2KB\n"
                    "            repo1: backup set size: 2.3MB, backup size: 346B\n"
                    "            backup reference list: 20181119-152138F\n"
                    "\n"
                    "        incr backup: 20181119-152138F_20181119-152155I\n"
                    "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
                    "            wal start/stop: n/a\n"
                    "            database size: 19.2MB, database backup size: 8.2KB\n"
                    "            repo1: backup set size: 2.3MB, backup size: 346B\n"
                    "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.5): 000000010000000000000002/000000010000000000000005\n"
                    "\n"
                    "        full backup: 20201116-155000F\n"
                    "            timestamp start/stop: 2020-11-16 15:50:00 / 2020-11-16 15:50:02\n"
                    "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
                    "            database size: 25.7MB, database backup size: 25.7MB\n"
                    "            repo1: backup set size: 3MB, backup size: 3KB\n"
                    "\n"
                    "        full backup: 20201116-200000F\n"
                    "            timestamp start/stop: 2020-11-16 20:00:00 / 2020-11-16 20:00:05\n"
                    "            wal start/stop: 000000010000000000000004 / 000000010000000000000004\n"
                    "            database size: 25.7MB, database backup size: 25.7MB\n"
                    "            repo2: backup set size: 3MB, backup size: 3KB\n"
                    "\n"
                    "        incr backup: 20201116-155000F_20201119-152100I\n"
                    "            timestamp start/stop: 2020-11-19 15:21:00 / 2020-11-19 15:21:03\n"
                    "            wal start/stop: 000000010000000000000005 / 000000010000000000000005\n"
                    "            database size: 19.2MB, database backup size: 8.2KB\n"
                    "            repo1: backup set size: 2.3MB, backup size: 346B\n"
                    "            backup reference list: 20201116-155000F\n"
                    "\n"
                    "stanza: stanza2\n"
                    "    status: mixed (backup/expire running)\n"
                    "        repo1: error (no valid backups)\n"
                    "        repo2: error (missing stanza path)\n"
                    "    cipher: mixed\n"
                    "        repo1: none\n"
                    "        repo2: aes-256-cbc\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.4): none present\n"
                    "\n"
                    "stanza: stanza3\n"
                    "    status: mixed\n"
                    "        repo1: error (missing stanza path)\n"
                    "        repo2: ok\n"
                    "    cipher: mixed\n"
                    "        repo1: none\n"
                    "        repo2: aes-256-cbc\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.4): 000000010000000000000001/000000010000000000000002\n"
                    "\n"
                    "        full backup: 20201110-100000F\n"
                    "            timestamp start/stop: 2020-11-10 10:00:00 / 2020-11-10 10:00:02\n"
                    "            wal start/stop: 000000010000000000000001 / 000000010000000000000002\n"
                    "            database size: 25.7MB, database backup size: 25.7MB\n"
                    "            repo2: backup set size: 3MB, backup size: 3KB\n",
                    "text - multiple stanzas, multi-repo with valid backups, backup lock held on one stanza");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo: stanza exists but requested backup does not");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        hrnCfgArgRawZ(argList2, cfgOptSet, "bogus");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (requested backup not found)\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n",
            "text, multi-repo, backup not found");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo: backup set requested on single repo, with 1 checksum error");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        hrnCfgArgRawZ(argList2, cfgOptSet, "20181119-152138F_20181119-152155I");
        hrnCfgArgRawZ(argList2, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152155I\n"
            "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: mail (16456), postgres (12173)\n"
            "            symlinks:\n"
            "                pg_hba.conf => ../pg_config/pg_hba.conf\n"
            "                pg_stat => ../pg_stat\n"
            "            tablespaces:\n"
            "                ts1 (1) => /tblspc/ts1\n"
            "                ts12 (12) => /tblspc/ts12\n"
            "            page checksum error: base/16384/17000\n",
            "text - backup set requested");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo: read encrypted manifest and confirm requested database found without setting --repo");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        hrnCfgArgRawZ(argList2, cfgOptSet, "20201116-200000F");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000002/000000010000000000000005\n"
            "\n"
            "        full backup: 20201116-200000F\n"
            "            timestamp start/stop: 2020-11-16 20:00:00 / 2020-11-16 20:00:05\n"
            "            wal start/stop: 000000010000000000000004 / 000000010000000000000004\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo2: backup set size: 3MB, backup size: 3KB\n"
            "            database list: mail (16456), postgres (12173)\n"
            "            symlinks:\n"
            "                pg_hba.conf => ../pg_config/pg_hba.conf\n"
            "                pg_stat => ../pg_stat\n"
            "            tablespaces:\n"
            "                ts1 (1) => /tblspc/ts1\n"
            "                ts12 (12) => /tblspc/ts12\n"
            "            page checksum error: base/16384/17000\n",
            "text - multi-repo, backup set requested, found on repo2, report stanza and db over all repos");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("option 'set' not valid for json output");

        hrnCfgArgRawZ(argList2, cfgOptOutput, "json");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_ERROR(strZ(infoRender()), ConfigError, "option 'set' is currently only valid for text output");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup set requested but no links, multiple checksum errors");

        // Remove the environment variable so config system will only count one repo
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        argList2 = strLstDup(argListTextStanzaOpt);
        hrnCfgArgRawZ(argList2, cfgOptSet, "20181119-152138F_20181119-152155I");
        hrnCfgArgRawZ(argList2, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        #define TEST_MANIFEST_TARGET_NO_LINK                                                                                       \
            "\n"                                                                                                                   \
            "[backup:target]\n"                                                                                                    \
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"                                                                  \

        #define TEST_MANIFEST_FILE_MULTIPLE_CHECKSUM_ERRORS                                                                        \
        "\n"                                                                                                                       \
        "[target:file]\n"                                                                                                          \
        "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"                            \
            ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"                          \
        "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"              \
            ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"                            \
        "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"      \
            ",\"timestamp\":1565282115}\n"                                                                                         \
        "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":false"              \
            ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"                                  \
        "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"         \
            ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"                                       \
        "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"         \
            ",\"timestamp\":1565282114}\n"                                                                                         \
        "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"

        HRN_INFO_PUT(
             storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET_NO_LINK
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE_MULTIPLE_CHECKSUM_ERRORS
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = "write manifest with checksum errors and no links");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152155I\n"
            "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: mail (16456), postgres (12173)\n"
            "            page checksum error: base/16384/17000, base/32768/33000\n",
            "text - backup set requested, no links");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup set requested but no databases, no checksum error");

        // Using the same configuration from previous test
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        #define TEST_MANIFEST_NO_DB                                                                                                \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        #define TEST_MANIFEST_FILE_NO_CHECKSUM_ERROR                                                                               \
        "\n"                                                                                                                       \
        "[target:file]\n"                                                                                                          \
        "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"                            \
            ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"                          \
        "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":true"               \
            ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"                            \
        "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"      \
            ",\"timestamp\":1565282115}\n"                                                                                         \
        "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"               \
            ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"                                  \
        "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"         \
            ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"                                       \
        "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"         \
            ",\"timestamp\":1565282114}\n"                                                                                         \
        "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"

        HRN_INFO_PUT(
             storageRepoWrite(), STORAGE_REPO_BACKUP "/20181119-152138F_20181119-152155I/" BACKUP_MANIFEST_FILE,
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET_NO_LINK
            TEST_MANIFEST_NO_DB
            TEST_MANIFEST_FILE_NO_CHECKSUM_ERROR
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT,
            .comment = " rewrite same manifest withut checksum errors");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152155I\n"
            "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: none\n",
            "text - backup set requested, no db and no checksum error");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo: stanza found");

        // Reconfigure environment variable for repo2
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);

        argList2 = strLstDup(argListMultiRepoJson);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza2");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1,"
                                "\"repo-key\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":null,"
                            "\"min\":null"
                        "}"
                    "],"
                     "\"backup\":[],"
                     "\"cipher\":\"mixed\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"repo-key\":1,"
                            "\"system-id\":6625633699176220261,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                    "\"name\":\"stanza2\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":2,"
                                "\"message\":\"no valid backups\""
                            "}"
                        "},"
                        "{"
                            "\"cipher\":\"aes-256-cbc\","
                            "\"key\":2,"
                            "\"status\":{"
                                "\"code\":1,"
                                "\"message\":\"missing stanza path\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":4,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"different across repos\""
                    "}"
                "}"
            "]",
            "json - multiple stanzas - selected found, repo1");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza2");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza2\n"
            "    status: mixed\n"
            "        repo1: error (no valid backups)\n"
            "        repo2: error (missing stanza path)\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): none present\n",
            "text - multiple stanzas - selected found, repo1");

        // Remove backups from repo2 for stanza1 so multi-repos are scanned but backups are on only 1 repo
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo, backups only on one");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6626363367545678089\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.5\"}\n",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS,
            .comment = "backup.info without current, repo2, stanza1");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: mixed\n"
            "        repo1: ok\n"
            "        repo2: error (no valid backups)\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 2018-11-19 15:21:38 / 2018-11-19 15:21:39\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n"
            "            database size: 19.2MB, database backup size: 19.2MB\n"
            "            repo1: backup set size: 2.3MB, backup size: 2.3MB\n"
            "\n"
            "        diff backup: 20181119-152138F_20181119-152152D\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: 000000010000000000000003 / 000000020000000000000003\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152155I\n"
            "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000002/000000010000000000000005\n"
            "\n"
            "        full backup: 20201116-155000F\n"
            "            timestamp start/stop: 2020-11-16 15:50:00 / 2020-11-16 15:50:02\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "        incr backup: 20201116-155000F_20201119-152100I\n"
            "            timestamp start/stop: 2020-11-19 15:21:00 / 2020-11-19 15:21:03\n"
            "            wal start/stop: 000000010000000000000005 / 000000010000000000000005\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20201116-155000F\n",
            "text - multi-repo, valid backups only on repo1");

        // Remove archives for prior backup so archiveMin prior DB == NULL but backupList > 0 (edge case)
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo, prior backup: no archives but backups (code coverage)");

        HRN_STORAGE_PATH_REMOVE(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/9.4-1", .recurse = true, .comment = "remove archives on db prior");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: mixed\n"
            "        repo1: ok\n"
            "        repo2: error (no valid backups)\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): none present\n"
            "\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 2018-11-19 15:21:38 / 2018-11-19 15:21:39\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000002\n"
            "            database size: 19.2MB, database backup size: 19.2MB\n"
            "            repo1: backup set size: 2.3MB, backup size: 2.3MB\n"
            "\n"
            "        diff backup: 20181119-152138F_20181119-152152D\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: 000000010000000000000003 / 000000020000000000000003\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152155I\n"
            "            timestamp start/stop: 2018-11-19 15:21:55 / 2018-11-19 15:21:57\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000002/000000010000000000000005\n"
            "\n"
            "        full backup: 20201116-155000F\n"
            "            timestamp start/stop: 2020-11-16 15:50:00 / 2020-11-16 15:50:02\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "        incr backup: 20201116-155000F_20201119-152100I\n"
            "            timestamp start/stop: 2020-11-19 15:21:00 / 2020-11-19 15:21:03\n"
            "            wal start/stop: 000000010000000000000005 / 000000010000000000000005\n"
            "            database size: 19.2MB, database backup size: 8.2KB\n"
            "            repo1: backup set size: 2.3MB, backup size: 346B\n"
            "            backup reference list: 20201116-155000F\n",
            "text - multi-repo, prior backup: no archives but backups (code coverage)");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo, stanza requested does not exist, but other stanzas do");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza4");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza4\n"
            "    status: error (missing stanza path)\n",
            "multi-repo, stanza requested does not exist, but other stanzas do");

        // Add stanza3 to repo1 but with a current PG that is different than repo2
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo, current database different across repos");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza3");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6626363367545678888\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545678089,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":6626363367545678888,\"db-version\":\"9.5\"}\n",
            .comment = "put archive info to file - stanza3, repo1 stanza upgraded");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=2\n"
            "db-system-id=6626363367545678888\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[backup:current]\n"
            "20201212-192538F={\"backrest-format\":5,\"backrest-version\":\"2.25\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000003\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1607801138,\"backup-timestamp-stop\":1607801140,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":true,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":true,"
            "\"option-online\":true}\n"
            "20210112-192538F={\"backrest-format\":5,\"backrest-version\":\"2.25\","
            "\"backup-archive-start\":\"000000010000000000000006\",\"backup-archive-stop\":\"000000010000000000000006\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1610479538,\"backup-timestamp-stop\":1610479540,"
            "\"backup-type\":\"full\",\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":true,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":true,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.4\"}\n"
            "2={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6626363367545678888,"
                "\"db-version\":\"9.5\"}\n",
            .comment = "put backup info to file - stanza3, repo1 stanza upgraded");

        // Create stanza3 db1 WAL, repo1
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE
            "/9.4-1/0000000100000000/000000010000000000000002-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE
            "/9.4-1/0000000100000000/000000010000000000000003-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE
            "/9.5-2/0000000100000000/000000010000000000000006-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza3\n"
            "    status: error (database mismatch across repos)\n"
            "        repo1: ok\n"
            "        repo2: ok\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): 000000010000000000000001/000000010000000000000003\n"
            "\n"
            "        full backup: 20201110-100000F\n"
            "            timestamp start/stop: 2020-11-10 10:00:00 / 2020-11-10 10:00:02\n"
            "            wal start/stop: 000000010000000000000001 / 000000010000000000000002\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo2: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "        full backup: 20201212-192538F\n"
            "            timestamp start/stop: 2020-12-12 19:25:38 / 2020-12-12 19:25:40\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000006/000000010000000000000006\n"
            "\n"
            "        full backup: 20210112-192538F\n"
            "            timestamp start/stop: 2021-01-12 19:25:38 / 2021-01-12 19:25:40\n"
            "            wal start/stop: 000000010000000000000006 / 000000010000000000000006\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n",
            "text - multi-repo, database mismatch, repo2 stanza-upgrade needed");

        hrnCfgArgRawZ(argList2, cfgOptOutput, "json");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1,"
                                "\"repo-key\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":\"000000010000000000000003\","
                            "\"min\":\"000000010000000000000002\""
                        "},"
                        "{"
                            "\"database\":{"
                                "\"id\":2,"
                                "\"repo-key\":1"
                            "},"
                            "\"id\":\"9.5-2\","
                            "\"max\":\"000000010000000000000006\","
                            "\"min\":\"000000010000000000000006\""
                        "},"
                        "{"
                            "\"database\":{"
                                "\"id\":1,"
                                "\"repo-key\":2"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":\"000000010000000000000002\","
                            "\"min\":\"000000010000000000000001\""
                        "}"
                    "],"
                    "\"backup\":["
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000001\","
                                "\"stop\":\"000000010000000000000002\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.25\""
                            "},"
                            "\"database\":{"
                                "\"id\":1,"
                                "\"repo-key\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":26897020,"
                                "\"repository\":{"
                                    "\"delta\":3100,"
                                    "\"size\":3159000"
                                "},"
                                "\"size\":26897000"
                            "},"
                            "\"label\":\"20201110-100000F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1605002400,"
                                "\"stop\":1605002402"
                            "},"
                            "\"type\":\"full\""
                        "},"
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000002\","
                                "\"stop\":\"000000010000000000000003\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.25\""
                            "},"
                            "\"database\":{"
                                "\"id\":1,"
                                "\"repo-key\":1"
                            "},"
                            "\"info\":{"
                                "\"delta\":26897020,"
                                "\"repository\":{"
                                    "\"delta\":3100,"
                                    "\"size\":3159000"
                                "},"
                                "\"size\":26897000"
                            "},"
                            "\"label\":\"20201212-192538F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1607801138,"
                                "\"stop\":1607801140"
                            "},"
                            "\"type\":\"full\""
                        "},"
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000006\","
                                "\"stop\":\"000000010000000000000006\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.25\""
                            "},"
                            "\"database\":{"
                                "\"id\":2,"
                                "\"repo-key\":1"
                            "},"
                            "\"info\":{"
                                "\"delta\":26897020,"
                                "\"repository\":{"
                                    "\"delta\":3100,"
                                    "\"size\":3159000"
                                "},"
                                "\"size\":26897000"
                            "},"
                            "\"label\":\"20210112-192538F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1610479538,"
                                "\"stop\":1610479540"
                            "},"
                            "\"type\":\"full\""
                        "}"
                    "],"
                     "\"cipher\":\"mixed\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"repo-key\":1,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.4\""
                        "},"
                        "{"
                            "\"id\":2,"
                            "\"repo-key\":1,"
                            "\"system-id\":6626363367545678888,"
                            "\"version\":\"9.5\""
                        "},"
                        "{"
                            "\"id\":1,"
                            "\"repo-key\":2,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                    "\"name\":\"stanza3\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":0,"
                                "\"message\":\"ok\""
                            "}"
                        "},"
                        "{"
                            "\"cipher\":\"aes-256-cbc\","
                            "\"key\":2,"
                            "\"status\":{"
                                "\"code\":0,"
                                "\"message\":\"ok\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":5,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"database mismatch across repos\""
                    "}"
                "}"
            "]",
            "json - multi-repo, database mismatch, repo2 stanza-upgrade needed");

        // Crypto error
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("encryption error");

        // Change repo1 to have the same cipher type as repo2 even though on disk it does not
        HRN_STORAGE_PUT_Z(storageTest, TEST_PATH "/pgbackrest.conf", "[global]\nrepo-cipher-pass=123abc\n");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgKeyRawStrId(argList2, cfgOptRepoCipherType, 1, cipherTypeAes256Cbc);
        hrnCfgArgRawZ(argList2, cfgOptConfig, TEST_PATH "/pgbackrest.conf");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [CryptoError] unable to load info file '" TEST_PATH "/repo/backup/stanza1/backup.info' or '"
                            TEST_PATH "/repo/backup/stanza1/backup.info.copy':\n"
            "               CryptoError: cipher header invalid\n"
            "               HINT: is or was the repo encrypted?\n"
            "               FileMissingError: unable to open missing file '" TEST_PATH "/repo/backup/stanza1/backup.info.copy'"
                            " for read\n"
            "               HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "               HINT: has a stanza-create been performed?\n"
            "               HINT: use option --stanza if encryption settings are different for the stanza than the global"
            " settings.\n"
            "        repo2: error (no valid backups)\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000003/000000010000000000000004\n"
            "\n"
            "stanza: stanza2\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [CryptoError] unable to load info file '" TEST_PATH "/repo/backup/stanza2/backup.info' or '"
                            TEST_PATH "/repo/backup/stanza2/backup.info.copy':\n"
            "               CryptoError: cipher header invalid\n"
            "               HINT: is or was the repo encrypted?\n"
            "               FileMissingError: unable to open missing file '" TEST_PATH "/repo/backup/stanza2/backup.info.copy'"
                            " for read\n"
            "               HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "               HINT: has a stanza-create been performed?\n"
            "               HINT: use option --stanza if encryption settings are different for the stanza than the global"
            " settings.\n"
            "        repo2: error (missing stanza path)\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "stanza: stanza3\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [CryptoError] unable to load info file '" TEST_PATH "/repo/backup/stanza3/backup.info' or '"
                            TEST_PATH "/repo/backup/stanza3/backup.info.copy':\n"
            "               CryptoError: cipher header invalid\n"
            "               HINT: is or was the repo encrypted?\n"
            "               FileMissingError: unable to open missing file '" TEST_PATH "/repo/backup/stanza3/backup.info.copy'"
                            " for read\n"
            "               HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "               HINT: has a stanza-create been performed?\n"
            "               HINT: use option --stanza if encryption settings are different for the stanza than the global"
            " settings.\n"
            "        repo2: ok\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): 000000010000000000000001/000000010000000000000002\n"
            "\n"
            "        full backup: 20201110-100000F\n"
            "            timestamp start/stop: 2020-11-10 10:00:00 / 2020-11-10 10:00:02\n"
            "            wal start/stop: 000000010000000000000001 / 000000010000000000000002\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo2: backup set size: 3MB, backup size: 3KB\n",
            "text - multi-repo, multi-stanza cipher error");

        // Backup label not found, one repo in error
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("backup label exists on one repo, other repo in error");

        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza3");
        hrnCfgArgRawZ(argList2, cfgOptSet, "20201110-100000F");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza3\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [CryptoError] unable to load info file '" TEST_PATH "/repo/backup/stanza3/backup.info' or '"
                            TEST_PATH "/repo/backup/stanza3/backup.info.copy':\n"
            "               CryptoError: cipher header invalid\n"
            "               HINT: is or was the repo encrypted?\n"
            "               FileMissingError: unable to open missing file '" TEST_PATH "/repo/backup/stanza3/backup.info.copy'"
                            " for read\n"
            "               HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "               HINT: has a stanza-create been performed?\n"
            "               HINT: use option --stanza if encryption settings are different for the stanza than the global"
            " settings.\n"
            "        repo2: error (requested backup not found)\n"
            "    cipher: aes-256-cbc\n",
            "backup label not found, one repo in error");

        // Crypto error
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL read error");

        argList2 = strLstDup(argListMultiRepo);
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        HRN_STORAGE_PATH_CREATE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/9.4-1", .mode = 0200, .comment = "WAL directory with bad permissions");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [PathOpenError] unable to list file info for path '" TEST_PATH "/repo/archive/stanza1/9.4-1': [13]"
            " Permission denied\n"
            "        repo2: error (no valid backups)\n"
            "    cipher: mixed\n"
            "        repo1: none\n"
            "        repo2: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.5): 000000010000000000000003/000000010000000000000004\n",
            "WAL directory read error");

        // Unset environment key
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);
    }

    //******************************************************************************************************************************
    if (testBegin("database mismatch - special cases"))
    {
        // These tests cover branches not covered in other tests
        TEST_TITLE("multi-repo, database mismatch, pg system-id only");

        StringList *argList2 = strLstNew();
        hrnCfgArgRawZ(argList2, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList2, cfgOptStanza, "stanza1");
        hrnCfgArgKeyRawZ(argList2, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        HRN_CFG_LOAD(cfgCmdInfo, argList2);

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20201116-155000F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000003\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605541800,\"backup-timestamp-stop\":1605541802,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n",
            .comment = "put backup info to file, repo1");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n",
            .comment = "put archive info to file, repo1");

        // Create stanza1, repo1, archives
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE
            "/9.4-1/0000000100000000/000000010000000000000002-22dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE
            "/9.4-1/0000000100000000/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[backup:current]\n"
            "20201116-155010F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605541810,\"backup-timestamp-stop\":1605541812,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n",
            .comment = "put backup info to file, repo2, same system-id, different version");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n",
            .comment = "put archive info to file,  repo2, same system-id, different version");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE
            "/9.5-1/0000000100000000/000000010000000000000001-11dff2b7552a9d66e4bae1a762488a6885e7082c.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE
            "/9.5-1/0000000100000000/000000010000000000000002-222ff2b7552a9d66e4bae1a762488a6885e7082c.gz");

        // Note that although the time on the backup in repo2 > repo1, repo1 current db is not the same because of the version so
        // the repo1, since read first, will be considered the current PG
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (database mismatch across repos)\n"
            "        repo1: ok\n"
            "        repo2: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.5): 000000010000000000000001/000000010000000000000002\n"
            "\n"
            "        full backup: 20201116-155010F\n"
            "            timestamp start/stop: 2020-11-16 15:50:10 / 2020-11-16 15:50:12\n"
            "            wal start/stop: 000000010000000000000001 / 000000010000000000000002\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo2: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000010000000000000003\n"
            "\n"
            "        full backup: 20201116-155000F\n"
            "            timestamp start/stop: 2020-11-16 15:50:00 / 2020-11-16 15:50:02\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n",
            "text - db mismatch, diff system-id across repos, repo1 considered current db since read first");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multi-repo, database mismatch, pg version only");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665888\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:current]\n"
            "20201116-155010F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.30\","
            "\"backup-archive-start\":\"000000010000000000000001\",\"backup-archive-stop\":\"000000010000000000000002\","
            "\"backup-info-repo-size\":3159000,\"backup-info-repo-size-delta\":3100,\"backup-info-size\":26897000,"
            "\"backup-info-size-delta\":26897020,\"backup-timestamp-start\":1605541810,\"backup-timestamp-stop\":1605541812,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":true,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":false,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665888,"
                "\"db-version\":\"9.4\"}\n",
            .comment = "put backup info to file, repo2, different system-id, same version");

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665888\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665888,\"db-version\":\"9.4\"}\n",
            .comment = "put archive info to file,  repo2, different system-id, same version");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (database mismatch across repos)\n"
            "        repo1: ok\n"
            "        repo2: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4): none present\n"
            "\n"
            "        full backup: 20201116-155010F\n"
            "            timestamp start/stop: 2020-11-16 15:50:10 / 2020-11-16 15:50:12\n"
            "            wal start/stop: 000000010000000000000001 / 000000010000000000000002\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo2: backup set size: 3MB, backup size: 3KB\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4): 000000010000000000000002/000000010000000000000003\n"
            "\n"
            "        full backup: 20201116-155000F\n"
            "            timestamp start/stop: 2020-11-16 15:50:00 / 2020-11-16 15:50:02\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000003\n"
            "            database size: 25.7MB, database backup size: 25.7MB\n"
            "            repo1: backup set size: 3MB, backup size: 3KB\n",
            "text - db mismatch, diff version across repos, repo1 considered current db since read first");
    }

    //******************************************************************************************************************************
    if (testBegin("cmdInfo()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_ARCHIVE, .comment = "create repo archive path");
        HRN_STORAGE_PATH_CREATE(storageRepoWrite(), STORAGE_REPO_BACKUP, .comment = "create repo backup path");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no stanza exist");

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        const String *stdoutFile = STRDEF(TEST_PATH "/stdout.info");

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdInfo();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        // Check output of info command stored in file
        TEST_STORAGE_GET(storageTest, strZ(stdoutFile), "No stanzas exist in the repository.\n", .remove = true);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("set option invalid without stanza option");

        hrnCfgArgRawZ(argList, cfgOptSet, "bogus");

        TEST_ERROR(hrnCfgLoadP(cfgCmdInfo, argList), OptionInvalidError, "option 'set' not valid without option 'stanza'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo-level error");

        HRN_STORAGE_PATH_CREATE(storageTest, TEST_PATH "/repo2", .mode = 0200, .comment = "repo directory with bad permissions");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo2");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: [invalid]\n"
            "    status: error (other)\n"
            "            [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/backup': [13] Permission denied\n"
            "    cipher: none\n",
            "text - invalid stanza");

        hrnCfgArgRawZ(argList, cfgOptOutput, "json");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":[],"
                    "\"backup\":[],"
                    "\"cipher\":\"none\","
                    "\"db\":[],"
                    "\"name\":\"[invalid]\","
                    "\"repo\":["
                        "{"
                            "\"cipher\":\"none\","
                            "\"key\":1,"
                            "\"status\":{"
                                "\"code\":99,"
                                "\"message\":\"[PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/backup':"
                                    " [13] Permission denied\""
                            "}"
                        "}"
                    "],"
                    "\"status\":{"
                        "\"code\":99,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"other\""
                        "}"
                "}"
            "]",
            "json - invalid stanza");

        argList = strLstNew();
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptStanza, "stanza1");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (other)\n"
            "            [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/backup': [13] Permission denied\n"
            "    cipher: none\n",
            "text - stanza requested");

        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: mixed\n"
            "        repo1: error (other)\n"
            "               [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/backup':"
                " [13] Permission denied\n"
            "        repo2: error (missing stanza path)\n"
            "    cipher: none\n",
            "text - stanza repo structure exists");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
