/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPq.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    String *stanza = strNew("db");
    String *fileName = strNew("test.info");
    String *backupStanzaPath = strNewFmt("repo/backup/%s", strZ(stanza));
    String *backupInfoFileName = strNewFmt("%s/backup.info", strZ(backupStanzaPath));
    String *archiveStanzaPath = strNewFmt("repo/archive/%s", strZ(stanza));
    String *archiveInfoFileName = strNewFmt("%s/archive.info", strZ(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    strLstAddZ(argListBase, "--no-online");
    strLstAdd(argListBase, strNewFmt("--stanza=%s", strZ(stanza)));
    strLstAdd(argListBase, strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
    strLstAdd(argListBase, strNewFmt("--repo1-path=%s/repo", testPath()));

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate(), checkStanzaInfo()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-host=/repo");
        strLstAddZ(argList, "--repo2-host=/repo/not/local");
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        TEST_ERROR_FMT(
            cmdStanzaCreate(), HostInvalidError, "stanza-create command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdStanzaCreate(), StopError, "stop file exists for stanza %s", strZ(stanza));
        TEST_RESULT_VOID(
            storageRemoveP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), "    remove the stop file");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - one repo, no files exist");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "put archive info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate success - multi-repo and encryption");

        argList = strLstDup(argListBase);
        strLstAdd(argList, strNewFmt("--repo2-path=%s/repo2", testPath()));
        strLstAddZ(argList, "--repo2-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO2_CIPHER_PASS", "12345678", true);
        strLstAdd(argList, strNewFmt("--repo3-path=%s/repo3", testPath()));
        strLstAdd(argList, strNewFmt("--repo4-path=%s/repo4", testPath()));
        strLstAddZ(argList, "--repo4-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO4_CIPHER_PASS", "87654321", true);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - files already exist on repo1 and both are valid");
        harnessLogResult(
            "P00   INFO: beginning stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' already exists on repo1 and is valid\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo2\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo3\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo4");

        String *archiveInfoFileNameRepo2 = strNewFmt("repo2/archive/%s/archive.info", strZ(stanza));
        String *backupInfoFileNameRepo2 = strNewFmt("repo2/backup/%s/backup.info", strZ(stanza));
        String *archiveInfoFileNameRepo3 = strNewFmt("repo3/archive/%s/archive.info", strZ(stanza));
        String *backupInfoFileNameRepo3 = strNewFmt("repo3/backup/%s/backup.info", strZ(stanza));
        String *archiveInfoFileNameRepo4 = strNewFmt("repo4/archive/%s/archive.info", strZ(stanza));
        String *backupInfoFileNameRepo4 = strNewFmt("repo4/backup/%s/backup.info", strZ(stanza));

        InfoArchive *infoArchive = NULL;
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageTest, archiveInfoFileNameRepo2, cipherTypeAes256Cbc, strNew("12345678")),
            "  load archive info");
        TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "  cipher sub set");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageTest, backupInfoFileNameRepo2, cipherTypeAes256Cbc, strNew("12345678")),
            "  load backup info");
        TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "  cipher sub set");

        TEST_RESULT_BOOL(
            strEq(infoArchiveCipherPass(infoArchive), infoBackupCipherPass(infoBackup)), false,
            "  cipher sub different for archive and backup");

        // Confirm non-encrypted repo created successfully
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageTest, archiveInfoFileNameRepo3, cipherTypeNone, NULL), "  load archive info");
        TEST_RESULT_PTR(infoArchiveCipherPass(infoArchive), NULL, "  archive cipher sub not set on non-encrypted repo");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageTest, backupInfoFileNameRepo3, cipherTypeNone, NULL),"  load backup info");
        TEST_RESULT_PTR(infoBackupCipherPass(infoBackup), NULL, "  backup cipher sub not set on non-encrypted repo");

        // Confirm other repo encrypted with different password
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageTest, archiveInfoFileNameRepo4, cipherTypeAes256Cbc, strNew("87654321")),
            "  load archive info");
        TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "  cipher sub set");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageTest, backupInfoFileNameRepo4, cipherTypeAes256Cbc, strNew("87654321")),
            "  load backup info");
        TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "  cipher sub set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate missing files - multi-repo and encryption");

        // Remove backup.info on repo1
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true), "backup.info removed");

        // Remove archive.info on repo2
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileNameRepo2, .errorOnMissing = true), "archive.info removed");

        // Remove info files on repo3
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileNameRepo3, .errorOnMissing = true), "archive.info removed");
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileNameRepo3, .errorOnMissing = true), "backup.info removed");

        // Remove copy files repo4
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileNameRepo4)), .errorOnMissing = true),
            "archive.info.copy removed");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileNameRepo4)), .errorOnMissing = true),
            "backup.info.copy removed");

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with missing files");
        harnessLogResult(
            "P00   INFO: beginning stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo2\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo3\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo4");

        TEST_RESULT_BOOL(
            bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName))))),
            true, "backup.info recreated repo1 from backup.info.copy");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileNameRepo2)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileNameRepo2))))),
            true, "archive.info repo2 recreated from archive.info.copy");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileNameRepo3)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileNameRepo3))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileNameRepo3)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileNameRepo3)))))),
            true, "info files recreated repo3  from copy files");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileNameRepo4)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileNameRepo4))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileNameRepo4)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileNameRepo4)))))),
            true, "info files recreated repo4 from info files");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaDelete - multi-repo and encryption, delete");

        StringList *argListCmd = strLstNew();
        strLstAdd(argListCmd, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argListCmd, strNewFmt("--repo2-path=%s/repo3", testPath()));
        strLstAdd(argListCmd, strNewFmt("--repo3-path=%s/repo3", testPath()));
        strLstAdd(argListCmd, strNewFmt("--repo4-path=%s/repo4", testPath()));
        strLstAdd(argListCmd, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argListCmd, strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));

        StringList *argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "4");
        harnessCfgLoad(cfgCmdStanzaDelete, argListDelete);

        TEST_ERROR_FMT(
            cmdStanzaDelete(), FileMissingError, "stop file does not exist for stanza 'db'\n"
            "HINT: has the pgbackrest stop command been run on this server for this stanza?");

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - repo4");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo4/archive/%s", strZ(stanza))), false, "    stanza deleted");
        TEST_RESULT_BOOL(
            storageExistsP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))), false, "    stop file removed");
        unsetenv("PGBACKREST_REPO4_CIPHER_PASS");

        // Stanza with directories only
        argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "3");
        harnessCfgLoad(cfgCmdStanzaDelete, argListDelete);

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("repo3/archive/%s/9.6-1/1234567812345678", strZ(stanza))),
            "create archive sub directory");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("repo3/backup/%s/20190708-154306F", strZ(stanza))),
            "create backup sub directory");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - repo3 - sub directories only");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo3/archive/%s", strZ(stanza))), false, "    stanza archive deleted");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo3/backup/%s", strZ(stanza))), false, "    stanza backup deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate errors");

        argList = strLstDup(argListBase);
        strLstAdd(argList, strNewFmt("--repo2-path=%s/repo2", testPath()));
        strLstAddZ(argList, "--repo2-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO2_CIPHER_PASS", "12345678", true);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Backup files removed - archive.info and archive.info.copy exist repo2
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileNameRepo2)), .errorOnMissing = true),
            "backup.info.copy removed repo2");
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileNameRepo2, .errorOnMissing = true),
            "backup.info removed repo2");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 2);
        harnessLogResult(
            "P00   INFO: beginning stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' already exists on repo1 and is valid\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo2");

        // Archive files removed - backup.info and backup.info.copy exist repo1
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName)), .errorOnMissing = true),
            "archive.info.copy removed repo1");
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true),
            "archive.info removed repo1");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 1);
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Delete the last repo so only 1 remains
        argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "2");
        harnessCfgLoad(cfgCmdStanzaDelete, argListDelete);
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - only 1 remains");
        unsetenv("PGBACKREST_REPO2_CIPHER_PASS");

        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Archive files removed - backup.info exists
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName)), .errorOnMissing = true),
            "backup.info.copy removed");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 1);
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Archive files removed - backup.info.copy exists
        TEST_RESULT_VOID(
            storageMoveP(storageTest,
                storageNewReadP(storageTest, backupInfoFileName),
                storageNewWriteP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName)))),
            "backup.info moved to backup.info.copy");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "backup.info exists but archive.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 1);
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Backup files removed - archive.info file exists
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName)), .errorOnMissing = true),
            "backup.info.copy removed");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 1);
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Backup files removed - archive.info.copy file exists
        TEST_RESULT_VOID(
            storageMoveP(storageTest,
                storageNewReadP(storageTest, archiveInfoFileName),
                storageNewWriteP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName)))),
                "archive.info moved to archive.info.copy");
        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileMissingError, "archive.info exists but backup.info is missing on repo%u\n"
            "HINT: this may be a symptom of repository corruption!", 1);
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // checkStanzaInfo() - already checked in checkTest so just a sanity check here
        //--------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - db id
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file - bad db-id");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        // Copy files may or may not exist - remove
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName)));
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName)));

        // Create an archive.info file and backup.info files that match but do not match the current database version
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info file");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Create archive.info and backup.info files that match but do not match the current database system-id
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        TEST_ERROR_FMT(
            cmdStanzaCreate(), FileInvalidError, "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Remove the info files and add sub directory to backup
        TEST_RESULT_VOID(storageRemoveP(storageTest, archiveInfoFileName, .errorOnMissing = true), "archive.info removed");
        TEST_RESULT_VOID(storageRemoveP(storageTest, backupInfoFileName, .errorOnMissing = true), "backup.info removed");

        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("%s/backup.history", strZ(backupStanzaPath))),
            "create directory in backup");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "backup directory not empty");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // File in archive, directory in backup
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("%s/somefile", strZ(archiveStanzaPath))), BUFSTRDEF("some content")),
            "create file in archive");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "backup directory and/or archive directory not empty");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // File in archive, backup empty
        TEST_RESULT_VOID(
            storagePathRemoveP(storageTest, strNewFmt("%s/backup.history", strZ(backupStanzaPath))), "remove backup subdir");
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");

        // Repeat last test using --force (deprecated)
        //--------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--force");
        harnessCfgLoad(cfgCmdStanzaCreate, argList);
        TEST_ERROR_FMT(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");
        harnessLogResult(
            "P00   WARN: option --force is no longer supported\n"
            "P00   INFO: beginning stanza-create for stanza 'db' on repo1");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgValidate(), online=y"))
    {
        String *pg1 = strNew("pg1");
        String *pg1Path = strNewFmt("%s/%s", testPath(), strZ(pg1));

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strZ(pg1Path)));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // pgControl and database match
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - db online");
        harnessLogResult("P00   INFO: beginning stanza-create for stanza 'db' on repo1");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("repo/archive/%s/archive.info", strZ(stanza))), true, "    stanza created");

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - db online");
        harnessLogResult("P00   INFO: stanza 'db' is already up to date");

        // Version mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control with different version
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_91, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            pgValidate(), DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.",
            strZ(pgVersionToStr(PG_VERSION_92)), strZ(pg1Path), strZ(pgVersionToStr(PG_VERSION_91)), strZ(pg1Path), strZ(pg1Path));

        // Path mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(
                1, "dbname='postgres' port=5432", PG_VERSION_92, strZ(strNewFmt("%s/pg2", testPath())), false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR_FMT(
            pgValidate(), DbMismatchError, "version '%s' and path '%s' queried from cluster do not match version '%s' and '%s'"
            " read from '%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL
            "'\nHINT: the pg1-path and pg1-port settings likely reference different clusters.",
            strZ(pgVersionToStr(PG_VERSION_92)), strZ(strNewFmt("%s/pg2", testPath())), strZ(pgVersionToStr(PG_VERSION_92)),
            strZ(pg1Path), strZ(pg1Path));

        // Primary at pg2
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--pg2-path=%s", strZ(pg1Path)));
        strLstAddZ(argList, "--pg2-port=5434");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control for primary
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(pg1))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        // Create pg_control for standby
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, testPath())),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_94, .systemId = 6569239123849665700}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, testPath(), true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(2, "dbname='postgres' port=5434", PG_VERSION_92, strZ(pg1Path), false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        PgControl pgControl = {0};
        TEST_ASSIGN(pgControl, pgValidate(), "validate primary on pg2");
        TEST_RESULT_UINT(pgControl.version, PG_VERSION_92, "    version set");
        TEST_RESULT_UINT(pgControl.systemId, 6569239123849665699, "    systemId set");
        TEST_RESULT_UINT(pgControl.catalogVersion, 201204301, "    catalogVersion set");
    }

