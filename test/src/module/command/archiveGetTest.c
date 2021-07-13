/***********************************************************************************************************************************
Test Archive Get Command
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessInfo.h"
#include "common/harnessPostgres.h"
#include "common/harnessProtocol.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("queueNeed()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/unused");
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        size_t queueSize = 16 * 1024 * 1024;
        size_t walSegmentSize = 16 * 1024 * 1024;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path missing");

        TEST_ERROR(
            queueNeed(STRDEF("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            PathMissingError, "unable to list file info for missing path '" TEST_PATH "/spool/archive/test1/in'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("queue size too small");

        HRN_STORAGE_PATH_CREATE(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN);

        TEST_RESULT_STRLST_Z(
            queueNeed(STRDEF("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000100000001\n000000010000000100000002\n", "queue size smaller than min");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("queue empty");

        queueSize = (16 * 1024 * 1024) * 3;

        TEST_RESULT_STRLST_Z(
            queueNeed(STRDEF("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000100000001\n000000010000000100000002\n000000010000000100000003\n", "empty queue");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg version earlier than 9.3");

        Buffer *walSegmentBuffer = bufNew(walSegmentSize);
        memset(bufPtr(walSegmentBuffer), 0, walSegmentSize);

        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE", walSegmentBuffer);
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF", walSegmentBuffer);

        TEST_RESULT_STRLST_Z(
            queueNeed(STRDEF("0000000100000001000000FE"), false, queueSize, walSegmentSize, PG_VERSION_92),
            "000000010000000200000000\n000000010000000200000001\n", "queue has wal < 9.3");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "0000000100000001000000FE\n", .comment = "check queue");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg >= 9.3 and ok/junk status files");

        walSegmentSize = 1024 * 1024;
        queueSize = walSegmentSize * 5;

        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/junk", "JUNK");

        // Bad OK file with wrong length (just to make sure this does not cause strSubN() issues)
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/AAA.ok", "0\nWARNING");

        // OK file with warnings somehow left over from a prior run
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFD.ok", "0\nWARNING");

        // Valid queued WAL segments (one with an OK file containing warnings)
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFE", walSegmentBuffer);
        HRN_STORAGE_PUT(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF", walSegmentBuffer);
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF.ok", "0\nWARNING2");

        // Empty OK file indicating a WAL segment not found at the end of the queue
        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000B00000000.ok");

        TEST_RESULT_STRLST_Z(
            queueNeed(STRDEF("000000010000000A00000FFD"), true, queueSize, walSegmentSize, PG_VERSION_11),
            "000000010000000B00000000\n000000010000000B00000001\n000000010000000B00000002\n", "queue has wal >= 9.3");

        TEST_STORAGE_LIST(
            storageSpool(), STORAGE_SPOOL_ARCHIVE_IN,
            "000000010000000A00000FFE\n000000010000000A00000FFF\n000000010000000A00000FFF.ok\n");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGetAsync()"))
    {
        harnessLogLevelSet(logLevelDetail);

        // Install local command handler shim
        static const ProtocolServerHandler testLocalHandlerList[] = {PROTOCOL_SERVER_HANDLER_ARCHIVE_GET_LIST};
        hrnProtocolLocalShimInstall(testLocalHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(testLocalHandlerList));

        // Arguments that must be included
        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argBaseList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test2");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptPgHost, BOGUS_STR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchiveGetAsync(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/global.error",
            "72\narchive-get command must be run on the PostgreSQL host", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on no segments");

        argList = strLstDup(argBaseList);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

        TEST_ERROR(cmdArchiveGetAsync(), ParamInvalidError, "at least one wal segment is required");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/global.error", "96\nat least one wal segment is required",
            .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no segments to find");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n");

        strLstAddZ(argList, "000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "get async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P00 DETAIL: unable to find 000000010000000100000001 in the archive");

        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on path permission");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/10-1", .mode = 0400);

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "get async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P00   WARN: repo1: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo/archive/test2/10-1"
                "/0000000100000001': [13] Permission denied\n"
            "P00   WARN: [RepoInvalidError] unable to find a valid repository");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.error",
            "103\n"
            "unable to find a valid repository\n"
            "repo1: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo/archive/test2/10-1/0000000100000001':"
                " [13] Permission denied",
            .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        HRN_STORAGE_MODE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/10-1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on invalid compressed segment");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "get async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01   WARN: [FileReadError] raised from local-1 shim protocol: unable to get 000000010000000100000001:\n"
            "            repo1: 10-1/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz"
                " [FormatError] unexpected eof in compressed data");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.error",
            "42\n"
            "raised from local-1 shim protocol: unable to get 000000010000000100000001:\n"
            "repo1: 10-1/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz [FormatError]"
                " unexpected eof in compressed data",
            .remove = true);
        TEST_STORAGE_LIST(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000001.pgbackrest.tmp\n");

        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single segment");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");

        // There should be a temp file left over. Make sure it still exists to test that temp files are removed on retry.
        TEST_STORAGE_EXISTS(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.pgbackrest.tmp");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01 DETAIL: found 000000010000000100000001 in the repo1: 10-1 archive");

        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single segment with one invalid file");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01   WARN: repo1: 10-2/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "P01 DETAIL: found 000000010000000100000001 in the repo1: 10-1 archive");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok",
            "0\n"
            "repo1: 10-2/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz [FormatError]"
                " unexpected eof in compressed data",
            .remove = true);
        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("single segment with one invalid file");

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01   WARN: repo1: 10-2/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "P01 DETAIL: found 000000010000000100000001 in the repo1: 10-1 archive");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok",
            "0\n"
            "repo1: 10-2/0000000100000001/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz [FormatError]"
                " unexpected eof in compressed data",
            .remove = true);
        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-2/000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd.gz",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple segments where some are missing or errored and mismatched repo");

        hrnCfgArgKeyRawZ(argBaseList, cfgOptRepoPath, 2, TEST_PATH "/repo2");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "0000000100000001000000FE");
        strLstAddZ(argList, "0000000100000001000000FF");
        strLstAddZ(argList, "000000010000000200000000");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

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

        TEST_RESULT_LOG(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000\n"
            "P00   WARN: repo2: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'\n"
            "P01 DETAIL: found 0000000100000001000000FE in the repo1: 10-1 archive\n"
            "P00 DETAIL: unable to find 0000000100000001000000FF in the archive");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE.ok",
            "0\n"
            "repo2: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
            " '18072658121562454734'",
            .remove = true);
        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF.ok",
            "0\n"
            "repo2: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
            " '18072658121562454734'",
            .remove = true);
        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

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

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/10-1", .mode = 0400);

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/0000000100000001000000FF-efefefefefefefefefefefefefefefefefefefef");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000\n"
            "P00   WARN: repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test2/10-1"
                "/0000000100000001': [13] Permission denied\n"
            "P00   WARN: repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test2/10-1"
                "/0000000100000002': [13] Permission denied\n"
            "P01 DETAIL: found 0000000100000001000000FE in the repo1: 10-1 archive\n"
            "P01 DETAIL: found 0000000100000001000000FF in the repo1: 10-1 archive\n"
            "P00   WARN: [ArchiveDuplicateError] duplicates found for WAL segment 000000010000000200000000:\n"
            "            repo1: 10-1/0000000100000002/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                ", 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "            HINT: are multiple primaries archiving to this stanza?");

        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE", .remove = true);
        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE.ok",
            "0\n"
            "repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test2/10-1/0000000100000001':"
                " [13] Permission denied",
            .remove = true);
        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF", .remove = true);
        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF.ok",
            "0\n"
            "repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test2/10-1/0000000100000001':"
                " [13] Permission denied",
            .remove = true);
        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000200000000.error",
            "45\n"
            "duplicates found for WAL segment 000000010000000200000000:\n"
            "repo1: 10-1/0000000100000002/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb, 10-1/0000000100000002"
                "/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "HINT: are multiple primaries archiving to this stanza?\n"
            "repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo2/archive/test2/10-1"                     \
                "/0000000100000002': [13] Permission denied",
            .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        HRN_STORAGE_MODE(storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/10-1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on duplicates");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "000000010000000200000000");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000200000000\n"
            "P00   WARN: [ArchiveDuplicateError] duplicates found for WAL segment 000000010000000200000000:\n"
            "            repo1: 10-1/0000000100000002/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                ", 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "            HINT: are multiple primaries archiving to this stanza?");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000200000000.error",
            "45\n"
            "duplicates found for WAL segment 000000010000000200000000:\n"
            "repo1: 10-1/0000000100000002/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb, 10-1/0000000100000002"
                "/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
            "HINT: are multiple primaries archiving to this stanza?",
            .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            .remove = true);
        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("warn on invalid file");

        hrnCfgArgKeyRawZ(argBaseList, cfgOptRepoPath, 3, TEST_PATH "/repo3");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "000000010000000200000000");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync);

        HRN_INFO_PUT(
            storageRepoIdxWrite(2), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"11\"}\n");
        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(2), "10-1", .mode = 0400);

        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");
        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(1),
            STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000200000000\n"
            "P00   WARN: repo3: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'\n"
            "P01   WARN: repo1: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "P01 DETAIL: found 000000010000000200000000 in the repo2: 10-1 archive");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000200000000.ok",
            "0\n"
            "repo3: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'\n"
            "repo1: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data",
            .remove = true);
        TEST_STORAGE_GET_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000200000000", .remove = true);
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);

        TEST_STORAGE_EXISTS(
            storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error with warnings");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(1),
            STORAGE_REPO_ARCHIVE "/10-1/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");

        TEST_RESULT_LOG(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000200000000\n"
            "P00   WARN: repo3: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'\n"
            "P01   WARN: [FileReadError] raised from local-1 shim protocol: unable to get 000000010000000200000000:\n"
            "            repo1: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "            repo2: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data");

        TEST_STORAGE_GET(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000200000000.error",
            "42\n"
            "raised from local-1 shim protocol: unable to get 000000010000000200000000:\n"
            "repo1: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "repo2: 10-1/0000000100000002/000000010000000200000000-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "repo3: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '18072658121562454734'",
            .remove = true);
        TEST_STORAGE_LIST(
            storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000200000000.pgbackrest.tmp\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("global error on invalid executable");

        // Uninstall local command handler shim
        hrnProtocolLocalShimUninstall();

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test2");
        strLstAddZ(argList, "0000000100000001000000FE");
        strLstAddZ(argList, "0000000100000001000000FF");
        strLstAddZ(argList, "000000010000000200000000");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleAsync, .exeBogus = true);

        TEST_ERROR(
            cmdArchiveGetAsync(), ExecuteError,
            "local-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': [2] No such file or directory");

        TEST_RESULT_LOG(
            "P00   INFO: get 3 WAL file(s) from archive: 0000000100000001000000FE...000000010000000200000000");

        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageSpool(), STRDEF(STORAGE_SPOOL_ARCHIVE_IN "/global.error")))),
            "102\nlocal-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': "
                "[2] No such file or directory",
            "check global error");

        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "global.error\n", .remove = true);
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGet()"))
    {
        harnessLogLevelSet(logLevelDetail);

        StringList *argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argBaseList, cfgOptArchiveTimeout, "1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptPgHost, BOGUS_STR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("segment parameter not specified");

        argList = strLstDup(argBaseList);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "WAL segment to get required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("path parameter not specified");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "000000010000000100000001");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "path to copy WAL segment required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no valid repo");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), RepoInvalidError, "unable to find a valid repository");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [FileMissingError] unable to load info file '" TEST_PATH "/repo/archive/test1/archive.info' or '"
                TEST_PATH "/repo/archive/test1/archive.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info.copy' for"
                " read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                " scheme.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no valid repo - async");

        argList = strLstDup(argBaseList);
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        strLstAddZ(argList, "00000001.history");
        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYHISTORY");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), RepoInvalidError, "unable to find a valid repository");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [FileMissingError] unable to load info file '" TEST_PATH "/repo/archive/test1/archive.info' or '"
                TEST_PATH "/repo/archive/test1/archive.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info' for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo/archive/test1/archive.info.copy' for"
                " read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                " scheme.");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("WAL not found - timeout");

        // Make sure the process times out when there is nothing to get
        argList = strLstDup(argBaseList);
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        TEST_ERROR(
            cmdArchiveGet(), ArchiveTimeoutError,
            "unable to get WAL file '000000010000000100000001' from the archive asynchronously after 1 second(s)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check for missing WAL");

        HRN_STORAGE_PUT_EMPTY(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok");

        TEST_ERROR(
            cmdArchiveGet(), ArchiveTimeoutError,
            "unable to get WAL file '000000010000000100000001' from the archive asynchronously after 1 second(s)");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), STRDEF(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok")), false,
            "check OK file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write WAL segment for success");

        HRN_STORAGE_PATH_CREATE(storagePgWrite(), "pg_wal");
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", "SHOULD-BE-A-REAL-WAL-FILE");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");

        TEST_RESULT_LOG("P00   INFO: found 000000010000000100000001 in the archive asynchronously");

        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);
        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write WAL segments for success - queue full");

        hrnCfgArgRawZ(argList, cfgOptArchiveGetQueueMax, "48");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        // Write more WAL segments (in this case queue should be full)
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001", "SHOULD-BE-A-REAL-WAL-FILE");
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok", "0\nwarning about x");
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000002", "SHOULD-BE-A-REAL-WAL-FILE");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");

        TEST_RESULT_LOG(
            "P00   WARN: warning about x\n"
            "P00   INFO: found 000000010000000100000001 in the archive asynchronously");

        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);
        TEST_STORAGE_LIST(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN, "000000010000000100000002\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to get lock");

        // Make sure the process times out when it can't get a lock
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNewOpen(STRDEF("child read"), HARNESS_FORK_CHILD_READ(), 2000);
                IoWrite *write = ioFdWriteNewOpen(STRDEF("child write"), HARNESS_FORK_CHILD_WRITE(), 2000);

                TEST_RESULT_VOID(
                    lockAcquire(
                        cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), STRDEF("999-dededede"), cfgLockType(), 30000,
                        true),
                    "acquire lock");

                // Let the parent know the lock has been acquired and wait for the parent to allow lock release
                ioWriteStrLine(write, strNew());
                ioWriteFlush(write);
                ioReadLine(read);

                lockRelease(true);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNewOpen(STRDEF("parent read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                IoWrite *write = ioFdWriteNewOpen(STRDEF("parent write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);

                // Wait for the child to acquire the lock
                ioReadLine(read);

                TEST_ERROR(
                    cmdArchiveGet(), ArchiveTimeoutError,
                    "unable to get WAL file '000000010000000100000001' from the archive asynchronously after 1 second(s)");

                // Notify the child to release the lock
                ioWriteLine(write, bufNew(0));
                ioWriteFlush(write);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("too many parameters specified");

        strLstAddZ(argList, BOGUS_STR);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .exeBogus = true);

        TEST_ERROR(cmdArchiveGet(), ParamInvalidError, "extra parameters found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg version does not match archive.info");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACEFACEFACE}));

        HRN_INFO_PUT(
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=1\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        argBaseList = strLstNew();
        hrnCfgArgRawZ(argBaseList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argBaseList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argBaseList, cfgOptStanza, "test1");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "01ABCDEF01ABCDEF01ABCDEF");
        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ERROR(cmdArchiveGet(), RepoInvalidError, "unable to find a valid repository");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [ArchiveMismatchError] unable to retrieve the archive id for database version '11' and system-id"
                " '18072658121562454734'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg system id does not match archive.info");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0x8888888888888888}));

        TEST_ERROR(cmdArchiveGet(), RepoInvalidError, "unable to find a valid repository");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [ArchiveMismatchError] unable to retrieve the archive id for database version '10' and system-id"
                " '9838263505978427528'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("file is missing");

        HRN_STORAGE_PUT(
            storagePgWrite(), PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL,
            hrnPgControlToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        TEST_RESULT_INT(cmdArchiveGet(), 1, "get");

        TEST_RESULT_LOG("P00   INFO: unable to find 01ABCDEF01ABCDEF01ABCDEF in the archive");

        TEST_STORAGE_LIST_EMPTY(storagePg(), "pg_wal");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get WAL segment");

        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        HRN_STORAGE_PUT(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            buffer);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1: 10-1 archive");

        TEST_RESULT_UINT(storageInfoP(storagePg(), STRDEF("pg_wal/RECOVERYXLOG")).size, 16 * 1024 * 1024, "check size");
        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);

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

        TEST_STORAGE_LIST(storagePg(), "pg_wal", NULL);
        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            .remove = true);

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

        TEST_RESULT_LOG("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1: 10-1 archive");

        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get from current db-id");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-4/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo1: 10-4 archive");

        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);
        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            .remove = true);
        TEST_STORAGE_EXISTS(
            storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-4/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            .remove = true);

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
        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG("P00   INFO: found 000000010000000100000001.partial in the repo1: 10-4 archive");

        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);
        TEST_STORAGE_EXISTS(
            storageRepoWrite(),
            STORAGE_REPO_ARCHIVE "/10-4/000000010000000100000001.partial-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get missing history");

        argList = strLstDup(argBaseList);
        strLstAddZ(argList, "00000001.history");
        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYHISTORY");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_RESULT_INT(cmdArchiveGet(), 1, "get");

        TEST_RESULT_LOG("P00   INFO: unable to find 00000001.history in the archive");

        TEST_STORAGE_LIST(storagePg(), "pg_wal", NULL);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("get history");

        HRN_STORAGE_PUT(storageRepoWrite(), STORAGE_REPO_ARCHIVE "/10-1/00000001.history", BUFSTRDEF("HISTORY"));

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG("P00   INFO: found 00000001.history in the repo1: 10-1 archive");

        TEST_RESULT_UINT(storageInfoP(storagePg(), STRDEF("pg_wal/RECOVERYHISTORY")).size, 7, "check size");
        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYHISTORY\n", .remove = true);

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
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, TEST_PATH "/repo-bogus");
        hrnCfgArgKeyRawFmt(argList, cfgOptRepoPath, 2, TEST_PATH "/repo");
        hrnCfgArgKeyRawStrId(argList, cfgOptRepoCipherType, 2, cipherTypeAes256Cbc);
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        strLstAddZ(argList, "01ABCDEF01ABCDEF01ABCDEF");
        strLstAddZ(argList, TEST_PATH "/pg/pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [FileMissingError] unable to load info file '" TEST_PATH "/repo-bogus/archive/test1/archive.info'"
                " or '" TEST_PATH "/repo-bogus/archive/test1/archive.info.copy':\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo-bogus/archive/test1/archive.info'"
                " for read\n"
            "            FileMissingError: unable to open missing file '" TEST_PATH "/repo-bogus/archive/test1/archive.info.copy'"
                " for read\n"
            "            HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
            "            HINT: is archive_command configured correctly in postgresql.conf?\n"
            "            HINT: has a stanza-create been performed?\n"
            "            HINT: use --no-archive-check to disable archive checks during backup if you have an alternate"
                " archiving scheme.\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2: 10-1 archive");

        TEST_RESULT_UINT(storageInfoP(storagePg(), STRDEF("pg_wal/RECOVERYXLOG")).size, 16 * 1024 * 1024, "check size");
        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo1 has info but bad permissions");

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        HRN_STORAGE_PATH_CREATE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/10-2", .mode = 0400);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo-bogus/archive/test1/10-2"
                "/01ABCDEF01ABCDEF': [13] Permission denied\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2: 10-1 archive");

        TEST_STORAGE_LIST(storagePgWrite(), "pg_wal", "RECOVERYXLOG\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("all repos have info but bad permissions");

        HRN_STORAGE_MODE(storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/10-1", .mode = 0400);

        TEST_ERROR(cmdArchiveGet(), RepoInvalidError, "unable to find a valid repository");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo-bogus/archive/test1/10-2"
                "/01ABCDEF01ABCDEF': [13] Permission denied\n"
            "P00   WARN: repo2: [PathOpenError] unable to list file info for path '" TEST_PATH "/repo/archive/test1/10-1"
                "/01ABCDEF01ABCDEF': [13] Permission denied");

        HRN_STORAGE_MODE(storageRepoIdxWrite(0), STORAGE_REPO_ARCHIVE "/10-2");
        HRN_STORAGE_MODE(storageRepoIdxWrite(1), STORAGE_REPO_ARCHIVE "/10-1");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to get from one repo");

        HRN_STORAGE_PUT_EMPTY(
            storageRepoIdxWrite(0),
            STORAGE_REPO_ARCHIVE "/10-2/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG(
            "P00   WARN: repo1: 10-2/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"
                " [FormatError] unexpected eof in compressed data\n"
            "P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2: 10-1 archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unable to get from all repos");

        HRN_STORAGE_MODE(
            storageRepoIdxWrite(1),
            STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz", .mode = 0200);

        TEST_ERROR(
            cmdArchiveGet(), FileReadError,
            "unable to get 01ABCDEF01ABCDEF01ABCDEF:\n"
            "repo1: 10-2/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz [FormatError]"
                " unexpected eof in compressed data\n"
            "repo2: 10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz [FileOpenError]"
                " unable to open file '" TEST_PATH "/repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF"
                    "-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz' for read: [13] Permission denied");

        HRN_STORAGE_MODE(
            storageRepoIdxWrite(1),
            STORAGE_REPO_ARCHIVE "/10-1/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("repo is specified so invalid repo is skipped");

        hrnCfgArgRawZ(argList, cfgOptRepo, "2");
        hrnCfgEnvKeyRawZ(cfgOptRepoCipherPass, 2, TEST_CIPHER_PASS);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);
        hrnCfgEnvKeyRemoveRaw(cfgOptRepoCipherPass, 2);

        TEST_RESULT_INT(cmdArchiveGet(), 0, "get");

        TEST_RESULT_LOG("P00   INFO: found 01ABCDEF01ABCDEF01ABCDEF in the repo2: 10-1 archive");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no segments to find with existing ok file");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, TEST_PATH "/repo");
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argList, cfgOptArchiveTimeout, "10");
        hrnCfgArgRawZ(argList, cfgOptSpoolPath, TEST_PATH "/spool");
        hrnCfgArgRawBool(argList, cfgOptArchiveAsync, true);
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        HRN_INFO_PUT(
            storageRepoIdxWrite(0), INFO_ARCHIVE_PATH_FILE,
            "[db]\n"
            "db-id=2\n"
            "\n"
            "[db:history]\n"
            "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}");

        // Put a warning in the file to show that it was read and later overwritten
        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.ok", "0\nshould not be output");

        TEST_RESULT_VOID(cmdArchiveGet(), "get async");

        TEST_RESULT_LOG("P00   INFO: unable to find 000000010000000100000001 in the archive asynchronously");

        // Check that the ok file is missing since it should have been removed on the first loop and removed again on a subsequent
        // loop once the async process discovered that the file was missing and wrote the ok file again.
        TEST_STORAGE_LIST_EMPTY(storageSpool(), STORAGE_SPOOL_ARCHIVE_IN);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
