/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/time.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessProtocol.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("archivePushReadyList(), archivePushProcessList(), and archivePushDrop()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/db");
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync);

        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "pg_wal/archive_status");
        HRN_STORAGE_PATH_CREATE(storageTest, "spool/archive/db/out");

        // Create ok files to indicate WAL that has already been archived
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok");
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000003.ok");
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000004.ok");
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000005.error");
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000006.error");
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/global.error");

        // Create ready files for wal that still needs to be archived
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000002.ready");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000003.ready");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000005.ready");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000006.ready");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ready list");

        TEST_RESULT_STRLST_Z(
            archivePushProcessList(STRDEF(TEST_PATH "/db/pg_wal")),
            "000000010000000100000002\n000000010000000100000005\n000000010000000100000006\n", "ready list");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT, "000000010000000100000003.ok\n", .comment = "remaining status list");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL drop");

        StringList *argListDrop = strLstDup(argList);
        hrnCfgArgRawFmt(argListDrop, cfgOptArchivePushQueueMax, "%zu", (size_t)1024 * 1024 * 1024);
        HRN_CFG_LOAD(cfgCmdArchivePush, argListDrop, .role = cfgCmdRoleAsync);

        // Write the files that we claim are in pg_wal
        Buffer *walBuffer = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer, bufSize(walBuffer));
        memset(bufPtr(walBuffer), 0, bufSize(walBuffer));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}, walBuffer);

        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000002", walBuffer);
        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000003", walBuffer);
        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000005", walBuffer);
        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000006", walBuffer);

        // Queue max is high enough that no WAL will be dropped
        TEST_RESULT_BOOL(
            archivePushDrop(STRDEF("pg_wal"), archivePushProcessList(STRDEF(TEST_PATH "/db/pg_wal"))), false, "wal is not dropped");

        // Now set queue max low enough that WAL will be dropped
        argListDrop = strLstDup(argList);
        hrnCfgArgRawFmt(argListDrop, cfgOptArchivePushQueueMax, "%zu", (size_t)16 * 1024 * 1024 * 2);
        HRN_CFG_LOAD(cfgCmdArchivePush, argListDrop, .role = cfgCmdRoleAsync);

        TEST_RESULT_BOOL(
            archivePushDrop(STRDEF("pg_wal"), archivePushProcessList(STRDEF(TEST_PATH "/db/pg_wal"))), true, "wal is dropped");
    }

    // *****************************************************************************************************************************
    if (testBegin("archivePushCheck()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mismatched pg_control and archive.info - pg version");

        HRN_STORAGE_PUT(
            storageTest, "pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_96, .systemId = 0xFACEFACEFACEFACE}));

        // Create incorrect archive info
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n");

        TEST_ERROR(
            archivePushCheck(true), RepoInvalidError,
            "unable to find a valid repository:\n"
            "repo1: [ArchiveMismatchError] PostgreSQL version 9.6, system-id 18072658121562454734 do not match repo1 stanza version"
                " 9.4, system-id 5555555555555555555"
                "\nHINT: are you archiving to the correct stanza?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mismatched pg_control and archive.info - system-id");

        // Fix the version
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":5555555555555555555,\"db-version\":\"9.6\"}\n");

        TEST_ERROR(
            archivePushCheck(true), RepoInvalidError,
            "unable to find a valid repository:\n"
            "repo1: [ArchiveMismatchError] PostgreSQL version 9.6, system-id 18072658121562454734 do not match repo1 stanza version"
                " 9.6, system-id 5555555555555555555"
                "\nHINT: are you archiving to the correct stanza?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg_control and archive.info match");

        // Fix archive info
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n");

        ArchivePushCheckResult result = {0};
        TEST_ASSIGN(result, archivePushCheck(true), "get archive check result");

        TEST_RESULT_UINT(result.pgVersion, PG_VERSION_96, "check pg version");
        TEST_RESULT_UINT(result.pgSystemId, 0xFACEFACEFACEFACE, "check pg system id");

        ArchivePushFileRepoData *repoData = lstGet(result.repoList, 0);
        TEST_RESULT_UINT(repoData->repoIdx, 0, "check repo idx");
        TEST_RESULT_STR_Z(repoData->archiveId, "9.6-1", "check archive id");
        TEST_RESULT_UINT(repoData->cipherType, cipherTypeNone, "check cipher type");
        TEST_RESULT_STR_Z(repoData->cipherPass, NULL, "check cipher pass (not set in this test)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("mismatched repos when pg-path not present");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 4, TEST_PATH "/repo4");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        // repo2 has correct info
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n");

        // repo4 has incorrect info
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n");

        TEST_ASSIGN(result, archivePushCheck(false), "get archive check result");

        TEST_RESULT_UINT(result.pgVersion, PG_VERSION_96, "check pg version");
        TEST_RESULT_UINT(result.pgSystemId, 0xFACEFACEFACEFACE, "check pg system id");
        TEST_RESULT_STRLST_Z(
            result.errorList,
            "repo4: [ArchiveMismatchError] repo2 stanza version 9.6, system-id 18072658121562454734 do not match repo4 stanza"
                " version 9.4, system-id 5555555555555555555\n"
            "HINT: are you archiving to the correct stanza?\n",
            "check error list");

        repoData = lstGet(result.repoList, 0);
        TEST_RESULT_UINT(repoData->repoIdx, 0, "check repo idx");
        TEST_RESULT_STR_Z(repoData->archiveId, "9.6-1", "check archive id");
        TEST_RESULT_UINT(repoData->cipherType, cipherTypeNone, "check cipher type");
        TEST_RESULT_STR_Z(repoData->cipherPass, NULL, "check cipher pass (not set in this test)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("matched repos when pg-path not present");

        // repo4 has correct info
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n");

        TEST_ASSIGN(result, archivePushCheck(false), "get archive check result");

        TEST_RESULT_UINT(result.pgVersion, PG_VERSION_96, "check pg version");
        TEST_RESULT_UINT(result.pgSystemId, 0xFACEFACEFACEFACE, "check pg system id");

        repoData = lstGet(result.repoList, 0);
        TEST_RESULT_UINT(repoData->repoIdx, 0, "check repo idx");
        TEST_RESULT_STR_Z(repoData->archiveId, "9.6-1", "check repo2 archive id");
        TEST_RESULT_UINT(repoData->cipherType, cipherTypeNone, "check repo2 cipher pass");
        TEST_RESULT_STR_Z(repoData->cipherPass, NULL, "check repo2 cipher pass (not set in this test)");

        repoData = lstGet(result.repoList, 1);
        TEST_RESULT_UINT(repoData->repoIdx, 1, "check repo idx");
        TEST_RESULT_STR_Z(repoData->archiveId, "9.6-2", "check repo4 archive id");
        TEST_RESULT_UINT(repoData->cipherType, cipherTypeNone, "check repo4 cipher type");
        TEST_RESULT_STR_Z(repoData->cipherPass, NULL, "check repo4 cipher pass (not set in this test)");
    }

    // *****************************************************************************************************************************
    if (testBegin("Synchronous cmdArchivePush() and archivePushFile()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test2");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleMain);

        TEST_ERROR(cmdArchivePush(), HostInvalidError, "archive-push command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL segment not specified");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_ERROR(cmdArchivePush(), ParamRequiredError, "WAL segment to push required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg-path not specified");

        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        TEST_ERROR(
            cmdArchivePush(), OptionRequiredError,
            "option 'pg1-path' must be specified when relative wal paths are used"
            "\nHINT: is %f passed to archive-push instead of %p?"
            "\nHINT: PostgreSQL may pass relative paths even with %p depending on the environment.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("attempt to push WAL with incorrect headers");

        // Create pg_control and archive.info
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        HRN_STORAGE_PUT(
            storageTest, "pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}));

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}\n");

        // Generate WAL with incorrect headers and try to push them
        Buffer *walBuffer1 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer1, bufSize(walBuffer1));
        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}, walBuffer1);

        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000001", walBuffer1);

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_ERROR(
            cmdArchivePush(), ArchiveMismatchError,
            "WAL file '" TEST_PATH "/pg/pg_wal/000000010000000100000001' version 10, system-id 18072658121562454734 do not match"
                " stanza version 11, system-id 18072658121562454734");

        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xECAFECAFECAFECAF}, walBuffer1);
        const char *walBuffer1Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer1)));

        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000001", walBuffer1);

        TEST_ERROR(
            cmdArchivePush(), ArchiveMismatchError,
            "WAL file '" TEST_PATH "/pg/pg_wal/000000010000000100000001' version 11, system-id 17055110554209741999 do not match"
                " stanza version 11, system-id 18072658121562454734");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push by ignoring the invalid header");

        argListTemp = strLstDup(argList);
        hrnCfgArgRawBool(argListTemp, cfgOptArchiveHeaderCheck, false);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '000000010000000100000001' to the archive");

        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-1/000000010000000100000001-%s.gz", walBuffer1Sha1)),
            .remove = true, .comment = "check repo for WAL file, then remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("generate valid WAL and push them");

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        memset(bufPtr(walBuffer1), 0, bufSize(walBuffer1));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}, walBuffer1);

        // Check sha1 checksum against fixed values once to make sure they are not getting munged. After this we'll calculate them
        // directly from the buffers to reduce the cost of maintaining checksums.
        walBuffer1Sha1 = TEST_64BIT() ?
            (TEST_BIG_ENDIAN() ? "1c5f963d720bb199d7935dbd315447ea2ec3feb2" : "aae7591a1dbc58f21d0d004886075094f622e6dd") :
            "28a13fd8cf6fcd9f9a8108aed4c8bcc58040863a";

        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000001", walBuffer1);

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '000000010000000100000001' to the archive");

        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0), strZ(strNewFmt(STORAGE_REPO_ARCHIVE "/11-1/000000010000000100000001-%s.gz", walBuffer1Sha1)),
            .comment = "check repo for WAL file");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment again");
        TEST_RESULT_LOG(
            "P00   WARN: WAL file '000000010000000100000001' already exists in the repo1 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000001' to the archive");

        // Now create a new WAL buffer with a different checksum to test checksum errors
        Buffer *walBuffer2 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer2, bufSize(walBuffer2));
        memset(bufPtr(walBuffer2), 0xFF, bufSize(walBuffer2));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}, walBuffer2);
        const char *walBuffer2Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer2)));

        HRN_STORAGE_PUT(storagePgWrite(), "pg_wal/000000010000000100000001", walBuffer2);

        TEST_ERROR(
            cmdArchivePush(), ArchiveDuplicateError,
            "WAL file '000000010000000100000001' already exists in the repo1 archive with a different checksum");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL with absolute path and no pg1-path");

        argListTemp = strLstNew();
        hrnCfgArgRawZ(argListTemp, cfgOptStanza, "test");
        hrnCfgArgRawZ(argListTemp, cfgOptRepoPath, TEST_PATH "/repo");
        strLstAddZ(argListTemp, TEST_PATH "/pg/pg_wal/000000010000000100000002");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        HRN_STORAGE_PUT(storageTest, "pg/pg_wal/000000010000000100000002", walBuffer2, .comment = "write WAL");

        // Create tmp file to make it look like a prior push failed partway through to ensure that retries work
        HRN_STORAGE_PUT_Z(
            storageTest,
            strZ(
                strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s.gz.pgbackrest.tmp", walBuffer2Sha1)),
            "PARTIAL", .comment = "write WAL tmp file");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(0),
            strZ(
                strNewFmt(
                    STORAGE_REPO_ARCHIVE "/11-1/0000000100000001/000000010000000100000002-%s.gz", walBuffer2Sha1)),
            .comment = "check repo for WAL file");
        TEST_RESULT_BOOL(
            storageExistsP(
                storageTest,
                strNewFmt("repo/archive/test/11-1/0000000100000001/000000010000000100000002-%s.gz.pgbackrest.tmp", walBuffer2Sha1)),
            false, "check WAL tmp file is gone");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push a history file");

        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "pg_wal/00000001.history");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        HRN_STORAGE_PUT_Z(storagePgWrite(), "pg_wal/00000001.history", "FAKEHISTORY");

        TEST_RESULT_VOID(cmdArchivePush(), "push a history file");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '00000001.history' to the archive");

        TEST_STORAGE_EXISTS(
            storageRepoIdx(0), STORAGE_REPO_ARCHIVE "/11-1/00000001.history", .comment = "check repo for history file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check drop functionality");

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000001.ready");
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_wal/archive_status/000000010000000100000002.ready");

        argListTemp = strLstDup(argList);
        hrnCfgArgRawZ(argListTemp, cfgOptArchivePushQueueMax, "16m");
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(cmdArchivePush(), "drop WAL file");
        TEST_RESULT_LOG("P00   WARN: dropped WAL file '000000010000000100000002' because archive queue exceeded 16MB");

        argListTemp = strLstDup(argList);
        hrnCfgArgRawZ(argListTemp, cfgOptArchivePushQueueMax, "1GB");
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        TEST_RESULT_VOID(cmdArchivePush(), "push WAL file again");
        TEST_RESULT_LOG(
            "P00   WARN: WAL file '000000010000000100000002' already exists in the repo1 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple repos, one encrypted");

        // Remove old repo
        HRN_STORAGE_PATH_REMOVE(storageTest, "repo", .errorOnMissing = true, .recurse = true);

        argListTemp = strLstNew();
        hrnCfgArgRawZ(argListTemp, cfgOptStanza, "test");
        hrnCfgArgKeyRawZ(argListTemp, cfgOptPgPath, 1, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argListTemp, cfgOptRepoPath, 2, TEST_PATH "/repo2");
        hrnCfgArgKeyRawStrId(argListTemp, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, "badpassphrase");
        hrnCfgArgKeyRawZ(argListTemp, cfgOptRepoPath, 3, TEST_PATH "/repo3");
        hrnCfgArgRawNegate(argListTemp, cfgOptCompress);
        strLstAddZ(argListTemp, "pg_wal/000000010000000100000002");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        // repo2 is encrypted
        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[cipher]\n"
            "cipher-pass=\"badsubpassphrase\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}",
            .cipherType = cipherTypeAes256Cbc, .cipherPass = "badpassphrase");

        // repo3 is not encrypted
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}");

        // Push encrypted WAL segment
        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo2/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true, .comment = "check repo2 for WAL file then remove");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true, .comment = "check repo3 for WAL file then remove");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write error on one repo but other repo succeeds");

        HRN_STORAGE_MODE(storageTest, "repo2/archive/test/11-1/0000000100000001", .mode = 0500);

        TEST_ERROR(
            cmdArchivePush(), CommandError,
            strZ(
                strNewFmt(
                    "archive-push command encountered error(s):\n"
                    "repo2: [FileOpenError] unable to open file '" TEST_PATH "/repo2/archive/test/11-1/0000000100000001"
                        "/000000010000000100000002-%s' for write: [13] Permission denied", walBuffer2Sha1)));

        TEST_STORAGE_LIST_EMPTY(storageTest, "repo2/archive/test/11-1/0000000100000001", .comment = "check repo2 for no WAL file");
        TEST_STORAGE_LIST(
            storageTest, "repo3/archive/test/11-1/0000000100000001",
            strZ(strNewFmt("000000010000000100000002-%s\n", walBuffer2Sha1)), .comment = "check repo3 for WAL file");

        HRN_STORAGE_MODE(storageTest, "repo2/archive/test/11-1/0000000100000001");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push WAL to one repo");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG(
            "P00   WARN: WAL file '000000010000000100000002' already exists in the repo3 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        TEST_STORAGE_LIST(
            storageTest, "repo2/archive/test/11-1/0000000100000001",
            strZ(strNewFmt("000000010000000100000002-%s\n", walBuffer2Sha1)), .comment = "check repo2 for WAL file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL already exists in both repos");

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG(
            "P00   WARN: WAL file '000000010000000100000002' already exists in the repo2 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   WARN: WAL file '000000010000000100000002' already exists in the repo3 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P00   INFO: pushed WAL file '000000010000000100000002' to the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push succeeds on one repo when other repo fails to load archive.info");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo2/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true);
        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true);
        HRN_STORAGE_MODE(storageTest, "repo2", .mode = 0200);

        TEST_ERROR(
            cmdArchivePush(), CommandError,
            "archive-push command encountered error(s):\n"
            "repo2: [FileOpenError] unable to load info file '" TEST_PATH "/repo2/archive/test/archive.info' or"
                " '" TEST_PATH "/repo2/archive/test/archive.info.copy':\n"
            "FileOpenError: unable to open file '" TEST_PATH "/repo2/archive/test/archive.info' for read: [13] Permission denied\n"
            "FileOpenError: unable to open file '" TEST_PATH "/repo2/archive/test/archive.info.copy' for read:"
                " [13] Permission denied\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.");

        // Make sure WAL got pushed to repo3
        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true);

        HRN_STORAGE_MODE(storageTest, "repo2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push succeeds on one repo when other repo fails to read path");

        HRN_STORAGE_MODE(storageTest, "repo2/archive/test/11-1", .mode = 0200);

        TEST_ERROR(
            cmdArchivePush(), CommandError,
            "archive-push command encountered error(s):\n"
            "repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test/11-1/0000000100000001':"
                " [13] Permission denied");

        // Make sure WAL got pushed to repo3
        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/11-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .remove = true);

        HRN_STORAGE_MODE(storageTest, "repo2/archive/test/11-1");
    }

    // *****************************************************************************************************************************
    if (testBegin("Asynchronous cmdArchivePush() and cmdArchivePushAsync()"))
    {
        harnessLogLevelSet(logLevelDetail);

        // Install local command handler shim
        static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_ARCHIVE_PUSH_LIST};
        hrnProtocolLocalShimInstall(testLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testLocalHandlerList));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        strLstAddZ(argList, "--" CFGOPT_SPOOL_PATH "=/spool");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchivePush(), HostInvalidError, "archive-push command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1-path must be set when async");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, "/spool");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test2");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        strLstAddZ(argList, "/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchivePush(), OptionRequiredError, "'archive-push' command in async mode requires option 'pg1-path'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check timeout on async error");

        // Call with a bogus exe name so the async process will error out and we can make sure timeouts work
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "1");
        strLstAddZ(argList, "pg_wal/bogus");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .exeBogus = true);

        HRN_STORAGE_PATH_CREATE(storageTest, strZ(cfgOptionStr(cfgOptPgPath)));
        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL file 'bogus' to the archive asynchronously after 1 second(s)");

        // Create pg_control and archive.info for next set of tests
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawBool(argList, cfgOptCompress, false);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawBool(argList, cfgOptLogSubprocess, true);

        HRN_STORAGE_PUT(storageTest, "pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}));

        HRN_INFO_PUT(
            storageTest, "repo/archive/test/archive.info",
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":12297848147757817309,\"db-version\":\"9.4\"}\n");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("async, ignore error file on first pass");

        // Write out an error file that will be ignored on the first pass, then the async process will write a new one
        StringList *argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, TEST_PATH "/pg/pg_xlog/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "pg_xlog/archive_status");

        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error", "25\nbogus error");

        TEST_ERROR(cmdArchivePush(), AssertError, "no WAL files to process");

        TEST_STORAGE_EXISTS(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/global.error", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("with lock, prevent async from running");

        // Acquire a lock so the async process will not be able to run -- this will result in a timeout
        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, TEST_PATH "/pg/pg_xlog/000000010000000100000001");
        hrnCfgArgRawZ(argListTemp, cfgOptArchiveTimeout, "1");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                lockAcquire(
                    cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), STRDEF("555-fefefefe"), cfgLockType(), 30000, true);

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

                TEST_ERROR(
                    cmdArchivePush(), ArchiveTimeoutError,
                    "unable to push WAL file '000000010000000100000001' to the archive asynchronously after 1 second(s)");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("async WAL push");

        // Actually push a WAL file
        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, TEST_PATH "/pg/pg_xlog/000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp);

        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_xlog/archive_status/000000010000000100000001.ready");

        Buffer *walBuffer1 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer1, bufSize(walBuffer1));
        memset(bufPtr(walBuffer1), 0xFF, bufSize(walBuffer1));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}, walBuffer1);
        const char *walBuffer1Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer1)));

        HRN_STORAGE_PUT(storagePgWrite(),"pg_xlog/000000010000000100000001", walBuffer1);

        TEST_RESULT_VOID(cmdArchivePush(), "push the WAL segment");
        TEST_RESULT_LOG("P00   INFO: pushed WAL file '000000010000000100000001' to the archive asynchronously");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            .comment = "check repo for WAL file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("direct tests of the async function");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawBool(argList, cfgOptLogSubprocess, true);
        hrnCfgArgRawZ(argList, cfgOptCompressType, "none");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchivePushAsync(), ParamRequiredError, "WAL path to push required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("async, check that global.error is created");

        // Remove data from prior tests
        HRN_STORAGE_PATH_REMOVE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT, .recurse = true);
        HRN_STORAGE_PATH_CREATE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT);

        HRN_STORAGE_PATH_REMOVE(storagePgWrite(), "pg_xlog/archive_status", .recurse = true);
        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "pg_xlog/archive_status");

        strLstAddZ(argList, TEST_PATH "/pg/pg_xlog");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchivePushAsync(), AssertError, "no WAL files to process");

        TEST_STORAGE_GET(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT "/global.error", "25\nno WAL files to process",
            .comment = "check global.error");

        TEST_STORAGE_LIST(storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT, "global.error\n", .comment = "check status files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("add repo, push already pushed WAL and new WAL");

        // Add repo3
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 3, TEST_PATH "/repo3");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync, .jobRetry = 1);

        HRN_INFO_PUT(
            storageTest, "repo3/archive/test/archive.info",
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":12297848147757817309,\"db-version\":\"9.4\"}\n");

        // Recreate ready file for WAL 1
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_xlog/archive_status/000000010000000100000001.ready");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            .comment = "check repo1 for WAL 1 file");

        // Create a ready file for WAL 2 but don't create the segment yet -- this will test the file error
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_xlog/archive_status/000000010000000100000002.ready");

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        TEST_RESULT_LOG_FMT(
            "P00   INFO: push 2 WAL file(s) to archive: 000000010000000100000001...000000010000000100000002\n"
            "P01   WARN: WAL file '000000010000000100000001' already exists in the repo1 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P01 DETAIL: pushed WAL file '000000010000000100000001' to the archive\n"
            "P01   WARN: could not push WAL file '000000010000000100000002' to the archive (will be retried): "
                "[55] raised from local-1 shim protocol: " STORAGE_ERROR_READ_MISSING "\n"
            "            [FileMissingError] on retry after 0ms",
            TEST_PATH "/pg/pg_xlog/000000010000000100000002");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            .comment = "check repo1 for WAL 1 file");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/9.4-1/0000000100000001/000000010000000100000001-%s", walBuffer1Sha1)),
            .comment = "check repo3 for WAL 1 file");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT,
            "000000010000000100000001.ok\n"
            "000000010000000100000002.error\n",
            .comment = "check status files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create and push previously missing WAL");

        // Create WAL 2 segment
        Buffer *walBuffer2 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer2, bufSize(walBuffer2));
        memset(bufPtr(walBuffer2), 0x0C, bufSize(walBuffer2));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}, walBuffer2);
        const char *walBuffer2Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer2)));

        HRN_STORAGE_PUT(storagePgWrite(), "pg_xlog/000000010000000100000002", walBuffer2);

        argListTemp = strLstDup(argList);
        hrnCfgArgRawZ(argListTemp, cfgOptArchivePushQueueMax, "1gb");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp, .role = cfgCmdRoleAsync);

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        TEST_RESULT_LOG(
            "P00   INFO: push 1 WAL file(s) to archive: 000000010000000100000002\n"
            "P01 DETAIL: pushed WAL file '000000010000000100000002' to the archive");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .comment = "check repo1 for WAL 2 file");
        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/9.4-1/0000000100000001/000000010000000100000002-%s", walBuffer2Sha1)),
            .comment = "check repo3 for WAL 2 file");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT,
            "000000010000000100000001.ok\n"
            "000000010000000100000002.ok\n",
            .comment = "check status files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("push wal 2 again to get warnings from both repos");

        // Remove the OK file so the WAL gets pushed again
        HRN_STORAGE_REMOVE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000002.ok");

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        TEST_RESULT_LOG(
            "P00   INFO: push 1 WAL file(s) to archive: 000000010000000100000002\n"
            "P01   WARN: WAL file '000000010000000100000002' already exists in the repo1 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P01   WARN: WAL file '000000010000000100000002' already exists in the repo3 archive with the same checksum\n"
            "            HINT: this is valid in some recovery scenarios but may also indicate a problem.\n"
            "P01 DETAIL: pushed WAL file '000000010000000100000002' to the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("create and push WAL 3 to both repos");

        // Create WAL 3 segment
        Buffer *walBuffer3 = bufNew((size_t)16 * 1024 * 1024);
        bufUsedSet(walBuffer3, bufSize(walBuffer3));
        memset(bufPtr(walBuffer3), 0x44, bufSize(walBuffer3));
        hrnPgWalToBuffer((PgWal){.version = PG_VERSION_94, .systemId = 0xAAAABBBBCCCCDDDD}, walBuffer3);
        const char *walBuffer3Sha1 = strZ(bufHex(cryptoHashOne(HASH_TYPE_SHA1_STR, walBuffer3)));

        HRN_STORAGE_PUT(storagePgWrite(), "pg_xlog/000000010000000100000003", walBuffer3);

        // Create ready file
        HRN_STORAGE_PUT_EMPTY(storagePgWrite(), "pg_xlog/archive_status/000000010000000100000003.ready");

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segment");
        TEST_RESULT_LOG(
            "P00   INFO: push 1 WAL file(s) to archive: 000000010000000100000003\n"
            "P01 DETAIL: pushed WAL file '000000010000000100000003' to the archive");

        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo/archive/test/9.4-1/0000000100000001/000000010000000100000003-%s", walBuffer3Sha1)),
            .comment = "check repo1 for WAL 3 file");
        TEST_STORAGE_EXISTS(
            storageTest, strZ(strNewFmt("repo3/archive/test/9.4-1/0000000100000001/000000010000000100000003-%s", walBuffer3Sha1)),
            .comment = "check repo3 for WAL 3 file");

        // Remove the ready file to prevent WAL 3 from being considered for the next test
        HRN_STORAGE_REMOVE(storagePgWrite(), "pg_xlog/archive_status/000000010000000100000003.ready", .errorOnMissing = true);

        // Check that drop functionality works
        // -------------------------------------------------------------------------------------------------------------------------
        // Remove status files
        HRN_STORAGE_PATH_REMOVE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT, .recurse = true);
        HRN_STORAGE_PATH_CREATE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT);

        argListTemp = strLstDup(argList);
        hrnCfgArgRawZ(argListTemp, cfgOptArchivePushQueueMax, "16m");
        HRN_CFG_LOAD(cfgCmdArchivePush, argListTemp, .role = cfgCmdRoleAsync);

        TEST_RESULT_VOID(cmdArchivePushAsync(), "push WAL segments");
        TEST_RESULT_LOG(
            "P00   INFO: push 2 WAL file(s) to archive: 000000010000000100000001...000000010000000100000002\n"
            "P00   WARN: dropped WAL file '000000010000000100000001' because archive queue exceeded 16MB\n"
            "P00   WARN: dropped WAL file '000000010000000100000002' because archive queue exceeded 16MB");

        TEST_STORAGE_GET(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok",
            "0\ndropped WAL file '000000010000000100000001' because archive queue exceeded 16MB", .comment = "check WAL 1 warning");

        TEST_STORAGE_GET(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000002.ok",
            "0\ndropped WAL file '000000010000000100000002' because archive queue exceeded 16MB", .comment = "check WAL 2 warning");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_OUT,
            "000000010000000100000001.ok\n"
            "000000010000000100000002.ok\n",
            .comment = "check status files");

        // Uninstall local command handler shim
        hrnProtocolLocalShimUninstall();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