// DELETE - covered in run=1
     // *****************************************************************************************************************************
//     if (testBegin("encryption, multi-repos"))
//     {
//         StringList *argList = strLstNew();
//         strLstAddZ(argList, "--no-online");
//         strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
//         strLstAdd(argList, strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
//         strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
//         strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
//         setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345678", true);
//         strLstAdd(argList, strNewFmt("--repo3-path=%s/repo3", testPath()));
//         strLstAdd(argList, strNewFmt("--repo4-path=%s/repo4", testPath()));
//         strLstAddZ(argList, "--repo4-cipher-type=aes-256-cbc");
//         setenv("PGBACKREST_REPO4_CIPHER_PASS", "87654321", true);
//         harnessCfgLoad(cfgCmdStanzaCreate, argList);
//
//         // Create pg_control
//         storagePutP(
//             storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanza))),
//             pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));
//
//         TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - encryption, multi repo");
//
//         InfoArchive *infoArchive = NULL;
//         TEST_ASSIGN(
//             infoArchive, infoArchiveLoadFile(storageTest, archiveInfoFileName, cipherTypeAes256Cbc, strNew("12345678")),
//             "  load archive info");
//         TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "  cipher sub set");
//
//         InfoBackup *infoBackup = NULL;
//         TEST_ASSIGN(
//             infoBackup, infoBackupLoadFile(storageTest, backupInfoFileName, cipherTypeAes256Cbc, strNew("12345678")),
//             "  load backup info");
//         TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "  cipher sub set");
//
//         TEST_RESULT_BOOL(
//             strEq(infoArchiveCipherPass(infoArchive), infoBackupCipherPass(infoBackup)), false,
//             "  cipher sub different for archive and backup");
//
//         // Confirm non-encrypted repo created successfully
//         TEST_ASSIGN(
//             infoArchive, infoArchiveLoadFile(storageTest, strNewFmt("repo3/archive/%s/archive.info", strZ(stanza)), cipherTypeNone,
//             NULL), "  load archive info");
//         TEST_RESULT_PTR(infoArchiveCipherPass(infoArchive), NULL, "  archive cipher sub not set on non-encrypted repo");
//
//         TEST_ASSIGN(
//             infoBackup, infoBackupLoadFile(storageTest, strNewFmt("repo3/backup/%s/backup.info", strZ(stanza)), cipherTypeNone,
//             NULL),"  load backup info");
//         TEST_RESULT_PTR(infoBackupCipherPass(infoBackup), NULL, "  backup cipher sub not set on non-encrypted repo");
//
//         // Confirm other repo encrypted with different password
//         TEST_ASSIGN(
//             infoArchive, infoArchiveLoadFile(storageTest, strNewFmt("repo4/archive/%s/archive.info", strZ(stanza)),
//             cipherTypeAes256Cbc, strNew("87654321")), "  load archive info");
//         TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "  cipher sub set");
//
//         TEST_ASSIGN(
//             infoBackup, infoBackupLoadFile(storageTest, strNewFmt("repo4/backup/%s/backup.info", strZ(stanza)), cipherTypeAes256Cbc,
//             strNew("87654321")), "  load backup info");
//         TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "  cipher sub set");
// // CSHANG Should probably integrate delete test here so we can delete one of the repos and rerun with issues/failure - like remove the info.copy files from one, have a host optino set for another and confirm nothing after is created/touched (info message that the stanza already created on a repo should show us that some were skipped)
//
    // }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaUpgrade()"))
    {
        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

        TEST_ERROR_FMT(
            cmdStanzaUpgrade(), HostInvalidError, "stanza-upgrade command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

        // Create the stop file
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(cmdStanzaUpgrade(), StopError, "stop file exists for stanza %s", strZ(stanza));
        TEST_RESULT_VOID(
            storageRemoveP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), "    remove the stop file");

        //--------------------------------------------------------------------------------------------------------------------------
        // Load Parameters
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create");

        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListBase);
        harnessCfgLoad(cfgCmdStanzaUpgrade, argList);

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - files already exist and both are valid");
        harnessLogResult("P00   INFO: stanza 'db' is already up to date");

        // Remove the copy files
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName)), .errorOnMissing = true);
        storageRemoveP(storageTest, strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName)), .errorOnMissing = true);

        // backup info up to date but archive info db-id mismatch
        //--------------------------------------------------------------------------------------------------------------------------
        String *contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_ERROR_FMT(
            cmdStanzaUpgrade(), FileInvalidError, "backup info file and archive info file do not match on repo%u\n"
            "archive: id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!", 1);

        // backup info up to date but archive info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        String *contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - version");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        // archive info up to date but backup info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - version");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");

        // backup info up to date but archive info system-id is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        contentArchive = strNew
        (
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
                "put archive info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - system-id");
        contentArchive = strNew
        (
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
                "    put archive info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza archive info files are equal");

        // archive info up to date but backup info system-id is not
        //--------------------------------------------------------------------------------------------------------------------------
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
                "put backup info to file");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - system-id");
        contentBackup = strNew
        (
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n"
        );
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
                "    put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName))))) &&
            bufEq(
                storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
                storageGetP(storageNewReadP(storageTest, fileName)))),
            true, "    test and stanza backup info files are equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaDelete(), stanzaDelete(), manifestDelete()"))
    {
        // Load Parameters
        StringList *argListCmd = strLstNew();
        strLstAdd(argListCmd, strNewFmt("--repo1-path=%s/repo", testPath()));

        StringList *argList = strLstDup(argListCmd);
        strLstAddZ(argList, "--repo1-host=/repo/not/local");
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
        harnessCfgLoad(cfgCmdStanzaDelete, argList);

        TEST_ERROR_FMT(
            cmdStanzaDelete(), HostInvalidError, "stanza-delete command must be run on the repository host");

        //--------------------------------------------------------------------------------------------------------------------------
        String *stanzaOther = strNew("otherstanza");

        // Load Parameters
        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanzaOther)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanzaOther)));
        strLstAddZ(argList, "--no-online");
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control for stanza-create
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanzaOther))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza that will not be deleted");

        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
        harnessCfgLoad(cfgCmdStanzaDelete, argList);

        // stanza already deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - success on stanza does not exist");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, NULL), true, "    archiveList=NULL, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), NULL), true, "    archiveList=0, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, strLstNew()), true, "    archiveList=NULL, backupList=0");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), strLstNew()), true, "    archiveList=0, backupList=0");

        // create and delete a stanza
