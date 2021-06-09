/***********************************************************************************************************************************
Test Stanza Commands
***********************************************************************************************************************************/
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create storage objects for the testing location and for the harness directory for items such as logs, stop files, etc
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
    Storage *storageHrn = storagePosixNewP(HRN_PATH_STR, .write = true);

    const String *stanza = STRDEF("db");  // CSHANG See if can remove
    // const String *fileName = STRDEF("test.info");
    // String *backupStanzaPath = strNewFmt("repo/backup/%s", strZ(stanza));
    // String *backupInfoFileName = strNewFmt("%s/backup.info", strZ(backupStanzaPath));
    // String *archiveStanzaPath = strNewFmt("repo/archive/%s", strZ(stanza));
    // String *archiveInfoFileName = strNewFmt("%s/archive.info", strZ(archiveStanzaPath));

    StringList *argListBase = strLstNew();
    hrnCfgArgRawBool(argListBase, cfgOptOnline, false);
    hrnCfgArgRawZ(argListBase, cfgOptStanza, "db");
    hrnCfgArgRawZ(argListBase, cfgOptPgPath, TEST_PATH_PG); // CSHANG See if can replace with TEST_PATH_PG
    hrnCfgArgRawZ(argListBase, cfgOptRepoPath, TEST_PATH_REPO);

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate(), checkStanzaInfo(), cmdStanzaDelete()"))
    {
        TEST_TITLE("stanza-create: repo option not valid");

        // Load Parameters
        StringList *argList =  strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaCreate, argList), OptionInvalidError, "option 'repo' not valid for command 'stanza-create'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-create: stop file error");

        HRN_CFG_LOAD(cfgCmdStanzaCreate, argListBase);

        // Create the stop file, test, then remove the stop file
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));
        TEST_ERROR_FMT(cmdStanzaCreate(), StopError, "stop file exists for stanza %s", strZ(stanza));
        HRN_STORAGE_REMOVE(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-create: validate info files created");

        argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - one repo, no files exist");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n",
            .comment = "put archive info contents to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza archive info files are equal");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info contents to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate success - multi-repo and encryption");

        argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, "12345678");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 3, TEST_PATH "/repo3");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, TEST_PATH "/repo4");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 4, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 4, "87654321");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - files already exist on repo1 and both are valid");
        TEST_RESULT_LOG(
            "P00   INFO: stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' already exists on repo1 and is valid\n"
            "P00   INFO: stanza-create for stanza 'db' on repo2\n"
            "P00   INFO: stanza-create for stanza 'db' on repo3\n"
            "P00   INFO: stanza-create for stanza 'db' on repo4");
