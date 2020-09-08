/***********************************************************************************************************************************
Test Info Command
***********************************************************************************************************************************/
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

    // The tests expect the timezone to be UTC
    setenv("TZ", "UTC", true);

    // Create the repo directories
    String *repoPath = strNewFmt("%s/repo", testPath());
    String *archivePath = strNewFmt("%s/%s", strZ(repoPath), "archive");
    String *backupPath = strNewFmt("%s/%s", strZ(repoPath), "backup");
    String *archiveStanza1Path = strNewFmt("%s/stanza1", strZ(archivePath));
    String *backupStanza1Path = strNewFmt("%s/stanza1", strZ(backupPath));

    // *****************************************************************************************************************************
    if (testBegin("infoRender()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/", strZ(repoPath)));
        StringList *argListText = strLstDup(argList);

        strLstAddZ(argList, "--output=json");
        harnessCfgLoad(cfgCmdInfo, argList);

        // No stanzas have been created
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(infoRender(), "[]", "json - repo but no stanzas");

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(infoRender(), "No stanzas exist in the repository.\n", "text - no stanzas");

        storagePathCreateP(storageLocalWrite(), archivePath);
        storagePathCreateP(storageLocalWrite(), backupPath);

        // Empty stanza
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), backupStanza1Path), "backup stanza1 directory");
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveStanza1Path), "archive stanza1 directory");
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing stanza data)\n"
            "    cipher: none\n",
            "text - missing stanza data");

        harnessCfgLoad(cfgCmdInfo, argList);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":[],"
                    "\"backup\":[],"
                    "\"cipher\":\"none\","
                    "\"db\":[],"
                    "\"name\":\"stanza1\","
                    "\"status\":{"
                        "\"code\":3,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"missing stanza data\""
                        "}"
                "}"
            "]",
            "json - missing stanza data");

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/backup.info", strZ(backupStanza1Path))),
                harnessInfoChecksum(content)),
            "put backup info to file");

        TEST_ERROR_FMT(infoRender(), FileMissingError,
            "unable to load info file '%s/archive.info' or '%s/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
            strZ(archiveStanza1Path), strZ(archiveStanza1Path),
            strZ(strNewFmt("%s/archive.info", strZ(archiveStanza1Path))),
            strZ(strNewFmt("%s/archive.info.copy", strZ(archiveStanza1Path))));

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/archive.info", strZ(archiveStanza1Path))),
                harnessInfoChecksum(content)),
            "put archive info to file");

        // archive section will cross reference backup db-id 2 to archive db-id 3 but db section will only use the db-ids from
        // backup.info
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"9.4-3\","
                            "\"max\":null,"
                            "\"min\":null"
                        "}"
                    "],"
                     "\"backup\":[],"
                     "\"cipher\":\"aes-256-cbc\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6569239123849665666,"
                            "\"version\":\"9.3\""
                        "},"
                        "{"
                            "\"id\":2,"
                            "\"system-id\":6569239123849665679,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                     "\"name\":\"stanza1\","
                     "\"status\":{"
                        "\"code\":2,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"no valid backups\""
                    "}"
                "}"
            "]",
            "json - single stanza, no valid backups");

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (no valid backups)\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-3): none present\n",
            "text - single stanza, no valid backups");

        // Repeat prior tests while a backup lock is held
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), strNew("stanza1"), lockTypeBackup, 0, true), -1,
                    "create backup/expire lock");

                sleepMSec(1000);
                lockRelease(true);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                sleepMSec(250);

                harnessCfgLoad(cfgCmdInfo, argList);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "["
                        "{"
                            "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":2"
                                    "},"
                                    "\"id\":\"9.4-3\","
                                    "\"max\":null,"
                                    "\"min\":null"
                                "}"
                            "],"
                             "\"backup\":[],"
                             "\"cipher\":\"aes-256-cbc\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"system-id\":6569239123849665666,"
                                    "\"version\":\"9.3\""
                                "},"
                                "{"
                                    "\"id\":2,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                             "\"name\":\"stanza1\","
                             "\"status\":{"
                                "\"code\":2,"
                                "\"lock\":{\"backup\":{\"held\":true}},"
                                "\"message\":\"no valid backups\""
                            "}"
                        "}"
                    "]",
                    "json - single stanza, no valid backups, backup/expire lock detected");

                harnessCfgLoad(cfgCmdInfo, argListText);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "stanza: stanza1\n"
                    "    status: error (no valid backups, backup/expire running)\n"
                    "    cipher: aes-256-cbc\n"
                    "\n"
                    "    db (current)\n"
                    "        wal archive min/max (9.4-3): none present\n",
                    "text - single stanza, no valid backups, backup/expire lock detected");

            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // Add WAL segments
        //--------------------------------------------------------------------------------------------------------------------------
        String *archiveDb3 = strNewFmt("%s/9.4-3/0000000100000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb3), "create db3 archive WAL1 directory");

        String *archiveDb3Wal = strNewFmt(
            "%s/000000010000000000000004-47dff2b7552a9d66e4bae1a762488a6885e7082c.gz", strZ(archiveDb3));
        TEST_RESULT_VOID(storagePutP(storageNewWriteP(storageLocalWrite(), archiveDb3Wal), bufNew(0)), "touch WAL3 file");

        StringList *argList2 = strLstDup(argListText);
        strLstAddZ(argList2, "--stanza=stanza1");
        harnessCfgLoad(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
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
        String *archiveDb1_1 = strNewFmt("%s/9.4-1/0000000100000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_1), "create db1 archive WAL1 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000002-ac61b8f1ec7b1e6c3eaee9345214595eb7daa9a1.gz",
            strZ(archiveDb1_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strZ(archiveDb1_1)))))), 0, "touch WAL1 file");

        String *archiveDb1_2 = strNewFmt("%s/9.4-1/0000000200000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_2), "create db1 archive WAL2 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000020000000000000003-37dff2b7552a9d66e4bae1a762488a6885e7082c.gz",
            strZ(archiveDb1_2)))))), 0, "touch WAL2 file");

        String *archiveDb1_3 = strNewFmt("%s/9.4-1/0000000300000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_3), "create db1 archive WAL3 directory");

        harnessCfgLoad(cfgCmdInfo, argList);
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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/backup.info", strZ(backupStanza1Path))),
                harnessInfoChecksum(content)),
            "put backup info to file");

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":\"000000020000000000000003\","
                            "\"min\":\"000000010000000000000002\""
                        "},"
                        "{"
                            "\"database\":{"
                                "\"id\":3"
                            "},"
                            "\"id\":\"9.4-3\","
                            "\"max\":null,"
                            "\"min\":null"
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
                                "\"id\":1"
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
                        "}"
                    "],"
                     "\"cipher\":\"none\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6569239123849665679,"
                            "\"version\":\"9.4\""
                        "},"
                        "{"
                            "\"id\":2,"
                            "\"system-id\":6569239123849665666,"
                            "\"version\":\"9.3\""
                        "},"
                        "{"
                            "\"id\":3,"
                            "\"system-id\":6569239123849665679,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                     "\"name\":\"stanza1\","
                     "\"status\":{"
                        "\"code\":0,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"ok\""
                    "}"
                "}"
            "]",
            "json - single stanza, valid backup, no priors, no archives in latest DB");

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
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
            "        wal archive min/max (9.4-3): none present\n",
            "text - single stanza, valid backup, no priors, no archives in latest DB");

        // Repeat prior tests while a backup lock is held
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT_NE(
                    lockAcquire(cfgOptionStr(cfgOptLockPath), strNew("stanza1"), lockTypeBackup, 0, true), -1,
                    "create backup/expire lock");

                sleepMSec(1000);
                lockRelease(true);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                sleepMSec(250);

                harnessCfgLoad(cfgCmdInfo, argList);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "["
                        "{"
                             "\"archive\":["
                                "{"
                                    "\"database\":{"
                                        "\"id\":1"
                                    "},"
                                    "\"id\":\"9.4-1\","
                                    "\"max\":\"000000020000000000000003\","
                                    "\"min\":\"000000010000000000000002\""
                                "},"
                                "{"
                                    "\"database\":{"
                                        "\"id\":3"
                                    "},"
                                    "\"id\":\"9.4-3\","
                                    "\"max\":null,"
                                    "\"min\":null"
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
                                        "\"id\":1"
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
                                "}"
                            "],"
                             "\"cipher\":\"none\","
                             "\"db\":["
                                "{"
                                    "\"id\":1,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "},"
                                "{"
                                    "\"id\":2,"
                                    "\"system-id\":6569239123849665666,"
                                    "\"version\":\"9.3\""
                                "},"
                                "{"
                                    "\"id\":3,"
                                    "\"system-id\":6569239123849665679,"
                                    "\"version\":\"9.4\""
                                "}"
                            "],"
                             "\"name\":\"stanza1\","
                             "\"status\":{"
                                "\"code\":0,"
                                "\"lock\":{\"backup\":{\"held\":true}},"
                                "\"message\":\"ok\""
                            "}"
                        "}"
                    "]",
                    "json - single stanza, valid backup, no priors, no archives in latest DB, backup/expire lock detected");

                harnessCfgLoad(cfgCmdInfo, argListText);
                TEST_RESULT_STR_Z(
                    infoRender(),
                    "stanza: stanza1\n"
                    "    status: ok (backup/expire running)\n"
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
                    "        wal archive min/max (9.4-3): none present\n",
                    "text - single stanza, valid backup, no priors, no archives in latest DB, backup/expire lock detected");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/archive.info", strZ(archiveStanza1Path))),
                harnessInfoChecksum(content)),
            "put archive info to file - stanza1");

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/backup.info", strZ(backupStanza1Path))),
                harnessInfoChecksum(content)),
            "put backup info to file - stanza1");

        // Manifest with all features
        // -------------------------------------------------------------------------------------------------------------------------
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

        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageLocalWrite(),
            strNewFmt("%s/20181119-152138F_20181119-152152I/" BACKUP_MANIFEST_FILE, strZ(backupStanza1Path))), contentLoad),
            "write manifest - stanza1");

        String *archiveStanza2Path = strNewFmt("%s/stanza2", strZ(archivePath));
        String *backupStanza2Path = strNewFmt("%s/stanza2", strZ(backupPath));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), backupStanza1Path), "backup stanza2 directory");
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveStanza1Path), "archive stanza2 directory");

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/archive.info", strZ(archiveStanza2Path))),
                harnessInfoChecksum(content)),
            "put archive info to file - stanza2");

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
            storagePutP(
                storageNewWriteP(storageLocalWrite(), strNewFmt("%s/backup.info", strZ(backupStanza2Path))),
                harnessInfoChecksum(content)),
            "put backup info to file - stanza2");

        harnessCfgLoad(cfgCmdInfo, argList);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":\"000000020000000000000003\","
                            "\"min\":\"000000010000000000000002\""
                        "},"
                        "{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"9.5-2\","
                            "\"max\":null,"
                            "\"min\":null"
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
                                "\"id\":1"
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
                                "\"stop\":1542640911"
                            "},"
                            "\"type\":\"full\""
                        "},"
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000003\","
                                "\"stop\":\"000000010000000000000003\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.08dev\""
                            "},"
                            "\"database\":{"
                                "\"id\":1"
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
                                "\"id\":1"
                            "},"
                            "\"info\":{"
                                "\"delta\":8428,"
                                "\"repository\":{"
                                    "\"delta\":346,"
                                    "\"size\":2369186"
                                "},"
                                "\"size\":20162900"
                            "},"
                            "\"label\":\"20181119-152138F_20181119-152152I\","
                            "\"prior\":\"20181119-152138F_20181119-152152D\","
                            "\"reference\":["
                                "\"20181119-152138F\","
                                "\"20181119-152138F_20181119-152152D\""
                            "],"
                            "\"timestamp\":{"
                                "\"start\":1542640912,"
                                "\"stop\":1542640915"
                            "},"
                            "\"type\":\"incr\""
                        "}"
                    "],"
                     "\"cipher\":\"none\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6625592122879095702,"
                            "\"version\":\"9.4\""
                        "},"
                        "{"
                            "\"id\":2,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.5\""
                        "}"
                    "],"
                     "\"name\":\"stanza1\","
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
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":null,"
                            "\"min\":null"
                        "}"
                    "],"
                     "\"backup\":[],"
                     "\"cipher\":\"none\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6625633699176220261,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                     "\"name\":\"stanza2\","
                     "\"status\":{"
                        "\"code\":2,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"no valid backups\""
                    "}"
                "}"
            "]",
            "json - multiple stanzas, one with valid backups, archives in latest DB");

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
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
            "        wal archive min/max (9.4-1): none present\n",
            "text - multiple stanzas, one with valid backups, archives in latest DB");

        // Backup set requested
        //--------------------------------------------------------------------------------------------------------------------------
        argList2 = strLstDup(argListText);
        strLstAddZ(argList2, "--stanza=stanza1");
        strLstAddZ(argList2, "--set=20181119-152138F_20181119-152152I");
        harnessCfgLoad(cfgCmdInfo, argList2);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152152I\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, backup size: 8.2KB\n"
            "            repository size: 2.3MB, repository backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: mail (16456), postgres (12173)\n"
            "            symlinks:\n"
            "                pg_hba.conf => ../pg_config/pg_hba.conf\n"
            "                pg_stat => ../pg_stat\n"
            "            tablespaces:\n"
            "                ts1 (1) => /tblspc/ts1\n"
            "                ts12 (12) => /tblspc/ts12\n",
            "text - backup set requested");

        strLstAddZ(argList2, "--output=json");
        harnessCfgLoad(cfgCmdInfo, argList2);

        TEST_ERROR(strZ(infoRender()), ConfigError, "option 'set' is currently only valid for text output");

        // Backup set requested but no links
        //--------------------------------------------------------------------------------------------------------------------------
        argList2 = strLstDup(argListText);
        strLstAddZ(argList2, "--stanza=stanza1");
        strLstAddZ(argList2, "--set=20181119-152138F_20181119-152152I");
        harnessCfgLoad(cfgCmdInfo, argList2);

        #define TEST_MANIFEST_TARGET_NO_LINK                                                                                       \
            "\n"                                                                                                                   \
            "[backup:target]\n"                                                                                                    \
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"                                                                  \

        contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET_NO_LINK
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNew(STORAGE_REPO_BACKUP "/20181119-152138F_20181119-152152I/" BACKUP_MANIFEST_FILE)),
                    contentLoad),
                "write manifest");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152152I\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, backup size: 8.2KB\n"
            "            repository size: 2.3MB, repository backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: mail (16456), postgres (12173)\n",
            "text - backup set requested, no links");

        // Backup set requested but no databases
        //--------------------------------------------------------------------------------------------------------------------------
        argList2 = strLstDup(argListText);
        strLstAddZ(argList2, "--stanza=stanza1");
        strLstAddZ(argList2, "--set=20181119-152138F_20181119-152152I");
        harnessCfgLoad(cfgCmdInfo, argList2);

        #define TEST_MANIFEST_NO_DB                                                                                                \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET_NO_LINK
            TEST_MANIFEST_NO_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageRepoWrite(), strNew(STORAGE_REPO_BACKUP "/20181119-152138F_20181119-152152I/" BACKUP_MANIFEST_FILE)),
                    contentLoad),
            "write manifest");

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: none\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.4-1): 000000010000000000000002/000000020000000000000003\n"
            "\n"
            "        incr backup: 20181119-152138F_20181119-152152I\n"
            "            timestamp start/stop: 2018-11-19 15:21:52 / 2018-11-19 15:21:55\n"
            "            wal start/stop: n/a\n"
            "            database size: 19.2MB, backup size: 8.2KB\n"
            "            repository size: 2.3MB, repository backup size: 346B\n"
            "            backup reference list: 20181119-152138F, 20181119-152138F_20181119-152152D\n"
            "            database list: none\n",
            "text - backup set requested, no db");

        // Stanza not found
        //--------------------------------------------------------------------------------------------------------------------------
        argList2 = strLstDup(argList);
        strLstAddZ(argList2, "--stanza=silly");
        harnessCfgLoad(cfgCmdInfo, argList2);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"backup\":[],"
                     "\"db\":[],"
                     "\"name\":\"silly\","
                     "\"status\":{"
                        "\"code\":1,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"missing stanza path\""
                    "}"
                "}"
            "]",
            "json - missing stanza path");

        StringList *argListText2 = strLstDup(argListText);
        strLstAddZ(argListText2, "--stanza=silly");
        harnessCfgLoad(cfgCmdInfo, argListText2);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: silly\n"
            "    status: error (missing stanza path)\n",
            "text - missing stanza path");

        // Stanza found
        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--stanza=stanza2");
        harnessCfgLoad(cfgCmdInfo, argList);
        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                     "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.4-1\","
                            "\"max\":null,"
                            "\"min\":null"
                        "}"
                    "],"
                     "\"backup\":[],"
                     "\"cipher\":\"none\","
                     "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6625633699176220261,"
                            "\"version\":\"9.4\""
                        "}"
                    "],"
                     "\"name\":\"stanza2\","
                     "\"status\":{"
                        "\"code\":2,"
                        "\"lock\":{\"backup\":{\"held\":false}},"
                        "\"message\":\"no valid backups\""
                    "}"
                "}"
            "]",
            "json - multiple stanzas - selected found");

        strLstAddZ(argListText, "--stanza=stanza2");
        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza2\n"
            "    status: error (no valid backups)\n"
            "    cipher: none\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (9.4-1): none present\n",
            "text - multiple stanzas - selected found");

        // Crypto error
        //--------------------------------------------------------------------------------------------------------------------------
        content = strNew
        (
            "[global]\n"
            "repo-cipher-pass=123abc\n"
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageLocalWrite(), strNewFmt("%s/pgbackrest.conf", testPath())),
                BUFSTR(content)), "put pgbackrest.conf file");
        strLstAddZ(argListText, "--repo-cipher-type=aes-256-cbc");
        strLstAdd(argListText, strNewFmt("--config=%s/pgbackrest.conf", testPath()));
        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_ERROR_FMT(
            infoRender(), CryptoError,
            "unable to load info file '%s/backup.info' or '%s/backup.info.copy':\n"
            "CryptoError: cipher header invalid\n"
            "HINT: is or was the repo encrypted?\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: backup.info cannot be opened and is required to perform a backup.\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use option --stanza if encryption settings are different for the stanza than the global settings.",
            strZ(backupStanza2Path), strZ(backupStanza2Path),  strZ(strNewFmt("%s/backup.info.copy", strZ(backupStanza2Path))));
    }

    // *****************************************************************************************************************************
    if (testBegin("walArchivesGapDetection()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s/", strZ(repoPath)));
        strLstAddZ(argList, "--archive-gap-detection");
        strLstAddZ(argList, "--repo1-retention-full=1");  // avoid warning
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345", true);
        strLstAddZ(argList, "--repo-cipher-type=aes-256-cbc");
        StringList *argListText = strLstDup(argList);
        strLstAddZ(argList, "--output=json");

        // Create stanza1 paths
        String *archiveStanza1Path = strNewFmt("%s/stanza1", strZ(archivePath));
        String *backupStanza1Path = strNewFmt("%s/stanza1", strZ(backupPath));
        storagePathCreateP(storageLocalWrite(), archiveStanza1Path);
        storagePathCreateP(storageLocalWrite(), backupStanza1Path);

        // Put backup.info, archive.info and 00000003.history files
        String *backupInfoContent = strNew(
            "[cipher]\n"
            "cipher-pass=\"sub12345\"\n"
            "\n"
            "[backup:current]\n"
            "20200817-111046F={"
            "\"backrest-format\":5,\"backrest-version\":\"2.28\","
            "\"backup-archive-start\":\"000000010000000000000002\",\"backup-archive-stop\":\"000000010000000000000004\","
            "\"backup-info-repo-size\":2931163,\"backup-info-repo-size-delta\":2931163,"
            "\"backup-info-size\":24534588,\"backup-info-size-delta\":24534588,"
            "\"backup-timestamp-start\":1597655446,\"backup-timestamp-stop\":1597655450,\"backup-type\":\"full\","
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20200817-111046F_20200817-111051D={"
            "\"backrest-format\":5,\"backrest-version\":\"2.28\",\"backup-archive-start\":\"000000010000000000000006\","
            "\"backup-archive-stop\":\"000000010000000000000006\",\"backup-info-repo-size\":2931218,"
            "\"backup-info-repo-size-delta\":938,\"backup-info-size\":24534808,\"backup-info-size-delta\":9894,"
            "\"backup-prior\":\"20200817-111046F\",\"backup-reference\":[\"20200817-111046F\"],"
            "\"backup-timestamp-start\":1597655451,\"backup-timestamp-stop\":1597655453,\"backup-type\":\"diff\","
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20200817-111046F_20200817-111054I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.28\",\"backup-archive-start\":\"000000010000000000000008\","
            "\"backup-archive-stop\":\"000000010000000000000008\",\"backup-info-repo-size\":2931237,"
            "\"backup-info-repo-size-delta\":957,\"backup-info-size\":24535027,\"backup-info-size-delta\":10113,"
            "\"backup-prior\":\"20200817-111046F_20200817-111051D\","
            "\"backup-reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
            "\"backup-timestamp-start\":1597655454,\"backup-timestamp-stop\":1597655456,\"backup-type\":\"incr\","
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20200817-111046F_20200817-111103I={"
            "\"backrest-format\":5,\"backrest-version\":\"2.28\",\"backup-archive-start\":\"00000002000000000000000C\","
            "\"backup-archive-stop\":\"00000002000000000000000C\",\"backup-info-repo-size\":2931615,"
            "\"backup-info-repo-size-delta\":1518,\"backup-info-size\":24536651,\"backup-info-size-delta\":11956,"
            "\"backup-prior\":\"20200817-111046F_20200817-111054I\","
            "\"backup-reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
            "\"backup-timestamp-start\":1597655463,\"backup-timestamp-stop\":1597655465,\"backup-type\":\"incr\","
            "\"db-id\":2,\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":true,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201909212\n"
            "db-control-version=1201\n"
            "db-id=2\n"
            "db-system-id=6861877834002393398\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6626363367545678089,"
                "\"db-version\":\"9.2\"}\n"
            "2={\"db-catalog-version\":201909212,\"db-control-version\":1201,\"db-system-id\":6861877834002393398,"
                "\"db-version\":\"12\"}"
        );

        StorageWrite *backupInfoWrite = storageNewWriteP(
            storageLocalWrite(), strNewFmt("%s/backup.info", strZ(backupStanza1Path)));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(backupInfoWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("12345"), NULL));

        TEST_RESULT_VOID(storagePutP(backupInfoWrite, harnessInfoChecksum(backupInfoContent)), "put backup info to file");

        String *archiveInfoContent = strNew(
            "[cipher]\n"
            "cipher-pass=\"sub12345\"\n"
            "\n"
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6861877834002393398\n"
            "db-version=\"12\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6626363367545678089,\"db-version\":\"9.2\"}\n"
            "2={\"db-id\":6861877834002393398,\"db-version\":\"12\"}\n"
        );

        StorageWrite *archiveInfoWrite = storageNewWriteP(
            storageLocalWrite(), strNewFmt("%s/archive.info", strZ(archiveStanza1Path)));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(archiveInfoWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("12345"), NULL));

        TEST_RESULT_VOID(storagePutP(archiveInfoWrite, harnessInfoChecksum(archiveInfoContent)), "put archive info to file");

        const Buffer *historyContentBuffer1 = BUFSTRDEF(
            "2	000000010000000000000003	no recovery target specified\n"
            "\n"
            "\n"
            "3	000000020000000000000005	no recovery target specified\n"
        );

        StorageWrite *historyWrite1 = storageNewWriteP(
            storageLocalWrite(), strNewFmt("%s/9.2-1/00000003.history", strZ(archiveStanza1Path)));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(historyWrite1)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("sub12345"), NULL));

        TEST_RESULT_VOID(storagePutP(historyWrite1, historyContentBuffer1), "put db1 00000003.history to file");

        // Simulate wrong line in history file to test the history RegExp
        const Buffer *historyContentBuffer = BUFSTRDEF(
            "1	0/B000000	before 2000-01-01 01:00:00+01\n"
            "\n"
            "\n"
            "2	0/E000000	no recovery target specified\n"
            "\n"
            "\n"
            "2	0123/012345789ABCDEF	no recovery target specified\n"
        );

        StorageWrite *historyWrite = storageNewWriteP(
            storageLocalWrite(), strNewFmt("%s/12-2/00000003.history", strZ(archiveStanza1Path)));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(historyWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("sub12345"), NULL));

        TEST_RESULT_VOID(storagePutP(historyWrite, historyContentBuffer), "put db2 00000003.history to file");

        // Add WAL segments
        String *archiveDb1_1 = strNewFmt("%s/9.2-1/0000000100000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_1), "create db1 archive WAL1 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000001-33099f27f375f42f3aab8c9d644ed24dba7071d7.gz",
            strZ(archiveDb1_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000002-d72eddc39b75a32e791e2c55ec54b0b4a818b44c.gz",
            strZ(archiveDb1_1)))))), 0, "touch WAL1 file");

        String *archiveDb1_2 = strNewFmt("%s/9.2-1/0000000200000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_2), "create db1 archive WAL2 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000020000000000000003-07bf320a2c0a2705cbe045bd48a9b0f1b44a40c4.gz",
            strZ(archiveDb1_2)))))), 0, "touch WAL2 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000020000000000000004-5d30de2b3969472587948fb88ad19ca57c3b660f.gz",
            strZ(archiveDb1_2)))))), 0, "touch WAL2 file");

        String *archiveDb1_3 = strNewFmt("%s/9.2-1/0000000300000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb1_3), "create db1 archive WAL3 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000030000000000000005-c623389b95105f12f9559cc369c5d0fad5ea8c38.gz",
            strZ(archiveDb1_3)))))), 0, "touch WAL3 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000030000000000000006-db21643e5863d872db73fb38f0e616023baaf090.gz",
            strZ(archiveDb1_3)))))), 0, "touch WAL3 file");

        String *archiveDb2_1 = strNewFmt("%s/12-2/0000000100000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb2_1), "create db2 archive WAL1 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000002-d72eddc39b75a32e791e2c55ec54b0b4a818b44c.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000003-07bf320a2c0a2705cbe045bd48a9b0f1b44a40c4.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000004-5d30de2b3969472587948fb88ad19ca57c3b660f.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000005-c623389b95105f12f9559cc369c5d0fad5ea8c38.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000006-db21643e5863d872db73fb38f0e616023baaf090.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000007-0ec3fc389a62d3b1dc28ef1fea02a49ab70fd6b1.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000008-60259c0a71f841ffcaa68aa7a071c7cc6404ab3c.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000009-8c7e9da8a9bd92b2a30800e2406a775b2a9d6f10.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000001000000000000000A-503151655f5391c418d9a9b0c56ed52b47a8706e.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");

        String *archiveDb2_2 = strNewFmt("%s/12-2/0000000200000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb2_2), "create db2 archive WAL2 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000002000000000000000B-1b9ca307baced1b848a0cdd4a59b921153165800.gz",
            strZ(archiveDb2_2)))))), 0, "touch WAL2 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000002000000000000000C-7f38e1f587295bd65ddff35032e80ff785183b1b.gz",
            strZ(archiveDb2_2)))))), 0, "touch WAL2 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000002000000000000000D-953543645a27e72582730cb36677469be3e92860.gz",
            strZ(archiveDb2_2)))))), 0, "touch WAL2 file");

        String *archiveDb2_3 = strNewFmt("%s/12-2/0000000300000000", strZ(archiveStanza1Path));
        TEST_RESULT_VOID(storagePathCreateP(storageLocalWrite(), archiveDb2_3), "create db2 archive WAL3 directory");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000003000000000000000E-bd8a4171ecab59a6dc559b88b98f92a6737ce480.gz",
            strZ(archiveDb2_3)))))), 0, "touch WAL3 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000003000000000000000F-877867b41e3c31c34513f846c3921ff039e1bd6c.gz",
            strZ(archiveDb2_3)))))), 0, "touch WAL3 file");
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000030000000000000010-974c8e2de6594c7ba9422af515993334648f0862.gz",
            strZ(archiveDb2_3)))))), 0, "touch WAL3 file");

        // -------------------------------------------------------------------------------------------------------------------------
        // Everything is fine
        harnessCfgLoad(cfgCmdInfo, argList);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.2-1\","
                            "\"max\":\"000000030000000000000006\","
                            "\"min\":\"000000010000000000000001\","
                            "\"missing-list\":\"\","
                            "\"missing-nb\":0"
                        "},{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"12-2\","
                            "\"max\":\"000000030000000000000010\","
                            "\"min\":\"000000010000000000000002\","
                            "\"missing-list\":\"\","
                            "\"missing-nb\":0"
                        "}"
                    "],"
                    "\"backup\":["
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000002\","
                                "\"stop\":\"000000010000000000000004\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":24534588,"
                                "\"repository\":{"
                                    "\"delta\":2931163,"
                                    "\"size\":2931163"
                                "},"
                                "\"size\":24534588"
                            "},"
                            "\"label\":\"20200817-111046F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1597655446,"
                                "\"stop\":1597655450"
                            "},"
                            "\"type\":\"full\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000006\","
                                "\"stop\":\"000000010000000000000006\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":9894,"
                                "\"repository\":{"
                                    "\"delta\":938,"
                                    "\"size\":2931218"
                                "},"
                                "\"size\":24534808"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111051D\","
                            "\"prior\":\"20200817-111046F\","
                            "\"reference\":[\"20200817-111046F\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655451,"
                                "\"stop\":1597655453"
                            "},"
                            "\"type\":\"diff\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000008\","
                                "\"stop\":\"000000010000000000000008\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":10113,"
                                "\"repository\":{"
                                    "\"delta\":957,"
                                    "\"size\":2931237"
                                "},"
                                "\"size\":24535027"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111054I\","
                            "\"prior\":\"20200817-111046F_20200817-111051D\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655454,"
                                "\"stop\":1597655456"
                            "},"
                            "\"type\":\"incr\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"00000002000000000000000C\","
                                "\"stop\":\"00000002000000000000000C\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":11956,"
                                "\"repository\":{"
                                    "\"delta\":1518,"
                                    "\"size\":2931615"
                                "},"
                                "\"size\":24536651"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111103I\","
                            "\"prior\":\"20200817-111046F_20200817-111054I\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655463,"
                                "\"stop\":1597655465"
                            "},"
                            "\"type\":\"incr\""
                        "}],"
                    "\"cipher\":\"aes-256-cbc\","
                    "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.2\""
                        "},{"
                            "\"id\":2,"
                            "\"system-id\":6861877834002393398,"
                            "\"version\":\"12\""
                        "}],"
                    "\"name\":\"stanza1\","
                    "\"status\":{"
                        "\"code\":0,"
                        "\"lock\":{"
                            "\"backup\":{"
                                "\"held\":false"
                            "}"
                        "},"
                        "\"message\":\"ok\""
                    "}"
                "}"
            "]",
            "json - archive-gap-detection - everything is fine");

        harnessCfgLoad(cfgCmdInfo, argListText);
        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: ok\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.2-1): 000000010000000000000001/000000030000000000000006\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (12-2): 000000010000000000000002/000000030000000000000010\n"
            "\n"
            "        full backup: 20200817-111046F\n"
            "            timestamp start/stop: 2020-08-17 09:10:46 / 2020-08-17 09:10:50\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000004\n"
            "            database size: 23.4MB, backup size: 23.4MB\n"
            "            repository size: 2.8MB, repository backup size: 2.8MB\n"
            "\n"
            "        diff backup: 20200817-111046F_20200817-111051D\n"
            "            timestamp start/stop: 2020-08-17 09:10:51 / 2020-08-17 09:10:53\n"
            "            wal start/stop: 000000010000000000000006 / 000000010000000000000006\n"
            "            database size: 23.4MB, backup size: 9.7KB\n"
            "            repository size: 2.8MB, repository backup size: 938B\n"
            "            backup reference list: 20200817-111046F\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111054I\n"
            "            timestamp start/stop: 2020-08-17 09:10:54 / 2020-08-17 09:10:56\n"
            "            wal start/stop: 000000010000000000000008 / 000000010000000000000008\n"
            "            database size: 23.4MB, backup size: 9.9KB\n"
            "            repository size: 2.8MB, repository backup size: 957B\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111103I\n"
            "            timestamp start/stop: 2020-08-17 09:11:03 / 2020-08-17 09:11:05\n"
            "            wal start/stop: 00000002000000000000000C / 00000002000000000000000C\n"
            "            database size: 23.4MB, backup size: 11.7KB\n"
            "            repository size: 2.8MB, repository backup size: 1.5KB\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n",
            "text - archive-gap-detection - everything is fine");

        // -------------------------------------------------------------------------------------------------------------------------
        // 1 missing archive with retention-archive-type=incr and retention-archive=5
        // 000000010000000000000001 will be older than the 1st backup start wal location
        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/000000010000000000000001-33099f27f375f42f3aab8c9d644ed24dba7071d7.gz",
            strZ(archiveDb2_1)))))), 0, "touch WAL1 file");
        // Remove 000000010000000000000005, so it will be reported missing
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/000000010000000000000007-0ec3fc389a62d3b1dc28ef1fea02a49ab70fd6b1.gz",
            strZ(archiveDb2_1)));

        StringList *argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=5");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.2-1\","
                            "\"max\":\"000000030000000000000006\","
                            "\"min\":\"000000010000000000000001\","
                            "\"missing-list\":\"\","
                            "\"missing-nb\":0"
                        "},{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"12-2\","
                            "\"max\":\"000000030000000000000010\","
                            "\"min\":\"000000010000000000000001\","
                            "\"missing-list\":\"000000010000000000000007\","
                            "\"missing-nb\":1"
                        "}"
                    "],"
                    "\"backup\":["
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000002\","
                                "\"stop\":\"000000010000000000000004\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":24534588,"
                                "\"repository\":{"
                                    "\"delta\":2931163,"
                                    "\"size\":2931163"
                                "},"
                                "\"size\":24534588"
                            "},"
                            "\"label\":\"20200817-111046F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1597655446,"
                                "\"stop\":1597655450"
                            "},"
                            "\"type\":\"full\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000006\","
                                "\"stop\":\"000000010000000000000006\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":9894,"
                                "\"repository\":{"
                                    "\"delta\":938,"
                                    "\"size\":2931218"
                                "},"
                                "\"size\":24534808"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111051D\","
                            "\"prior\":\"20200817-111046F\","
                            "\"reference\":[\"20200817-111046F\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655451,"
                                "\"stop\":1597655453"
                            "},"
                            "\"type\":\"diff\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000008\","
                                "\"stop\":\"000000010000000000000008\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":10113,"
                                "\"repository\":{"
                                    "\"delta\":957,"
                                    "\"size\":2931237"
                                "},"
                                "\"size\":24535027"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111054I\","
                            "\"prior\":\"20200817-111046F_20200817-111051D\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655454,"
                                "\"stop\":1597655456"
                            "},"
                            "\"type\":\"incr\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"00000002000000000000000C\","
                                "\"stop\":\"00000002000000000000000C\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":11956,"
                                "\"repository\":{"
                                    "\"delta\":1518,"
                                    "\"size\":2931615"
                                "},"
                                "\"size\":24536651"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111103I\","
                            "\"prior\":\"20200817-111046F_20200817-111054I\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655463,"
                                "\"stop\":1597655465"
                            "},"
                            "\"type\":\"incr\""
                        "}],"
                    "\"cipher\":\"aes-256-cbc\","
                    "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.2\""
                        "},{"
                            "\"id\":2,"
                            "\"system-id\":6861877834002393398,"
                            "\"version\":\"12\""
                        "}],"
                    "\"name\":\"stanza1\","
                    "\"status\":{"
                        "\"code\":4,"
                        "\"lock\":{"
                            "\"backup\":{"
                                "\"held\":false"
                            "}"
                        "},"
                        "\"message\":\"missing wal segment(s)\""
                    "}"
                "}"
            "]",
            "json - archive-gap-detection - 1 missing archive");

        harnessLogResult(
        "P00   WARN: min wal archive ('000000010000000000000001') found in the repo is older than the first backup start WAL"
        " location ('000000010000000000000002').\n"
        "            HINT: this might indicate a configuration error in the retention policy.");

        argListTmp = strLstDup(argListText);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=5");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing wal segment(s))\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.2-1): 000000010000000000000001/000000030000000000000006\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (12-2): 000000010000000000000001/000000030000000000000010\n"
            "        missing (1): 000000010000000000000007\n"
            "\n"
            "        full backup: 20200817-111046F\n"
            "            timestamp start/stop: 2020-08-17 09:10:46 / 2020-08-17 09:10:50\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000004\n"
            "            database size: 23.4MB, backup size: 23.4MB\n"
            "            repository size: 2.8MB, repository backup size: 2.8MB\n"
            "\n"
            "        diff backup: 20200817-111046F_20200817-111051D\n"
            "            timestamp start/stop: 2020-08-17 09:10:51 / 2020-08-17 09:10:53\n"
            "            wal start/stop: 000000010000000000000006 / 000000010000000000000006\n"
            "            database size: 23.4MB, backup size: 9.7KB\n"
            "            repository size: 2.8MB, repository backup size: 938B\n"
            "            backup reference list: 20200817-111046F\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111054I\n"
            "            timestamp start/stop: 2020-08-17 09:10:54 / 2020-08-17 09:10:56\n"
            "            wal start/stop: 000000010000000000000008 / 000000010000000000000008\n"
            "            database size: 23.4MB, backup size: 9.9KB\n"
            "            repository size: 2.8MB, repository backup size: 957B\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111103I\n"
            "            timestamp start/stop: 2020-08-17 09:11:03 / 2020-08-17 09:11:05\n"
            "            wal start/stop: 00000002000000000000000C / 00000002000000000000000C\n"
            "            database size: 23.4MB, backup size: 11.7KB\n"
            "            repository size: 2.8MB, repository backup size: 1.5KB\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n",
            "text - archive-gap-detection - 1 missing archive");

        harnessLogResult(
        "P00   WARN: min wal archive ('000000010000000000000001') found in the repo is older than the first backup start WAL"
        " location ('000000010000000000000002').\n"
        "            HINT: this might indicate a configuration error in the retention policy.");

        // -------------------------------------------------------------------------------------------------------------------------
        // 2 missing archives with retention-archive-type=incr and retention-archive=1
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/000000010000000000000001-33099f27f375f42f3aab8c9d644ed24dba7071d7.gz",
            strZ(archiveDb2_1)));
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/000000010000000000000005-c623389b95105f12f9559cc369c5d0fad5ea8c38.gz",
            strZ(archiveDb2_1)));
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/000000010000000000000009-8c7e9da8a9bd92b2a30800e2406a775b2a9d6f10.gz",
            strZ(archiveDb2_1)));
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/00000001000000000000000A-503151655f5391c418d9a9b0c56ed52b47a8706e.gz",
            strZ(archiveDb2_1)));
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/00000002000000000000000B-e26735cd123aa5ebc263e3cfc6118acf23930486.gz",
            strZ(archiveDb2_2)));
        // Remove 00000002000000000000000D and 00000003000000000000000F so they will be reported missing
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/00000002000000000000000D-953543645a27e72582730cb36677469be3e92860.gz",
            strZ(archiveDb2_2)));
        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/00000003000000000000000F-877867b41e3c31c34513f846c3921ff039e1bd6c.gz",
            strZ(archiveDb2_3)));

        argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.2-1\","
                            "\"max\":\"000000030000000000000006\","
                            "\"min\":\"000000010000000000000001\","
                            "\"missing-list\":\"\","
                            "\"missing-nb\":0"
                        "},{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"12-2\","
                            "\"max\":\"000000030000000000000010\","
                            "\"min\":\"000000010000000000000002\","
                            "\"missing-list\":\"00000002000000000000000D,00000003000000000000000F\","
                            "\"missing-nb\":2"
                        "}"
                    "],"
                    "\"backup\":["
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000002\","
                                "\"stop\":\"000000010000000000000004\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":24534588,"
                                "\"repository\":{"
                                    "\"delta\":2931163,"
                                    "\"size\":2931163"
                                "},"
                                "\"size\":24534588"
                            "},"
                            "\"label\":\"20200817-111046F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1597655446,"
                                "\"stop\":1597655450"
                            "},"
                            "\"type\":\"full\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000006\","
                                "\"stop\":\"000000010000000000000006\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":9894,"
                                "\"repository\":{"
                                    "\"delta\":938,"
                                    "\"size\":2931218"
                                "},"
                                "\"size\":24534808"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111051D\","
                            "\"prior\":\"20200817-111046F\","
                            "\"reference\":[\"20200817-111046F\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655451,"
                                "\"stop\":1597655453"
                            "},"
                            "\"type\":\"diff\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000008\","
                                "\"stop\":\"000000010000000000000008\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":10113,"
                                "\"repository\":{"
                                    "\"delta\":957,"
                                    "\"size\":2931237"
                                "},"
                                "\"size\":24535027"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111054I\","
                            "\"prior\":\"20200817-111046F_20200817-111051D\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655454,"
                                "\"stop\":1597655456"
                            "},"
                            "\"type\":\"incr\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"00000002000000000000000C\","
                                "\"stop\":\"00000002000000000000000C\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":11956,"
                                "\"repository\":{"
                                    "\"delta\":1518,"
                                    "\"size\":2931615"
                                "},"
                                "\"size\":24536651"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111103I\","
                            "\"prior\":\"20200817-111046F_20200817-111054I\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655463,"
                                "\"stop\":1597655465"
                            "},"
                            "\"type\":\"incr\""
                        "}],"
                    "\"cipher\":\"aes-256-cbc\","
                    "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.2\""
                        "},{"
                            "\"id\":2,"
                            "\"system-id\":6861877834002393398,"
                            "\"version\":\"12\""
                        "}],"
                    "\"name\":\"stanza1\","
                    "\"status\":{"
                        "\"code\":4,"
                        "\"lock\":{"
                            "\"backup\":{"
                                "\"held\":false"
                            "}"
                        "},"
                        "\"message\":\"missing wal segment(s)\""
                    "}"
                "}"
            "]",
            "json - archive-gap-detection - retention-archive-type=incr and retention-archive=1, 2 missing archives");

        argListTmp = strLstDup(argListText);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=1");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing wal segment(s))\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.2-1): 000000010000000000000001/000000030000000000000006\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (12-2): 000000010000000000000002/000000030000000000000010\n"
            "        missing (2): 00000002000000000000000D,00000003000000000000000F\n"
            "\n"
            "        full backup: 20200817-111046F\n"
            "            timestamp start/stop: 2020-08-17 09:10:46 / 2020-08-17 09:10:50\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000004\n"
            "            database size: 23.4MB, backup size: 23.4MB\n"
            "            repository size: 2.8MB, repository backup size: 2.8MB\n"
            "\n"
            "        diff backup: 20200817-111046F_20200817-111051D\n"
            "            timestamp start/stop: 2020-08-17 09:10:51 / 2020-08-17 09:10:53\n"
            "            wal start/stop: 000000010000000000000006 / 000000010000000000000006\n"
            "            database size: 23.4MB, backup size: 9.7KB\n"
            "            repository size: 2.8MB, repository backup size: 938B\n"
            "            backup reference list: 20200817-111046F\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111054I\n"
            "            timestamp start/stop: 2020-08-17 09:10:54 / 2020-08-17 09:10:56\n"
            "            wal start/stop: 000000010000000000000008 / 000000010000000000000008\n"
            "            database size: 23.4MB, backup size: 9.9KB\n"
            "            repository size: 2.8MB, repository backup size: 957B\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111103I\n"
            "            timestamp start/stop: 2020-08-17 09:11:03 / 2020-08-17 09:11:05\n"
            "            wal start/stop: 00000002000000000000000C / 00000002000000000000000C\n"
            "            database size: 23.4MB, backup size: 11.7KB\n"
            "            repository size: 2.8MB, repository backup size: 1.5KB\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n",
            "text - archive-gap-detection - retention-archive-type=incr and retention-archive=1, 2 missing archives");

        // -------------------------------------------------------------------------------------------------------------------------
        // retention-full-type=time, 2 missing archives with one needed for the backup consistency

        storageRemoveP(
            storageRepoWrite(), strNewFmt("%s/000000010000000000000003-07bf320a2c0a2705cbe045bd48a9b0f1b44a40c4.gz",
            strZ(archiveDb2_1)));

        TEST_RESULT_INT(system(
            strZ(strNewFmt("touch %s", strZ(strNewFmt("%s/00000002000000000000000D-953543645a27e72582730cb36677469be3e92860.gz",
            strZ(archiveDb2_2)))))), 0, "touch WAL2 file");

        argListTmp = strLstDup(argList);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=1");
        strLstAddZ(argListTmp, "--repo1-retention-full-type=time");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "["
                "{"
                    "\"archive\":["
                        "{"
                            "\"database\":{"
                                "\"id\":1"
                            "},"
                            "\"id\":\"9.2-1\","
                            "\"max\":\"000000030000000000000006\","
                            "\"min\":\"000000010000000000000001\","
                            "\"missing-list\":\"\","
                            "\"missing-nb\":0"
                        "},{"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"id\":\"12-2\","
                            "\"max\":\"000000030000000000000010\","
                            "\"min\":\"000000010000000000000002\","
                            "\"missing-list\":\"000000010000000000000003,00000003000000000000000F\","
                            "\"missing-nb\":2"
                        "}"
                    "],"
                    "\"backup\":["
                        "{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000002\","
                                "\"stop\":\"000000010000000000000004\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":24534588,"
                                "\"repository\":{"
                                    "\"delta\":2931163,"
                                    "\"size\":2931163"
                                "},"
                                "\"size\":24534588"
                            "},"
                            "\"label\":\"20200817-111046F\","
                            "\"prior\":null,"
                            "\"reference\":null,"
                            "\"timestamp\":{"
                                "\"start\":1597655446,"
                                "\"stop\":1597655450"
                            "},"
                            "\"type\":\"full\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000006\","
                                "\"stop\":\"000000010000000000000006\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":9894,"
                                "\"repository\":{"
                                    "\"delta\":938,"
                                    "\"size\":2931218"
                                "},"
                                "\"size\":24534808"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111051D\","
                            "\"prior\":\"20200817-111046F\","
                            "\"reference\":[\"20200817-111046F\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655451,"
                                "\"stop\":1597655453"
                            "},"
                            "\"type\":\"diff\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"000000010000000000000008\","
                                "\"stop\":\"000000010000000000000008\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":10113,"
                                "\"repository\":{"
                                    "\"delta\":957,"
                                    "\"size\":2931237"
                                "},"
                                "\"size\":24535027"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111054I\","
                            "\"prior\":\"20200817-111046F_20200817-111051D\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655454,"
                                "\"stop\":1597655456"
                            "},"
                            "\"type\":\"incr\""
                        "},{"
                            "\"archive\":{"
                                "\"start\":\"00000002000000000000000C\","
                                "\"stop\":\"00000002000000000000000C\""
                            "},"
                            "\"backrest\":{"
                                "\"format\":5,"
                                "\"version\":\"2.28\""
                            "},"
                            "\"database\":{"
                                "\"id\":2"
                            "},"
                            "\"info\":{"
                                "\"delta\":11956,"
                                "\"repository\":{"
                                    "\"delta\":1518,"
                                    "\"size\":2931615"
                                "},"
                                "\"size\":24536651"
                            "},"
                            "\"label\":\"20200817-111046F_20200817-111103I\","
                            "\"prior\":\"20200817-111046F_20200817-111054I\","
                            "\"reference\":[\"20200817-111046F\",\"20200817-111046F_20200817-111051D\"],"
                            "\"timestamp\":{"
                                "\"start\":1597655463,"
                                "\"stop\":1597655465"
                            "},"
                            "\"type\":\"incr\""
                        "}],"
                    "\"cipher\":\"aes-256-cbc\","
                    "\"db\":["
                        "{"
                            "\"id\":1,"
                            "\"system-id\":6626363367545678089,"
                            "\"version\":\"9.2\""
                        "},{"
                            "\"id\":2,"
                            "\"system-id\":6861877834002393398,"
                            "\"version\":\"12\""
                        "}],"
                    "\"name\":\"stanza1\","
                    "\"status\":{"
                        "\"code\":4,"
                        "\"lock\":{"
                            "\"backup\":{"
                                "\"held\":false"
                            "}"
                        "},"
                        "\"message\":\"missing wal segment(s)\""
                    "}"
                "}"
            "]",
            "json - archive-gap-detection - retention-full-type=time, 2 missing archives");

        argListTmp = strLstDup(argListText);
        strLstAddZ(argListTmp, "--repo1-retention-archive-type=incr");
        strLstAddZ(argListTmp, "--repo1-retention-archive=1");
        strLstAddZ(argListTmp, "--repo1-retention-full-type=time");
        harnessCfgLoad(cfgCmdInfo, argListTmp);

        TEST_RESULT_STR_Z(
            infoRender(),
            "stanza: stanza1\n"
            "    status: error (missing wal segment(s))\n"
            "    cipher: aes-256-cbc\n"
            "\n"
            "    db (prior)\n"
            "        wal archive min/max (9.2-1): 000000010000000000000001/000000030000000000000006\n"
            "\n"
            "    db (current)\n"
            "        wal archive min/max (12-2): 000000010000000000000002/000000030000000000000010\n"
            "        missing (2): 000000010000000000000003,00000003000000000000000F\n"
            "\n"
            "        full backup: 20200817-111046F\n"
            "            timestamp start/stop: 2020-08-17 09:10:46 / 2020-08-17 09:10:50\n"
            "            wal start/stop: 000000010000000000000002 / 000000010000000000000004\n"
            "            database size: 23.4MB, backup size: 23.4MB\n"
            "            repository size: 2.8MB, repository backup size: 2.8MB\n"
            "\n"
            "        diff backup: 20200817-111046F_20200817-111051D\n"
            "            timestamp start/stop: 2020-08-17 09:10:51 / 2020-08-17 09:10:53\n"
            "            wal start/stop: 000000010000000000000006 / 000000010000000000000006\n"
            "            database size: 23.4MB, backup size: 9.7KB\n"
            "            repository size: 2.8MB, repository backup size: 938B\n"
            "            backup reference list: 20200817-111046F\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111054I\n"
            "            timestamp start/stop: 2020-08-17 09:10:54 / 2020-08-17 09:10:56\n"
            "            wal start/stop: 000000010000000000000008 / 000000010000000000000008\n"
            "            database size: 23.4MB, backup size: 9.9KB\n"
            "            repository size: 2.8MB, repository backup size: 957B\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n"
            "\n"
            "        incr backup: 20200817-111046F_20200817-111103I\n"
            "            timestamp start/stop: 2020-08-17 09:11:03 / 2020-08-17 09:11:05\n"
            "            wal start/stop: 00000002000000000000000C / 00000002000000000000000C\n"
            "            database size: 23.4MB, backup size: 11.7KB\n"
            "            repository size: 2.8MB, repository backup size: 1.5KB\n"
            "            backup reference list: 20200817-111046F, 20200817-111046F_20200817-111051D\n",
            "text - archive-gap-detection - retention-full-type=time, 2 missing archives");

        // Assert error if no timeline switch can be found in history file(s)
        //--------------------------------------------------------------------------------------------------------------------------

        StorageWrite *historyWriteCorrupt = storageNewWriteP(
            storageLocalWrite(), strNewFmt("%s/9.2-1/00000003.history", strZ(archiveStanza1Path)));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(historyWriteCorrupt)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("sub12345"), NULL));

        TEST_RESULT_VOID(storagePutP(historyWriteCorrupt, BUFSTRDEF("ooops\n")), "corrupt db1 00000003.history file");

        TEST_ERROR_FMT(infoRender(), AssertError, "no target WAL have been found in the history file(s)");

        // -------------------------------------------------------------------------------------------------------------------------
        // Reset env
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");
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
        formatTextDb(stanzaInfo, result, NULL);

        TEST_RESULT_STR_Z(
            result,
            "\n"
            "    db (current)\n"
            "        full backup: 20181119-152138F\n"
            "            timestamp start/stop: 2018-11-16 15:47:56 / 2018-11-16 15:48:09\n"
            "            wal start/stop: n/a\n"
            "            database size: 0B, backup size: 0B\n"
            "            repository size: 0B, repository backup size: 0B\n",
            "formatTextDb only backup section (code coverage only)");
    }

    //******************************************************************************************************************************
    if (testBegin("cmdInfo()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--repo-path=%s", strZ(repoPath)));
        harnessCfgLoad(cfgCmdInfo, argList);

        storagePathCreateP(storageLocalWrite(), archivePath);
        storagePathCreateP(storageLocalWrite(), backupPath);

        // Redirect stdout to a file
        int stdoutSave = dup(STDOUT_FILENO);
        String *stdoutFile = strNewFmt("%s/stdout.info", testPath());

        THROW_ON_SYS_ERROR(freopen(strZ(stdoutFile), "w", stdout) == NULL, FileWriteError, "unable to reopen stdout");

        // Not in a test wrapper to avoid writing to stdout
        cmdInfo();

        // Restore normal stdout
        dup2(stdoutSave, STDOUT_FILENO);

        Storage *storage = storagePosixNewP(strNew(testPath()));
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storage, stdoutFile))), "No stanzas exist in the repository.\n",
            "    check text");

        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--set=bogus");

        TEST_ERROR_FMT(
            harnessCfgLoad(cfgCmdInfo, argList), OptionInvalidError, "option 'set' not valid without option 'stanza'");

        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--stanza=stanza1");
        harnessCfgLoad(cfgCmdInfo, argList);

        TEST_ERROR_FMT(
                cmdInfo(), FileMissingError, "manifest does not exist for backup 'bogus'\n"
                "HINT: is the backup listed when running the info command with --stanza option only?");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
