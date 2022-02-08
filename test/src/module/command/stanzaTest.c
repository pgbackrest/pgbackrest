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
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create storage objects for the testing location and for the harness directory for items such as logs, stop files, etc
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
    Storage *storageHrn = storagePosixNewP(HRN_PATH_STR, .write = true);

    #define TEST_STANZA                                             "db"
    #define TEST_STANZA_OTHER                                       "otherstanza"

    StringList *argListBase = strLstNew();
    hrnCfgArgRawBool(argListBase, cfgOptOnline, false);
    hrnCfgArgRawZ(argListBase, cfgOptStanza, TEST_STANZA);
    hrnCfgArgRawZ(argListBase, cfgOptPgPath, TEST_PATH "/pg");
    hrnCfgArgRawZ(argListBase, cfgOptRepoPath, TEST_PATH "/repo");

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaCreate(), checkStanzaInfo(), cmdStanzaDelete()"))
    {
        TEST_TITLE("stanza-create: repo option not valid");

        // Load Parameters
        StringList *argList = strLstDup(argListBase);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");

        TEST_ERROR(
            hrnCfgLoadP(cfgCmdStanzaCreate, argList), OptionInvalidError, "option 'repo' not valid for command 'stanza-create'");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-create: stop file error");

        HRN_CFG_LOAD(cfgCmdStanzaCreate, argListBase);

        // Create the stop file, test, then remove the stop file
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));
        TEST_ERROR(cmdStanzaCreate(), StopError, "stop file exists for stanza db");
        HRN_STORAGE_REMOVE(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-create: validate info files created");

        argList = strLstDup(argListBase);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - one repo, no files exist");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.6\"}\n",
            .comment = "put archive info to test file");

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
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info to test file");

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
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 1, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 3, TEST_PATH "/repo3");
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 4, TEST_PATH "/repo4");
        hrnCfgArgRawZ(argListCmd, cfgOptStanza, TEST_STANZA);
        hrnCfgArgRawZ(argListCmd, cfgOptPgPath, TEST_PATH "/pg");

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
        HRN_STORAGE_MOVE(storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE, INFO_BACKUP_PATH_FILE INFO_COPY_EXT);
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
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.6\"}\n",
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
        HRN_STORAGE_MOVE(storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE, INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT);
        TEST_ERROR(
            cmdStanzaCreate(), FileMissingError,
            "archive.info exists but backup.info is missing on repo1\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("checkStanzaInfo() - already checked in checkTest so just a sanity check here");

        // Create a corrupted backup file - db id
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info to file - bad db-id");

        TEST_ERROR(
            cmdStanzaCreate(), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 1, version = 9.6, system-id = " HRN_PG_SYSTEMID_96_Z "\n"
            "backup : id = 2, version = 9.6, system-id = " HRN_PG_SYSTEMID_96_Z "\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info file and backup.info files that match but do not match the current database version");

        // Copy files may or may not exist - remove
        HRN_STORAGE_REMOVE(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT, .comment = "repo1: remove archive.info.copy");
        HRN_STORAGE_REMOVE(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE INFO_COPY_EXT, .comment = "repo1: remove backup.info.copy");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201510051\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.5\"}\n");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.5\"}\n");

        TEST_ERROR(
            cmdStanzaCreate(), FileInvalidError,
            "backup and archive info files exist but do not match the database\n"
            "HINT: is this the correct stanza?\n"
            "HINT: did an error occur during stanza-upgrade?");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archive.info and backup.info files that match but do not match the current database system-id");

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

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repeat last test using --force (deprecated)");

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
        hrnCfgArgRawZ(argList, cfgOptStanza, TEST_STANZA);
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pgControl and database match");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_92);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaCreate(), "stanza create - db online");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'db' on repo1");
        TEST_STORAGE_LIST(
            storageTest, TEST_PATH "/repo",
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
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - db online");
        TEST_RESULT_LOG(
            "P00   INFO: stanza-upgrade for stanza 'db' on repo1\n"
            "P00   INFO: stanza 'db' on repo1 is already up to date");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg_control and version mismatch");

        // Create pg_control with different version
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_91);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            pgValidate(), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH "/pg' queried from cluster do not match version '"
            PG_VERSION_91_STR "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "'\n"
            "HINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg_control and path mismatch");

        // Create pg_control
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_92);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg2", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(
            pgValidate(), DbMismatchError,
            "version '" PG_VERSION_92_STR "' and path '" TEST_PATH "/pg2' queried from cluster do not match version '"
                PG_VERSION_92_STR "' and '" TEST_PATH "/pg' read from '" TEST_PATH "/pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL
            "'\nHINT: the pg1-path and pg1-port settings likely reference different clusters.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary at pg2");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, TEST_STANZA);
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5434");

        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control for primary
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_92);

        // Create pg_control for standby
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_94);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, TEST_PATH "/pg", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(2, "dbname='postgres' port=5434", PG_VERSION_92, TEST_PATH "/pg1", false, NULL, NULL),
            HRNPQ_MACRO_DONE()
        });

        PgControl pgControl = {0};
        TEST_ASSIGN(pgControl, pgValidate(), "validate primary on pg2");
        TEST_RESULT_UINT(pgControl.version, PG_VERSION_92, "version set");
        TEST_RESULT_UINT(pgControl.systemId, HRN_PG_SYSTEMID_92, "systemId set");
        TEST_RESULT_UINT(pgControl.catalogVersion, 201204301, "catalogVersion set");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaUpgrade()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - config errors");

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
        TEST_ERROR(cmdStanzaUpgrade(), StopError, "stop file exists for stanza db");
        HRN_STORAGE_REMOVE(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))));

        //--------------------------------------------------------------------------------------------------------------------------
        // Create pg_control for the rest of the tests
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - info file mismatch: db-id");

        // Stanza with only archive.info and backup.info but no .copy files
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n");

        // backup info up to date but archive info db-id mismatch
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.6\"}\n");

        TEST_ERROR(
            cmdStanzaUpgrade(), FileInvalidError,
            "backup info file and archive info file do not match\n"
            "archive: id = 2, version = 9.6, system-id = " HRN_PG_SYSTEMID_96_Z "\n"
            "backup : id = 1, version = 9.6, system-id = " HRN_PG_SYSTEMID_96_Z "\n"
            "HINT: this may be a symptom of repository corruption!");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - info file mismatch: archive version");

        // backup info up to date but archive info version is not
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.5\"}\n");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - version");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.5\"}\n"
            "2={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.6\"}\n",
            .comment = "put archive info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza archive info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - info file mismatch: backup version");

        // archive info up to date but backup info version is not
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=1\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.5\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.5\"}\n");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - version");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza backup info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - info file mismatch: archive system-id");

        // backup info up to date but archive info system-id is not
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_BACKUP_PATH_FILE,
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.5\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n");
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665999\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n");

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - archive.info file upgraded - system-id");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665999,\"db-version\":\"9.6\"}\n"
            "2={\"db-id\":" HRN_PG_SYSTEMID_96_Z ",\"db-version\":\"9.6\"}\n",
            .comment = "put archive info to test file");

        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_ARCHIVE_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_ARCHIVE_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza archive info files are equal");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-upgrade - info file mismatch: backup system-id");

        // archive info up to date but backup info system-id is not
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

        TEST_RESULT_VOID(cmdStanzaUpgrade(), "stanza upgrade - backup.info file upgraded - system-id");
        TEST_RESULT_LOG("P00   INFO: stanza-upgrade for stanza 'db' on repo1");

        HRN_INFO_PUT(
            storageHrn, "test.info",
            "[db]\n"
            "db-catalog-version=201608131\n"
            "db-control-version=960\n"
            "db-id=2\n"
            "db-system-id=" HRN_PG_SYSTEMID_96_Z "\n"
            "db-version=\"9.6\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":6569239123849665999,"
                "\"db-version\":\"9.6\"}\n"
            "2={\"db-catalog-version\":201608131,\"db-control-version\":960,\"db-system-id\":" HRN_PG_SYSTEMID_96_Z ","
                "\"db-version\":\"9.6\"}\n",
            .comment = "put backup info to test file");
        TEST_RESULT_BOOL(
            (bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageRepoIdx(0), STRDEF(INFO_BACKUP_PATH_FILE INFO_COPY_EXT)))) &&
            bufEq(
                storageGetP(storageNewReadP(storageRepoIdx(0), INFO_BACKUP_PATH_FILE_STR)),
                storageGetP(storageNewReadP(storageHrn, STRDEF("test.info"))))),
            true, "test and stanza backup info files are equal");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStanzaDelete(), stanzaDelete()"))
    {
        // Load Parameters
        StringList *argListCmd = strLstNew();
        hrnCfgArgKeyRawZ(argListCmd, cfgOptRepoPath, 1, TEST_PATH "/repo");

        // Load Parameters
        StringList *argList = strLstDup(argListCmd);
        hrnCfgArgRawZ(argList, cfgOptStanza, TEST_STANZA_OTHER);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/" TEST_STANZA_OTHER);
        hrnCfgArgRawBool(argList, cfgOptOnline, false);
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList);

        // Create pg_control for stanza-create
        HRN_PG_CONTROL_PUT(storagePgWrite(), PG_VERSION_96);

        TEST_RESULT_VOID(cmdStanzaCreate(), "create a stanza that will not be deleted");
        TEST_RESULT_LOG("P00   INFO: stanza-create for stanza 'otherstanza' on repo1");

        argList = strLstDup(argListCmd);
        hrnCfgArgRawZ(argList, cfgOptStanza, TEST_STANZA);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList);

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-delete - stanza already deleted");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - success on stanza does not exist");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, NULL), true, "archiveList=NULL, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), NULL), true, "archiveList=0, backupList=NULL");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), NULL, strLstNew()), true, "archiveList=NULL, backupList=0");
        TEST_RESULT_BOOL(stanzaDelete(storageRepoWrite(), strLstNew(), strLstNew()), true, "archiveList=0, backupList=0");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-delete - only archive exists");

        // Confirm stanza does not exist
        TEST_STORAGE_LIST(
            storageTest, "repo/archive", "otherstanza/\n", .noRecurse=true,
            .comment = "stanza '" TEST_STANZA "' archive does not exist");
        TEST_STORAGE_LIST(
            storageTest, "repo/backup", "otherstanza/\n", .noRecurse=true,
            .comment = "stanza '" TEST_STANZA "' backup does not exist");

        // Create stanza archive only
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE, .comment = "create empty archive info for stanza '" TEST_STANZA "'");
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))), .comment = "create stop file");
        TEST_STORAGE_LIST(
            storageTest, "repo/archive",
            "db/\n"
            "otherstanza/\n",
            .noRecurse = true, .comment = "stanza archive exists");
        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - archive only");

        TEST_STORAGE_LIST(
            storageTest, "repo/archive", "otherstanza/\n", .noRecurse=true, .comment = "stanza '" TEST_STANZA "' deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-delete - only backup exists");

        // Create stanza backup only
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE,
            .comment = "create empty backup info for stanza '" TEST_STANZA "'");
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))), .comment = "create stop file");
        TEST_STORAGE_LIST(
            storageTest, "repo/backup",
            "db/\n"
            "otherstanza/\n",
            .noRecurse = true, .comment = "stanza backup exists");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - backup only");
        TEST_STORAGE_LIST(
            storageTest, "repo/backup", "otherstanza/\n", .noRecurse=true, .comment = "stanza '" TEST_STANZA "' deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stanza-delete - empty directories");

        TEST_RESULT_VOID(cmdStanzaDelete(), "remove stanza '" TEST_STANZA "'");

        // Create only stanza paths
        HRN_STORAGE_PATH_CREATE(storageTest, "repo/archive/" TEST_STANZA, .comment = "create empty stanza archive path");
        HRN_STORAGE_PATH_CREATE(storageTest, "repo/backup/" TEST_STANZA, .comment = "create empty stanza backup path");

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza delete - empty directories");
        TEST_STORAGE_LIST(
            storageTest, "repo/archive", "otherstanza/\n", .noRecurse=true, .comment = "stanza '" TEST_STANZA "' deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delete errors when pg appears to be running");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), INFO_BACKUP_PATH_FILE, .comment = "create empty backup info for stanza '" TEST_STANZA "'");
        HRN_STORAGE_PUT_EMPTY(storageHrn, strZ(lockStopFileName(cfgOptionStr(cfgOptStanza))), .comment = "create stop file");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), PG_FILE_POSTMTRPID, .comment = "create " PG_FILE_POSTMTRPID " file");

        TEST_ERROR(
            cmdStanzaDelete(), PgRunningError, PG_FILE_POSTMTRPID " exists - looks like " PG_NAME " is running. "
            "To delete stanza 'db' on repo1, shut down " PG_NAME " for stanza 'db' and try again, or use --force.");

        // Specify repo option
        StringList *argListDel = strLstDup(argList);
        hrnCfgArgKeyRawZ(argListDel, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argListDel, cfgOptRepo, "2");
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argListDel);

        HRN_STORAGE_PUT_EMPTY(storageRepoIdxWrite(1), INFO_BACKUP_PATH_FILE, .comment = "create empty backup info repo2");

        TEST_ERROR(
            cmdStanzaDelete(), PgRunningError, PG_FILE_POSTMTRPID " exists - looks like " PG_NAME " is running. "
            "To delete stanza 'db' on repo2, shut down " PG_NAME " for stanza 'db' and try again, or use --force.");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("force delete when pg appears to be running, multi-repo");

        argList = strLstDup(argListCmd);
        hrnCfgArgRawZ(argList, cfgOptStanza, TEST_STANZA);
        hrnCfgArgKeyRawFmt(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawBool(argList, cfgOptForce, true);
        HRN_CFG_LOAD(cfgCmdStanzaDelete, argList);

        TEST_RESULT_VOID(cmdStanzaDelete(), "stanza '" TEST_STANZA "' delete --force");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewZ("repo/backup/" TEST_STANZA)), false, "repo1: stanza deleted");
        TEST_RESULT_BOOL(storagePathExistsP(storageTest, strNewZ("repo2/backup/" TEST_STANZA)), true, "repo2: stanza not deleted");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ensure other stanza never deleted from repo1");

        TEST_STORAGE_LIST(
            storageRepo(), "",
            "archive/\n"
            "archive/otherstanza/\n"
            "archive/otherstanza/archive.info\n"
            "archive/otherstanza/archive.info.copy\n"
            "backup/\n"
            "backup/otherstanza/\n"
            "backup/otherstanza/backup.info\n"
            "backup/otherstanza/backup.info.copy\n",
            .comment = "otherstanza exists");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
