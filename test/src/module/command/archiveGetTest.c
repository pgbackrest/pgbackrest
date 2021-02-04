/***********************************************************************************************************************************
Test Archive Get Command
***********************************************************************************************************************************/
#include "common/compress/helper.h"
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("queueNeed()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--archive-async");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/unused");
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        size_t queueSize = 16 * 1024 * 1024;
        size_t walSegmentSize = 16 * 1024 * 1024;

        TEST_ERROR_FMT(
            queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            PathMissingError, "unable to list file info for missing path '%s/spool/archive/test1/in'", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN));

        TEST_RESULT_STRLST_Z(
            queueNeed(STRDEF("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000100000001\n000000010000000100000002\n", "queue size smaller than min");

        // -------------------------------------------------------------------------------------------------------------------------
        queueSize = (16 * 1024 * 1024) * 3;

        TEST_RESULT_STRLST_Z(
            queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000100000001\n000000010000000100000002\n000000010000000100000003\n", "empty queue");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *walSegmentBuffer = bufNew(walSegmentSize);
        memset(bufPtr(walSegmentBuffer), 0, walSegmentSize);

        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE", walSegmentBuffer);
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF", walSegmentBuffer);

        TEST_RESULT_STRLST_Z(
            queueNeed(strNew("0000000100000001000000FE"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000200000000\n000000010000000200000001\n", "queue has wal < 9.3");

        TEST_RESULT_STRLST_Z(
            storageListP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), "0000000100000001000000FE\n", "check queue");

        // -------------------------------------------------------------------------------------------------------------------------
        walSegmentSize = 1024 * 1024;
        queueSize = walSegmentSize * 5;

        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/junk", "JUNK");
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFE", walSegmentBuffer);
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF", walSegmentBuffer);

        TEST_RESULT_STRLST_Z(
            queueNeed(strNew("000000010000000A00000FFD"), true, queueSize, walSegmentSize, PG_VERSION_11),
            "000000010000000B00000000\n000000010000000B00000001\n000000010000000B00000002\n", "queue has wal >= 9.3");

        TEST_STORAGE_LIST(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000A00000FFE\n000000010000000A00000FFF\n");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGetAsync()"))
    {
        harnessLogLevelSet(logLevelDetail);

        // Arguments that must be included
        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH_REPO);
        hrnCfgArgRawZ(argBaseList, cfgOptSpoolPath, TEST_PATH_SPOOL);
        hrnCfgArgRawBool(argBaseList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptPgHost, BOGUS_STR);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchiveGetAsync(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "global.error\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no segments");

        argList = strLstDup(argBaseList);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchiveGetAsync(), ParamInvalidError, "at least one wal segment is required");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "global.error\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no segments to find");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n");

        strLstAddZ(argList, "000000010000000100000001");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "get async");

        harnessLogResult(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P00 DETAIL: unable to find 000000010000000100000001 in the archive");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000001.ok\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid compressed segment");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "get async");

        harnessLogResult(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01   WARN: [FileReadError] raised from local-1 protocol: unable to get 000000010000000100000001:\n"
            "            repo1: [FormatError] unexpected eof in compressed data");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000001.error\n000000010000000100000001.pgbackrest.tmp\n");
        TEST_STORAGE_REMOVE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.error");

        TEST_STORAGE_REMOVE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single segment");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");

        // There should be a temp file left over. Make sure it still exists to test that temp files are removed on retry.
        TEST_STORAGE_EXISTS(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.pgbackrest.tmp");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        harnessLogResult(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01 DETAIL: found 000000010000000100000001 in the repo1:10-1 archive");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000001\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple segments where some are missing or errored and mismatched repo");

        hrnCfgArgKeyRawZ(argBaseList, cfgOptRepoPath, 2, TEST_PATH_REPO "2");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "0000000100000001000000FE");
        strLstAddZ(argList, "0000000100000001000000FF");
        strLstAddZ(argList, "000000010000000200000000");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}\n");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/0000000100000001000000FE-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");

        // Create segment duplicates
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        harnessLogResult(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000\n"
            "P00   WARN: some repositories were invalid or encountered errors:\n"
            "            repo2: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'\n"
            "P01 DETAIL: found 0000000100000001000000FE in the repo1:10-1 archive\n"
            "P00 DETAIL: unable to find 0000000100000001000000FF in the archive");

        TEST_STORAGE_LIST(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN,
            "0000000100000001000000FE\n0000000100000001000000FE.ok\n0000000100000001000000FF.ok\n",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on duplicates now that no segments are missing, repo with bad perms");

        // Fix repo 2 archive info but break archive path
        HRN_INFO_PUT(
            storageRepoIdxWrite(1), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n");

        storagePathCreateP(storageRepoIdxWrite(1), STRDEF(STORAGE_REPO_ARCHIVE "/10-1"), .mode = 0400);
        HRN_STORAGE_MODE(storageRepoIdxWrite(1), 0400, STORAGE_REPO_ARCHIVE "/10-1");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/0000000100000001000000FF-efefefefefefefefefefefefefefefefefefefef");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        harnessLogResult(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000\n"
            "P00   WARN: some repositories were invalid or encountered errors for 0000000100000001000000FE:\n"
            "            repo2: [PathOpenError] unable to list file info for path '" TEST_PATH_REPO "2/archive/test2/10-1"
                "/0000000100000001': [13] Permission denied\n"
            "P01 DETAIL: found 0000000100000001000000FE in the repo1:10-1 archive\n"
            "P00   WARN: some repositories were invalid or encountered errors for 0000000100000001000000FF:\n"
            "            repo2: [PathOpenError] unable to list file info for path '" TEST_PATH_REPO "2/archive/test2/10-1"
                "/0000000100000001': [13] Permission denied\n"
            "P01 DETAIL: found 0000000100000001000000FF in the repo1:10-1 archive\n"
            "P00   WARN: [ArchiveDuplicateError] duplicates found for WAL segment 000000010000000200000000:\n"
            "            repo1: 10-1/0000000100000002/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                ", 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "            HINT: are multiple primaries archiving to this stanza?");

        TEST_STORAGE_LIST(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN,
            "0000000100000001000000FE\n0000000100000001000000FE.ok\n0000000100000001000000FF\n0000000100000001000000FF.ok\n"
                "000000010000000200000000.error\n",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("global error on invalid executable");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest-bogus");
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH_REPO);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH_SPOOL);
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test2");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_ASYNC);
        strLstAddZ(argList, "0000000100000001000000FE");
        strLstAddZ(argList, "0000000100000001000000FF");
        strLstAddZ(argList, "000000010000000200000000");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            cmdArchiveGetAsync(), ExecuteError,
            "local-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': [2] No such file or directory");

        harnessLogResult(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000");

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/global.error")))),
            "102\nlocal-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': "
                "[2] No such file or directory",
            "check global error");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "global.error\n", .remove = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGet()"))
    {
        harnessLogLevelSet(logLevelDetail);

        // Arguments that must be included. Use raw config here because we need to keep the
        StringList *argBaseList = strLstNew();
        strLstAddZ(argBaseList, "pgbackrest-bogus");
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH_REPO);
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argBaseList, cfgOptArchiveTimeout, "1");
        hrnCfgArgRawFmt(argBaseList, cfgOptLockPath, "%s/lock", testDataPath());
        strLstAddZ(argBaseList, CFGCMD_ARCHIVE_GET);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptPgHost, BOGUS_STR);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "WAL segment to get required");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "000000010000000100000001");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "path to copy WAL segment required");

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        storagePathCreateP(storageTest, strNewFmt("%s/pg/pg_wal", testPath()));

        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYXLOG");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdArchiveGet(), RepoInvalidError,
            "unable to find a valid repo:\n"
            "repo1: [FileMissingError] unable to load info file '%s/archive/test1/archive.info' or"
                " '%s/archive/test1/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                " scheme.",
            strZ(cfgOptionStr(cfgOptRepoPath)), strZ(cfgOptionStr(cfgOptRepoPath)),
            strZ(strNewFmt("%s/archive/test1/archive.info", strZ(cfgOptionStr(cfgOptRepoPath)))),
            strZ(strNewFmt("%s/archive/test1/archive.info.copy", strZ(cfgOptionStr(cfgOptRepoPath)))));

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "00000001.history");
        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYHISTORY");
        strLstAddZ(argList, "--archive-async");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdArchiveGet(), RepoInvalidError,
            "unable to find a valid repo:\n"
            "repo1: [FileMissingError] unable to load info file '%s/archive/test1/archive.info' or"
                " '%s/archive/test1/archive.info.copy':\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "FileMissingError: " STORAGE_ERROR_READ_MISSING "\n"
            "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "HINT: is archive_command configured correctly in postgresql.conf?\n"
            "HINT: has a stanza-create been performed?\n"
            "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                " scheme.",
            strZ(cfgOptionStr(cfgOptRepoPath)), strZ(cfgOptionStr(cfgOptRepoPath)),
            strZ(strNewFmt("%s/archive/test1/archive.info", strZ(cfgOptionStr(cfgOptRepoPath)))),
            strZ(strNewFmt("%s/archive/test1/archive.info.copy", strZ(cfgOptionStr(cfgOptRepoPath)))));

        // Make sure the process times out when there is nothing to get
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH_SPOOL);
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout getting WAL segment");

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive asynchronously");

        // Check for missing WAL
        // -------------------------------------------------------------------------------------------------------------------------
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok");

        TEST_RESULT_INT(cmdArchiveGet(), 1, "successful get of missing WAL");

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive asynchronously");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), STRDEF(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok")), false,
            "check OK file was removed");

        // Write out a WAL segment for success
        // -------------------------------------------------------------------------------------------------------------------------
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", "SHOULD-BE-A-REAL-WAL-FILE");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");

        TEST_RESULT_VOID(
            harnessLogResult("P00   INFO: found 000000010000000100000001 in the archive asynchronously"), "check log");

        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);
        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // Write more WAL segments (in this case queue should be full)
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--archive-get-queue-max=48");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", "SHOULD-BE-A-REAL-WAL-FILE");
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok", "0\nwarning about x");
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000002", "SHOULD-BE-A-REAL-WAL-FILE");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");

        TEST_RESULT_VOID(
            harnessLogResult(
                "P00   WARN: warning about x\n"
                "P00   INFO: found 000000010000000100000001 in the archive asynchronously"),
            "check log");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // Make sure the process times out when it can't get a lock
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(
                cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), STRDEF("999-dededede"), cfgLockType(), 30000, true),
            "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout waiting for lock");

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive asynchronously");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamInvalidError, "extra parameters found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg version does not match archive.info");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}));

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH_REPO);
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "01ABCDEF01ABCDEF01ABCDEF");
        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYXLOG");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_ERROR(
            cmdArchiveGet(), RepoInvalidError,
            "unable to find a valid repo:\n"
            "repo1: [ArchiveMismatchError] unable to retrieve the archive id for database version '11' and system-id"
                " '18072658121562454734'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg version does not match archive.info");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0x8888888888888888}));

        TEST_ERROR(
            cmdArchiveGet(), RepoInvalidError,
            "unable to find a valid repo:\n"
            "repo1: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '9838263505978427528'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file is missing");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        TEST_RESULT_INT(cmdArchiveGet(), 1, "get");

        harnessLogResult("P00   INFO: unable to find 01ABCDEF01ABCDEF01ABCDEF in the archive");

        TEST_STORAGE_LIST_EMPTY(storageTest, TEST_PATH_PG "/pg_wal");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get WAL segment");

        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        HRN_STORAGE_PUT(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1:10-1 archive");

        TEST_RESULT_UINT(
            storageInfoP(storageTest, STRDEF(TEST_PATH_PG "/pg_wal/RECOVERYXLOG")).size, 16 * 1024 * 1024, "check size");
        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on duplicate WAL segment");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

        TEST_ERROR(
            cmdArchiveGet(), ArchiveDuplicateError,
            "duplicates found for WAL segment 01ABCDEF01ABCDEF01ABCDEF:\n"
            "repo1: 10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                ", 10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "HINT: are multiple primaries archiving to this stanza?");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", NULL);
        TEST_STORAGE_REMOVE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get from prior db-id");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
            "3={\"db-id\":10000000000000000000,\"db-version\":\"11\"}\n"
            "4={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1:10-1 archive");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get from current db-id");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-4/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1:10-4 archive");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);
        TEST_STORAGE_REMOVE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        TEST_STORAGE_REMOVE(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-4/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get partial");

        buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0xFF, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        HRN_STORAGE_PUT(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/10-4/000000010000000100000001.partial-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "000000010000000100000001.partial");
        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYXLOG");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult("P00   INFO: found 000000010000000100000001.partial in the repo1:10-4 archive");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);
        TEST_STORAGE_REMOVE(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/10-4/000000010000000100000001.partial-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get missing history");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "00000001.history");
        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYHISTORY");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_INT(cmdArchiveGet(), 1, "get");

        harnessLogResult("P00   INFO: unable to find 00000001.history in the archive");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get history");

        HRN_STORAGE_PUT(storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/00000001.history", BUFSTRDEF("HISTORY"));

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult("P00   INFO: found 00000001.history in the repo1:10-1 archive");

        TEST_RESULT_UINT(storageInfoP(storageTest, STRDEF(TEST_PATH_PG "/pg_wal/RECOVERYHISTORY")).size, 7, "check size");
        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYHISTORY\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get compressed and encrypted WAL segment with invalid repo");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[cipher]\n"
            "cipher-pass=\"" TEST_CIPHER_PASS_ARCHIVE "\"\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}",
            .cipherType = cipherTypeAes256Cbc);

        HRN_STORAGE_PUT(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer, .compressType = compressTypeGz, .cipherType = cipherTypeAes256Cbc, .cipherPass = TEST_CIPHER_PASS_ARCHIVE);

        // Add encryption options
        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH_PG);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH_REPO "-bogus");
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 2, TEST_PATH_REPO);
        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoCipherType, 2, CIPHER_TYPE_AES_256_CBC);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        strLstAddZ(argList, "01ABCDEF01ABCDEF01ABCDEF");
        strLstAddZ(argList, TEST_PATH_PG "/pg_wal/RECOVERYXLOG");
        harnessCfgLoad(cfgCmdArchiveGet, argList);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult(
            "P00   WARN: some repositories were invalid or encountered errors:\n"
            "            repo1: [FileMissingError] unable to load info file '" TEST_PATH_REPO "-bogus/archive/test1/archive.info'"
                " or '" TEST_PATH_REPO "-bogus/archive/test1/archive.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "-bogus/archive/test1/archive.info'"
                " for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH_REPO "-bogus/archive/test1/archive.info.copy'"
                " for read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate"
                " archiving scheme.\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2:10-1 archive");

        TEST_RESULT_UINT(
            storageInfoP(storageTest, STRDEF(TEST_PATH_PG "/pg_wal/RECOVERYXLOG")).size, 16 * 1024 * 1024, "check size");
        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo1 has info but bad permissions");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        storagePathCreateP(storageRepoIdxWrite(0), STRDEF(STORAGE_REPO_ARCHIVE "/10-2"), .mode = 0400);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult(
            "P00   WARN: " REPO_INVALID_OR_ERR_MSG " for 01ABCDEF01ABCDEF01ABCDEF:\n"
            "            repo1: [PathOpenError] unable to list file info for path '" TEST_PATH_REPO "-bogus/archive/test1/10-2"
                "/01ABCDEF01ABCDEF': [13] Permission denied\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2:10-1 archive");

        TEST_STORAGE_LIST(storageTest, TEST_PATH_PG "/pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("all repos have info but bad permissions");

        HRN_STORAGE_MODE(storageRepoIdxWrite(1), 0400, STORAGE_REPO_ARCHIVE "/10-1");

        TEST_ERROR(
            cmdArchiveGet(), RepoInvalidError,
            "unable to find a valid repo:\n"
            "repo1: [PathOpenError] unable to list file info for path '" TEST_PATH_REPO "-bogus/archive/test1/10-2"
                "/01ABCDEF01ABCDEF': [13] Permission denied\n"
            "repo2: [PathOpenError] unable to list file info for path '" TEST_PATH_REPO "/archive/test1/10-1/01ABCDEF01ABCDEF':"
                " [13] Permission denied");

        HRN_STORAGE_MODE(storageRepoIdxWrite(0), 0700, STORAGE_REPO_ARCHIVE "/10-2");
        HRN_STORAGE_MODE(storageRepoIdxWrite(1), 0700, STORAGE_REPO_ARCHIVE "/10-1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to get from one repo");

        HRN_STORAGE_PUT(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/10-2/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz", NULL);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        harnessLogResult(
            "P00   WARN: " REPO_INVALID_OR_ERR_MSG " for 01ABCDEF01ABCDEF01ABCDEF:\n"
            "            repo1: [FormatError] unexpected eof in compressed data\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2:10-1 archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to get from all repos");

        HRN_STORAGE_MODE(
            storageRepoIdxWrite(1), 0000,
            STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");

        TEST_ERROR(
            cmdArchiveGet(), FileReadError,
            "unable to get 01ABCDEF01ABCDEF01ABCDEF:\n"
            "repo1: [FormatError] unexpected eof in compressed data\n"
            "repo2: [FileOpenError] unable to open file '" TEST_PATH_REPO "/archive/test1/10-1/01ABCDEF01ABCDEF"
                "/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz' for read: [13] Permission denied");

        HRN_STORAGE_MODE(
            storageRepoIdxWrite(1), 0700,
            STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("call protocol function directly");

        // Start a protocol server
        Buffer *serverWrite = bufNew(8192);
        IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
        ioWriteOpen(serverWriteIo);

        ProtocolServer *server = protocolServerNew(
            strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

        bufUsedSet(serverWrite, 0);

        // Add archive-async and spool path
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH_SPOOL);
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleLocal, argList);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        // Setup protocol command
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("01ABCDEF01ABCDEF01ABCDEF"));
        varLstAdd(
            paramList, varNewStrZ("10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"));
        varLstAdd(paramList, varNewUInt(1));
        varLstAdd(paramList, varNewStrZ("10-1"));
        varLstAdd(paramList, varNewUInt(cipherTypeAes256Cbc));
        varLstAdd(paramList, varNewStrZ(TEST_CIPHER_PASS_ARCHIVE));

        TEST_RESULT_BOOL(
            archiveGetProtocol(PROTOCOL_COMMAND_ARCHIVE_GET_STR, paramList, server), true, "protocol archive get");

        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":[0,null]}\n", "check result");
        TEST_STORAGE_LIST(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000002\n01ABCDEF01ABCDEF01ABCDEF\n");

        bufUsedSet(serverWrite, 0);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid protocol command");

        TEST_RESULT_BOOL(archiveGetProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