// CSHANG remove:
        // String *archiveInfoFileNameRepo2 = strNewFmt("repo2/archive/%s/archive.info", strZ(stanza));
        // String *backupInfoFileNameRepo2 = strNewFmt("repo2/backup/%s/backup.info", strZ(stanza));
        // String *archiveInfoFileNameRepo3 = strNewFmt("repo3/archive/%s/archive.info", strZ(stanza));
        // String *backupInfoFileNameRepo3 = strNewFmt("repo3/backup/%s/backup.info", strZ(stanza));
        // String *archiveInfoFileNameRepo4 = strNewFmt("repo4/archive/%s/archive.info", strZ(stanza));
        // String *backupInfoFileNameRepo4 = strNewFmt("repo4/backup/%s/backup.info", strZ(stanza));

        InfoArchive *infoArchive = NULL;
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageRepoIdx(1), INFO_ARCHIVE_PATH_FILE_STR, cipherTypeAes256Cbc,
            STRDEF("12345678")), "load archive info from encrypted repo2");
        TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "cipher sub set");

        InfoBackup *infoBackup = NULL;
        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(1), INFO_BACKUP_PATH_FILE_STR, cipherTypeAes256Cbc, STRDEF("12345678")),
            "load backup info from encrypted repo2");
        TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "cipher sub set");

        TEST_RESULT_BOOL(
            strEq(infoArchiveCipherPass(infoArchive), infoBackupCipherPass(infoBackup)), false,
            "cipher sub different for archive and backup");

        // Confirm non-encrypted repo created successfully
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageRepoIdx(2), INFO_ARCHIVE_PATH_FILE_STR, cipherTypeNone, NULL),
            "load archive info from repo3");
        TEST_RESULT_PTR(infoArchiveCipherPass(infoArchive), NULL, "archive cipher sub not set on non-encrypted repo");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(2), INFO_BACKUP_PATH_FILE_STR, cipherTypeNone, NULL),
            "load backup info from repo3");
        TEST_RESULT_PTR(infoBackupCipherPass(infoBackup), NULL, "backup cipher sub not set on non-encrypted repo");

        // Confirm other repo encrypted with different password
        TEST_ASSIGN(
            infoArchive, infoArchiveLoadFile(storageRepoIdx(3), INFO_ARCHIVE_PATH_FILE_STR, cipherTypeAes256Cbc,
            STRDEF("87654321")), "load archive info from encrypted repo4");
        TEST_RESULT_PTR_NE(infoArchiveCipherPass(infoArchive), NULL, "cipher sub set");

        TEST_ASSIGN(
            infoBackup, infoBackupLoadFile(storageRepoIdx(3), INFO_BACKUP_PATH_FILE_STR, cipherTypeAes256Cbc, STRDEF("87654321")),
            "load backup info from encrypted repo4");
        TEST_RESULT_PTR_NE(infoBackupCipherPass(infoBackup), NULL, "cipher sub set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate missing files - multi-repo and encryption");

        // Remove backup.info on repo1
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, .remove = true);

        // Remove archive.info on repo2
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE, .remove = true);

        // Remove info files on repo3
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(2), INFO_ARCHIVE_PATH_FILE, .remove = true);
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(2), INFO_BACKUP_PATH_FILE, .remove = true);

        // Remove copy files repo4
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(3), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, .remove = true);
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(3), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .remove = true);

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - success with missing files");
        TEST_RESULT_LOG(
            "P00   INFO: stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: stanza-create for stanza 'db' on repo2\n"
            "P00   INFO: stanza-create for stanza 'db' on repo3\n"
            "P00   INFO: stanza-create for stanza 'db' on repo4");

        TEST_RESULT_BOOL(
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))),
            true, "backup.info recreated repo1 from backup.info.copy");
        TEST_RESULT_BOOL(
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(1), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(1), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT)))),
            true, "archive.info repo2 recreated from archive.info.copy");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(2), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(2), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(2), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(2), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT))))),
            true, "info files recreated repo3 from copy files");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(3), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(3), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(3), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(3), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT))))),
            true, "copy files recreated repo4 from info files");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaDelete - multi-repo and encryption, delete");

        StringList *argListCmd = strLstNew();
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 1, TEST_PATH_REPO);
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 3, TEST_PATH "/repo3");
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 4, TEST_PATH "/repo4");
        hrnCfgArgRaw(argListCmd, cfgOptStanza, stanza);
        hrnCfgArgRawZ(argListCmd, cfgOptPgPath, TEST_PATH_PG);

        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaDelete, argListCmd), OptionRequiredError,
            "stanza-delete command requires option: repo\n"
            "HINT: this command requires a specific repository to operate on");

        // Add the repo option
        StringList *argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "4");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argListDelete);

        TEST_ERROR(
            cmdStanzaDelete(), FileMissingError,
            "stop file does not exist for stanza 'db'\n"
            "HINT: has the pgbackrest stop command been run on this server for this stanza?");

        // Create the stop file
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        TEST_STORAGE_LIST(
            storageTest, "repo4",
            "archive/\n"
            "archive/db/\n"
            "archive/db/archive.info\n"
            "archive/db/archive.info.copy\n"
            "backup/\n"
            "backup/db/\n"
            "backup/db/backup.info\n"
            "backup/db/backup.info.copy\n",
            .comment = "stanza exists in repo4");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - repo4");

        TEST_STORAGE_LIST(storageTest, "repo4", "archive/\nbackup/\n", .comment = "stanza deleted");

        TEST_RESULT_BOOL(
            storageExistsP(storageHrn, lockStopFileName(cfgOptionStr(cfgOptStanza))), false, "confirm stop file removed");

        // Remove the cipher pass environment variable otherwise stanza-create will recreate the stanza
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 4);

        // Stanza with directories only
        argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "3");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argListDelete);

        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(2), STORAGE_REPO_ARCHIVE "/9.6-1/1234567812345678", .comment = "create archive sub directory");
        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(2), STORAGE_REPO_BACKUP "/20190708-154306F", .comment = "create backup sub directory");
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))), .comment = "create stop file");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - repo3 - sub directories only");

        TEST_STORAGE_LIST(storageTest, "repo3", "archive/\nbackup/\n", .comment = "stanza deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaCreate errors");

        argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, "12345678");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Backup files removed - archive.info and archive.info.copy exist repo2
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .remove = true,
            .comment = "repo2: remove backup.info.copy");
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, .remove = true, .comment = "repo2: remove backup.info");

        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "archive.info exists but backup.info is missing on repo2\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG(
            "P00   INFO: stanza-create for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' already exists on repo1 and is valid\n"
            "P00   INFO: stanza-create for stanza 'db' on repo2");

        // Archive files removed - backup.info and backup.info.copy exist repo1
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, .remove = true,
            .comment = "repo1: remove archive.info.copy");
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE, .remove = true, .comment = "repo1: remove archive.info");

        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "backup.info exists but archive.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Delete the last repo so only 1 remains
        argListDelete = strLstDup(argListCmd);
        hrnCfgArgRawZ(argListDelete, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argListDelete);

        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))), .comment = "create stop file");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - only 1 remains");

        // Remove the cipher pass environment variable otherwise stanza-create will recreate the stanza
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Archive files removed - backup.info exists
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .remove = true,
            .comment = "repo1: remove backup.info.copy");
        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "backup.info exists but archive.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Archive files removed - backup.info.copy exists, backup.info moved to backup.info.copy
        HRN_SYSTEM(
            "mv " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE " " TEST_PATH_REPO "/backup/db/" INFO_BACKUP_FILE INFO_COPY_EXT); // CSHANG need to change to HRN_STORAGE_MOVE when we have one
        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "backup.info exists but archive.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Backup files removed - archive.info file exists
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n",
            .comment = "put archive info to file repo1");
        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .remove = true,
            .comment = "repo1: remove backup.info.copy");
        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "archive.info exists but backup.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Backup files removed - archive.info.copy file exists (renamed from archive.info)
        HRN_SYSTEM(
            "mv " TEST_PATH_REPO "/archive/db/" INFO_ARCHIVE_FILE " " TEST_PATH_REPO "/archive/db/" INFO_ARCHIVE_FILE
            INFO_COPY_EXT); // CSHANG need to change to HRN_STORAGE_MOVE when we have one
        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "archive.info exists but backup.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // checkStanzaInfo() - already checked in checkTest so just a sanity check here
        //--------------------------------------------------------------------------------------------------------------------------
        // Create a corrupted backup file - db id
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info to file - bad db-id");

        TEST_ERROR(
            cmdStanzaCreate(), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        // Copy files may or may not exist - remove
        HRN_STORAGE_REMOVE(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, .comment = "repo1: remove archive.info.copy");
        HRN_STORAGE_REMOVE(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .comment = "repo1: remove backup.info.copy");

        // Create an archive.info file and backup.info files that match but do not match the current database version
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n");

        TEST_ERROR(
            cmdStanzaCreate(), FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Create archive.info and backup.info files that match but do not match the current database system-id
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n");

        TEST_ERROR(
            cmdStanzaCreate(), FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Remove the info files and add sub directory to backup
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE, .remove = true);
        TEST_STORAGE_EXISTS(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, .remove = true);
        HRN_STORAGE_PATH_CREATE(
            storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/backup.history", .comment = "create directory in backup");

        TEST_ERROR(cmdStanzaCreate(), PathNotEmptyError, "backup directory not empty");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // File in archive, directory in backup
        HRN_STORAGE_PUT_Z(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/somefile", "some content");
        TEST_ERROR(cmdStanzaCreate(), PathNotEmptyError, "backup directory and/or archive directory not empty");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // File in archive, backup empty
        HRN_STORAGE_PATH_REMOVE(storageRepoIdxWrite(0), STORAGE_REPO_BACKUP "/backup.history", .comment = "remove backup subdir");
        TEST_ERROR(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        // Repeat last test using --force (deprecated)
        //--------------------------------------------------------------------------------------------------------------------------
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);
        TEST_ERROR(cmdStanzaCreate(), PathNotEmptyError, "archive directory not empty");
        TEST_RESULT_LOG(
            "P00   WARN: option --force is no longer supported\n"
            "P00   INFO: stanza-create for stanza 'db' on repo1");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgValidate(), online=y"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH_REPO);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pgControl and database match");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH_PG, false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - db online");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");
        TEST_STORAGE_LIST(
            storageTest, TEST_PATH_REPO,
            "archive/\n"
            "archive/db/\n"
            "archive/db/archive.info\n"
            "archive/db/archive.info.copy\n"
            "backup/\n"
            "backup/db/\n"
            "backup/db/backup.info\n"
            "backup/db/backup.info.copy\n",
            .comment = "stanza created");

        HRN_CFG_LOAD(cfgCmdStanzaUpgrade, argList);
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH_PG, false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - db online");
        TEST_RESULT_LOG(
            "P00   INFO: stanza-upgrade for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' on repo1 is already up to date");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg_control and version mismatch");

        // Create pg_control with different version
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_91, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH_PG, false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            pgValidate(), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH_PG "' queried from cluster do not match version '" PG_VERSION_91_STR "' and "
                "'" TEST_PATH_PG "' read from '" TEST_PATH_PG "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg_control and path mismatch");

        // Create pg_control
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg2", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            pgValidate(), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH "/pg2' queried from cluster do not match version '"
                PG_VERSION_92_STR "' and '" TEST_PATH_PG "' read from '" TEST_PATH_PG "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL
            "'\nHINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary at pg2");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH_REPO);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, TEST_PATH_PG "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5434");

        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control for primary
        HRN_STORAGE_PUT(
            storagePgIdxWrite(1), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_92, .systemId = 6569239123849665699}));

        // Create pg_control for standby
        HRN_STORAGE_PUT(
            storagePgIdxWrite(0), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_94, .systemId = 6569239123849665700}));

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH_PG, true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(2, "dbname='postgres' port=5434", PG_VERSION_92, TEST_PATH "/pg1", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        PgControl pgControl = {0};
        TEST_ASSIGN(pgControl, pgValidate(), "validate primary on pg2");
        TEST_RESULT_UINT(pgControl.version, PG_VERSION_92, "    version set");
        TEST_RESULT_UINT(pgControl.systemId, 6569239123849665699, "    systemId set");
        TEST_RESULT_UINT(pgControl.catalogVersion, 201204301, "    catalogVersion set");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaUpgrade()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade errors");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaUpgrade, argList), OptionInvalidError,
            "option 'repo' not valid for command 'stanza-upgrade'");

        HRN_CFG_LOAD(cfgCmdStanzaUpgrade, argListBase);

        // Create the stop file, test and remove
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));
        TEST_ERROR_FMT(cmdStanzaUpgrade(), StopError, "stop file exists for stanza %s", strZ(stanza));
        HRN_STORAGE_REMOVE(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control for the rest of the tests
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("cmdStanzaUpgrade - info file mismatches");

        // Stanza with only archive.info and backup.info but no .copy files
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.6\"}\n");

        // backup info up to date but archive info db-id mismatch
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n");

        TEST_ERROR(
            cmdStanzaUpgrade(), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 2, version = 9.6, system-id = 6569239123849665679\n"
            "backup : id = 1, version = 9.6, system-id = 6569239123849665679\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        // backup info up to date but archive info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
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
                "\"db-version\":\"9.6\"}\n");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - version");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-id=2\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.5\"}\n"
            "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n",
            .comment = "put archive info contents to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza archive info files are equal");

        // archive info up to date but backup info version is not
        //--------------------------------------------------------------------------------------------------------------------------
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.5\"}\n");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - version");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
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
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info contents to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza backup info files are equal");