// CSHANG Could probably combine this one test with the encryption, multi-repo test and also to confirm only the repo that is requested is deleted. It appears stanza-delete should NOT have to be changed at all - all error messages are not repo specific but stanza specific - we're forcing the user to delete one at a time. BUT we need to be aware of --force option - will this cause any issues? (hopefully not)
        //--------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
        strLstAddZ(argList, "--no-online");
        harnessCfgLoad(cfgCmdStanzaCreate, argList);

        // Create pg_control for stanza-create
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanza))),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));
// CSHANG These are no longer necessary
        // TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza to be deleted");
        // TEST_RESULT_BOOL(
        //     storageExistsP(storageTest, strNewFmt("repo/archive/%s/archive.info", strZ(stanza))), true, "    stanza created");
// CSHANG Moving to run =1
        // argList = strLstDup(argListCmd);
        // strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        // strLstAdd(argList,strNewFmt("--pg1-path=%s/%s", testPath(), strZ(stanza)));
        // harnessCfgLoad(cfgCmdStanzaDelete, argList);
        //
        // TEST_ERROR_FMT(
        //     cmdStanzaDelete(), FileMissingError, "stop file does not exist for stanza 'db'\n"
        //     "HINT: has the pgbackrest stop command been run on this server for this stanza?");
        //
        // // Create the stop file
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
        //         "create stop file");
        //
        // TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        // TEST_RESULT_BOOL(
        //     storagePathExistsP(storageTest, strNewFmt("repo/archive/%s", strZ(stanza))), false, "    stanza deleted");
        // TEST_RESULT_BOOL(
        //     storageExistsP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))), false, "    stop file removed");

        // // Create stanza with directories only
        // //--------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(
        //     storagePathCreateP(storageTest, strNewFmt("repo/archive/%s/9.6-1/1234567812345678", strZ(stanza))),
        //     "create archive sub directory");
        // TEST_RESULT_VOID(
        //     storagePathCreateP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strZ(stanza))),
        //     "create backup sub directory");
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
        //         "create stop file");
        // TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - sub directories only");
        // TEST_RESULT_BOOL(
        //     storagePathExistsP(storageTest, strNewFmt("repo/archive/%s", strZ(stanza))), false, "    stanza archive deleted");
        // TEST_RESULT_BOOL(
        //     storagePathExistsP(storageTest, strNewFmt("repo/backup/%s", strZ(stanza))), false, "    stanza backup deleted");

        // Create stanza archive only
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/archive/%s/archive.info", strZ(stanza))), BUFSTRDEF("")),
                "create archive.info");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - archive only");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo/archive/%s", strZ(stanza))), false, "    stanza deleted");

        // Create stanza backup only
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/backup.info", strZ(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - backup only");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo/backup/%s", strZ(stanza))), false, "    stanza deleted");

        // Create a backup file that matches the regex for a backup directory
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strZ(stanza))), BUFSTRDEF("")),
                "backup file that looks like a directory");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_ERROR_FMT(
            cmdStanzaDelete(), FileRemoveError,
            "unable to remove '%s/repo/backup/%s/20190708-154306F/backup.manifest': [20] Not a directory", testPath(),
            strZ(stanza));
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F", strZ(stanza))), "remove backup directory");

        // Create backup manifest
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strZ(stanza))),
                BUFSTRDEF("")), "create backup.manifest only");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
                strZ(stanza))), BUFSTRDEF("")), "create backup.manifest.copy only");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
                strZ(stanza))), BUFSTRDEF("")), "create backup.manifest");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
                strZ(stanza))), BUFSTRDEF("")), "create backup.manifest.copy");
        TEST_RESULT_VOID(manifestDelete(storageRepoWrite()), "delete manifests");
        TEST_RESULT_BOOL(
            (storageExistsP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F/backup.manifest", strZ(stanza))) &&
            storageExistsP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191726I/backup.manifest.copy",
            strZ(stanza))) &&
            storageExistsP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest",
            strZ(stanza))) &&
            storageExistsP(storageTest, strNewFmt("repo/backup/%s/20190708-154306F_20190716-191800D/backup.manifest.copy",
            strZ(stanza)))), false, "    all manifests deleted");

        // Create only stanza paths
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("repo/archive/%s", strZ(stanza))), "create empty stanza archive path");
        TEST_RESULT_VOID(
            storagePathCreateP(storageTest, strNewFmt("repo/backup/%s", strZ(stanza))), "create empty stanza backup path");
        TEST_RESULT_VOID(cmdStanzaDelete(), "    stanza delete - empty directories");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delete errors when pg appears to be running");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo/backup/%s/backup.info", strZ(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageLocalWrite(), lockStopFileName(cfgOptionStr(cfgOptStanza))), BUFSTRDEF("")),
                "create stop file");
        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNewFmt("%s/" PG_FILE_POSTMASTERPID, strZ(stanza))), BUFSTRDEF("")),
            "create pid file");
        TEST_ERROR_FMT(
            cmdStanzaDelete(), PgRunningError, PG_FILE_POSTMASTERPID " exists - looks like " PG_NAME " is running. "
            "To delete stanza 'db', shut down " PG_NAME " for stanza 'db' and try again, or use --force.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("force delete when pg appears to be running");

        strLstAddZ(argList,"--force");
        harnessCfgLoad(cfgCmdStanzaDelete, argList);
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete --force");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo/backup/%s", strZ(stanza))), false, "    stanza deleted");

        // Ensure other stanza never deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("repo/archive/%s/archive.info", strZ(stanzaOther))), true, "otherstanza exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