// CSHANG STOPPED HERE
        // // backup info up to date but archive info system-id is not
        // //--------------------------------------------------------------------------------------------------------------------------
        // contentBackup = STRDEF
        // (
        //     "[db]\n"
        //     "db-catalog-version=201608131\n"
        //     "db-control-version=960\n"
        //     "db-id=2\n"
        //     "db-system-id=6569239123849665679\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665999,"
        //         "\"db-version\":\"9.5\"}\n"
        //     "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
        //         "\"db-version\":\"9.6\"}\n"
        // );
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
        //         "put backup info to file");
        //
        // contentArchive = STRDEF
        // (
        //     "[db]\n"
        //     "db-id=1\n"
        //     "db-system-id=6569239123849665999\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        // );
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageTest, archiveInfoFileName), harnessInfoChecksum(contentArchive)),
        //         "put archive info to file");
        //
        // TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - system-id");
        // TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");
        // contentArchive = STRDEF
        // (
        //     "[db]\n"
        //     "db-id=2\n"
        //     "db-system-id=6569239123849665679\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
        //     "2={\"db-id\":6569239123849665679,\"db-version\":\"9.6\"}\n"
        // );
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentArchive)),
        //         "    put archive info to test file");
        // TEST_RESULT_BOOL(
        //     (bufEq(
        //         storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
        //         storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(archiveInfoFileName))))) &&
        //     bufEq(
        //         storageGetP(storageNewReadP(storageTest, archiveInfoFileName)),
        //         storageGetP(storageNewReadP(storageTest, fileName)))),
        //     true, "    test and stanza archive info files are equal");
        //
        // // archive info up to date but backup info system-id is not
        // //--------------------------------------------------------------------------------------------------------------------------
        // contentBackup = STRDEF
        // (
        //     "[db]\n"
        //     "db-catalog-version=201608131\n"
        //     "db-control-version=960\n"
        //     "db-id=1\n"
        //     "db-system-id=6569239123849665999\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
        //         "\"db-version\":\"9.6\"}\n"
        // );
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageTest, backupInfoFileName), harnessInfoChecksum(contentBackup)),
        //         "put backup info to file");
        //
        // TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - system-id");
        // TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");
        // contentBackup = STRDEF
        // (
        //     "[db]\n"
        //     "db-catalog-version=201608131\n"
        //     "db-control-version=960\n"
        //     "db-id=2\n"
        //     "db-system-id=6569239123849665679\n"
        //     "db-version=\"9.6\"\n"
        //     "\n"
        //     "[db:history]\n"
        //     "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
        //         "\"db-version\":\"9.6\"}\n"
        //     "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665679,"
        //         "\"db-version\":\"9.6\"}\n"
        // );
        // TEST_RESULT_VOID(
        //     storagePutP(
        //         storageNewWriteP(storageTest, fileName), harnessInfoChecksum(contentBackup)),
        //         "    put backup info to test file");
        //
        // TEST_RESULT_BOOL(
        //     (bufEq(
        //         storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
        //         storageGetP(storageNewReadP(storageTest,  strNewFmt("%s" INFO_COPY_EXT, strZ(backupInfoFileName))))) &&
        //     bufEq(
        //         storageGetP(storageNewReadP(storageTest, backupInfoFileName)),
        //         storageGetP(storageNewReadP(storageTest, fileName)))),
        //     true, "    test and stanza backup info files are equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaDelete(), stanzaDelete(), manifestDelete()"))
    {
        // Load Parameters
        StringList *argListCmd = strLstNew();
        strLstAddZ(argListCmd, "--repo1-path=" TEST_PATH "/repo");

        //--------------------------------------------------------------------------------------------------------------------------
        const String *stanzaOther = STRDEF("otherstanza");

        // Load Parameters
        StringList *argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanzaOther)));
        strLstAdd(argList, strNewFmt("--pg1-path=" TEST_PATH "/%s", strZ(stanzaOther)));
        strLstAddZ(argList, "--no-online");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control for stanza-create
        storagePutP(
            storageNewWriteP(storageTest, strNewFmt("%s/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, strZ(stanzaOther))),
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 6569239123849665679}));

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza that will not be deleted");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'otherstanza' on repo1");

        argList = strLstDup(argListCmd);
        strLstAdd(argList, strNewFmt("--stanza=%s", strZ(stanza)));
        strLstAdd(argList, strNewFmt("--pg1-path=" TEST_PATH "/%s", strZ(stanza)));
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList);

        // stanza already deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - success on stanza does not exist");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, NULL), true, "    archiveList=NULL, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), NULL), true, "    archiveList=0, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, strLstNew()), true, "    archiveList=NULL, backupList=0");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), strLstNew()), true, "    archiveList=0, backupList=0");

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
            "unable to remove '" TEST_PATH "/repo/backup/%s/20190708-154306F/backup.manifest': [20] Not a directory", strZ(stanza));
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
        TEST_ERROR(
            cmdStanzaDelete(), PgRunningError, PG_FILE_POSTMASTERPID " exists - looks like " PG_NAME " is running. "
            "To delete stanza 'db' on repo1, shut down " PG_NAME " for stanza 'db' and try again, or use --force.");

        // Specify repo option
        StringList *argListDel = strLstDup(argList);
        hrnCfgArgKeyRawZ(argListDel, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argListDel, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argListDel);

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageTest, strNewFmt("repo2/backup/%s/backup.info", strZ(stanza))), BUFSTRDEF("")),
                "create backup.info");
        TEST_ERROR(
            cmdStanzaDelete(), PgRunningError, PG_FILE_POSTMASTERPID " exists - looks like " PG_NAME " is running. "
            "To delete stanza 'db' on repo2, shut down " PG_NAME " for stanza 'db' and try again, or use --force.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("force delete when pg appears to be running, multi-repo");

        argList = strLstDup(argListCmd);
        hrnCfgArgRaw(argList, cfgOptStanza, stanza);
        hrnCfgArgKeyRawFmt(argList, cfgOptPgPath, 1, TEST_PATH "/%s", strZ(stanza));
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        strLstAddZ(argList,"--force");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList);

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete --force");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo/backup/%s", strZ(stanza))), false, "repo1: stanza deleted");
        TEST_RESULT_BOOL(
            storagePathExistsP(storageTest, strNewFmt("repo2/backup/%s", strZ(stanza))), true, "repo2: stanza not deleted");

        // Ensure other stanza never deleted
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("repo/archive/%s/archive.info", strZ(stanzaOther))), true, "otherstanza exists");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
