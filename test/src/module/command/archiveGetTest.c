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

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // Start a protocol server to test the protocol directly
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(
        strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("archiveGetCheck()"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        // Create pg_control file
        storagePutP(
            storageNewWriteP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        // Control and archive info mismatch
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test1/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"));

        TEST_ERROR(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '10' and system-id '18072658121562454734'");

        // Nothing to find in empty archive dir
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test1/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=3\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
                "3={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n"
                "4={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        TEST_RESULT_STR(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual, NULL, "no segment found");

        // Write segment into an older archive path
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-2/8765432187654321/876543218765432187654321-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            NULL);

        TEST_RESULT_STR_Z(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual,
            "10-2/8765432187654321/876543218765432187654321-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "segment found");

        // Write segment into an newer archive path
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-4/8765432187654321/876543218765432187654321-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")),
            NULL);

        TEST_RESULT_STR_Z(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual,
            "10-4/8765432187654321/876543218765432187654321-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "newer segment found");

        // Get history file
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(
            archiveGetCheck(strNew("00000009.history"), cipherTypeNone, NULL).archiveFileActual, NULL, "history file not found");

        storagePutP(storageNewWriteP(storageTest, strNew("repo/archive/test1/10-4/00000009.history")), NULL);

        TEST_RESULT_STR_Z(
            archiveGetCheck(strNew("00000009.history"), cipherTypeNone, NULL).archiveFileActual, "10-4/00000009.history",
            "history file found");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveGetFile()"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        // Create pg_control file
        storagePutP(
            storageNewWriteP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        // Create archive.info
        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test1/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        // Nothing to copy
        // -------------------------------------------------------------------------------------------------------------------------
        String *archiveFile = strNew("01ABCDEF01ABCDEF01ABCDEF");
        String *walDestination = strNewFmt("%s/db/pg_wal/RECOVERYXLOG", testPath());
        storagePathCreateP(storageTest, strPath(walDestination));

        TEST_RESULT_INT(
            archiveGetFile(storageTest, archiveFile, walDestination, false, cipherTypeNone, NULL), 1, "WAL segment missing");

        // Create a WAL segment to copy
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        storagePutP(
            storageNewWriteP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            buffer);

        TEST_RESULT_INT(
            archiveGetFile(storageTest, archiveFile, walDestination, false, cipherTypeNone, NULL), 0, "WAL segment copied");
        TEST_RESULT_BOOL(storageExistsP(storageTest, walDestination), true, "  check exists");
        TEST_RESULT_UINT(storageInfoP(storageTest, walDestination).size, 16 * 1024 * 1024, "  check size");

        storageRemoveP(
            storageTest,
            strNew("repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            .errorOnMissing = true);
        storageRemoveP(storageTest, walDestination, .errorOnMissing = true);

        // Create a compressed WAL segment to copy
        // -------------------------------------------------------------------------------------------------------------------------
        StorageWrite *infoWrite = storageNewWriteP(storageTest, strNew("repo/archive/test1/archive.info"));

        ioFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(infoWrite)), cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc,
            BUFSTRDEF("12345678"), NULL));

        storagePutP(
            infoWrite,
            harnessInfoChecksumZ(
                "[cipher]\n"
                "cipher-pass=\"worstpassphraseever\"\n"
                "\n"
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        StorageWrite *destination = storageNewWriteP(
            storageTest,
            strNew(
                "repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"));

        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(destination));
        ioFilterGroupAdd(filterGroup, compressFilter(compressTypeGz, 3));
        ioFilterGroupAdd(
            filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("worstpassphraseever"), NULL));
        storagePutP(destination, buffer);

        TEST_RESULT_INT(
            archiveGetFile(
                storageTest, archiveFile, walDestination, false, cipherTypeAes256Cbc, strNew("12345678")), 0, "WAL segment copied");
        TEST_RESULT_BOOL(storageExistsP(storageTest, walDestination), true, "  check exists");
        TEST_RESULT_UINT(storageInfoP(storageTest, walDestination).size, 16 * 1024 * 1024, "  check size");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "--repo1-cipher-type=aes-256-cbc");
        setenv("PGBACKREST_REPO1_CIPHER_PASS", "12345678", true);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);
        unsetenv("PGBACKREST_REPO1_CIPHER_PASS");

        storagePathCreateP(storageTest, strNew("spool/archive/test1/in"));

        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStr(archiveFile));

        TEST_RESULT_BOOL(
            archiveGetProtocol(PROTOCOL_COMMAND_ARCHIVE_GET_STR, paramList, server), true, "protocol archive get");
        TEST_RESULT_STR_Z(strNewBuf(serverWrite), "{\"out\":0}\n", "check result");
        TEST_RESULT_BOOL(
            storageExistsP(storageTest, strNewFmt("spool/archive/test1/in/%s", strZ(archiveFile))), true, "  check exists");

        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(archiveGetProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

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

        TEST_RESULT_STR_Z(
            strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|"),
            "000000010000000100000001|000000010000000100000002", "queue size smaller than min");

        // -------------------------------------------------------------------------------------------------------------------------
        queueSize = (16 * 1024 * 1024) * 3;

        TEST_RESULT_STR_Z(
            strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|"),
            "000000010000000100000001|000000010000000100000002|000000010000000100000003", "empty queue");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *walSegmentBuffer = bufNew(walSegmentSize);
        memset(bufPtr(walSegmentBuffer), 0, walSegmentSize);

        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE")), walSegmentBuffer);
        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF")), walSegmentBuffer);

        TEST_RESULT_STR_Z(
            strLstJoin(queueNeed(strNew("0000000100000001000000FE"), false, queueSize, walSegmentSize, PG_VERSION_92), "|"),
            "000000010000000200000000|000000010000000200000001", "queue has wal < 9.3");

        TEST_RESULT_STR_Z(
            strLstJoin(storageListP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), "|"), "0000000100000001000000FE",
            "check queue");

        // -------------------------------------------------------------------------------------------------------------------------
        walSegmentSize = 1024 * 1024;
        queueSize = walSegmentSize * 5;

        storagePutP(storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/junk")), BUFSTRDEF("JUNK"));
        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFE")), walSegmentBuffer);
        storagePutP(
            storageNewWriteP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF")), walSegmentBuffer);

        TEST_RESULT_STR_Z(
            strLstJoin(queueNeed(strNew("000000010000000A00000FFD"), true, queueSize, walSegmentSize, PG_VERSION_11), "|"),
            "000000010000000B00000000|000000010000000B00000001|000000010000000B00000002", "queue has wal >= 9.3");

        TEST_RESULT_STR_Z(
            strLstJoin(strLstSort(storageListP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), sortOrderAsc), "|"),
            "000000010000000A00000FFE|000000010000000A00000FFF", "check queue");
    }


    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGetAsync()"))
    {
        harnessLogLevelSet(logLevelDetail);

        StringList *argCleanList = strLstNew();
        strLstAdd(argCleanList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argCleanList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argCleanList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argCleanList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argCleanList, "--stanza=test2");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argCleanList);

        TEST_ERROR(cmdArchiveGetAsync(), ParamInvalidError, "at least one wal segment is required");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        strLstAddZ(argList, "--" CFGOPT_SPOOL_PATH "=/spool");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        strLstAddZ(argList, "000000010000000100000001");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        TEST_ERROR(cmdArchiveGetAsync(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        // Create pg_control file and archive.info
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageTest, strNew("pg/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        storagePutP(
            storageNewWriteP(storageTest, strNew("repo/archive/test2/archive.info")),
            harnessInfoChecksumZ(
                "[db]\n"
                "db-id=1\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"));

        // Get a single segment
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argCleanList);
        strLstAddZ(argList, "000000010000000100000001");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        storagePathCreateP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN));

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNew(
                        "repo/archive/test2/10-1/0000000100000001/"
                            "000000010000000100000001-abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd")),
                NULL),
            "normal WAL segment");

        // Create tmp file to make it look like a prior async get failed partway through to ensure that retries work
        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.pgbackrest.tmp")),
                NULL),
            "normal WAL segment");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");
        harnessLogResult(
            "P00   INFO: get 1 WAL file(s) from archive: 000000010000000100000001\n"
            "P01 DETAIL: found 000000010000000100000001 in the archive");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001")), true,
            "check 000000010000000100000001 in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.pgbackrest.tmp")), false,
            "check 000000010000000100000001 tmp not in spool");

        // Get multiple segments where some are missing or errored
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstDup(argCleanList);
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "000000010000000100000002");
        strLstAddZ(argList, "000000010000000100000003");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        storagePathCreateP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN));

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNew(
                        "repo/archive/test2/10-1/0000000100000001/"
                            "000000010000000100000003-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
                NULL),
            "normal WAL segment");

        TEST_RESULT_VOID(
            storagePutP(
                storageNewWriteP(
                    storageTest,
                    strNew(
                        "repo/archive/test2/10-1/0000000100000001/"
                            "000000010000000100000003-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")),
                NULL),
            "duplicate WAL segment");

        TEST_RESULT_VOID(cmdArchiveGetAsync(), "archive async");
        harnessLogResult(
            "P00   INFO: get 3 WAL file(s) from archive: 000000010000000100000001...000000010000000100000003\n"
            "P01 DETAIL: found 000000010000000100000001 in the archive\n"
            "P01 DETAIL: unable to find 000000010000000100000002 in the archive\n"
            "P01   WARN: could not get 000000010000000100000003 from the archive (will be retried): "
                "[45] raised from local-1 protocol: duplicates found in archive for WAL segment 000000010000000100000003: "
                "000000010000000100000003-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa, "
                "000000010000000100000003-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n"
            "            HINT: are multiple primaries archiving to this stanza?");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001")), true,
            "check 000000010000000100000001 in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000002")), false,
            "check 000000010000000100000002 not in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000002.ok")), true,
            "check 000000010000000100000002.ok in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000003")), false,
            "check 000000010000000100000003 not in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000003.error")), true,
            "check 000000010000000100000003.error in spool");

        protocolFree();

        // -------------------------------------------------------------------------------------------------------------------------
        storageRemoveP(
            storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000003.error"), .errorOnMissing = true);

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest-bogus");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        strLstAddZ(argList, "--stanza=test2");
        strLstAddZ(argList, CFGCMD_ARCHIVE_GET ":" CONFIG_COMMAND_ROLE_ASYNC);
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "000000010000000100000002");
        strLstAddZ(argList, "000000010000000100000003");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            cmdArchiveGetAsync(), ExecuteError,
            "local-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': [2] No such file or directory");

        harnessLogResult(
            "P00   INFO: get 3 WAL file(s) from archive: 000000010000000100000001...000000010000000100000003");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000001.error")), false,
            "check 000000010000000100000001.error not in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000002.error")), false,
            "check 000000010000000100000002.error not in spool");
        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000100000003.error")), false,
            "check 000000010000000100000003.error not in spool");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/global.error")))),
            "102\nlocal-1 process terminated unexpectedly [102]: unable to execute 'pgbackrest-bogus': "
                "[2] No such file or directory",
            "check global error");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGet()"))
    {
        TEST_TITLE("command must be run on the pg host");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptPgHost, "host");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg");
        hrnCfgArgRawZ(argList, cfgOptRepoPath, "/repo");
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test2");
        strLstAddZ(argList, "000000010000000100000001");
        strLstAddZ(argList, "pg_wal/000000010000000100000001");
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleDefault, argList);

        TEST_ERROR(cmdArchiveGet(), HostInvalidError, "archive-get command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest-bogus");                    // Break this until async tests are setup correctly
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAdd(argList, strNewFmt("--lock-path=%s/lock", testPath()));
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--log-level-file=debug"));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        hrnCfgArgRawFmt(argList, cfgOptPgPath, "%s/db", testPath());
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "WAL segment to get required");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTemp = strLstDup(argList);
        String *walSegment = strNew("000000010000000100000001");
        strLstAdd(argListTemp, walSegment);
        harnessCfgLoadRaw(strLstSize(argListTemp), strLstPtr(argListTemp));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "path to copy WAL segment required");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        storagePathCreateP(storageTest, strNewFmt("%s/db/pg_wal", testPath()));

        String *walFile = strNewFmt("%s/db/pg_wal/RECOVERYXLOG", testPath());
        strLstAdd(argListTemp, walFile);
        harnessCfgLoadRaw(strLstSize(argListTemp), strLstPtr(argListTemp));

        // Test this in a fork so we can use different Perl options in later tests
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_ERROR_FMT(
                    cmdArchiveGet(), FileMissingError,
                    "unable to load info file '%s/archive/test1/archive.info' or '%s/archive/test1/archive.info.copy':\n"
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
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAddZ(argListTemp, "00000001.history");
        strLstAdd(argListTemp, walFile);
        strLstAddZ(argListTemp, "--archive-async");
        harnessCfgLoadRaw(strLstSize(argListTemp), strLstPtr(argListTemp));

        // Test this in a fork so we can use different Perl options in later tests
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_ERROR_FMT(
                    cmdArchiveGet(), FileMissingError,
                    "unable to load info file '%s/archive/test1/archive.info' or '%s/archive/test1/archive.info.copy':\n"
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
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // Make sure the process times out when there is nothing to get
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, walSegment);
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        THROW_ON_SYS_ERROR(chdir(strZ(cfgOptionStr(cfgOptPgPath))) != 0, PathMissingError, "unable to chdir()");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout getting WAL segment");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        harnessLogResult("P01   INFO: unable to find 000000010000000100000001 in the archive");

        // Check for missing WAL
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strZ(walSegment))), NULL);

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "successful get of missing WAL");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        harnessLogResult("P01   INFO: unable to find 000000010000000100000001 in the archive");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strZ(walSegment))), false,
            "check OK file was removed");

        // Write out a WAL segment for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment))),
            BUFSTRDEF("SHOULD-BE-A-REAL-WAL-FILE"));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        TEST_RESULT_VOID(harnessLogResult("P01   INFO: found 000000010000000100000001 in the archive"), "check log");

        TEST_RESULT_BOOL(
            storageExistsP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment))), false,
            "check WAL segment was removed from source");
        TEST_RESULT_BOOL(storageExistsP(storageTest, walFile), true, "check WAL segment was moved to destination");
        storageRemoveP(storageTest, walFile, .errorOnMissing = true);

        // Write more WAL segments (in this case queue should be full)
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--archive-get-queue-max=48");
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        String *walSegment2 = strNew("000000010000000100000002");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment))),
            BUFSTRDEF("SHOULD-BE-A-REAL-WAL-FILE"));
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment2))),
            BUFSTRDEF("SHOULD-BE-A-REAL-WAL-FILE"));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        TEST_RESULT_VOID(harnessLogResult("P01   INFO: found 000000010000000100000001 in the archive"), "check log");

        TEST_RESULT_BOOL(storageExistsP(storageTest, walFile), true, "check WAL segment was moved");

        // Make sure the process times out when it can't get a lock
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(
                cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgOptionStr(cfgOptExecId), cfgLockType(), 30000, true),
            "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout waiting for lock");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        harnessLogResult("P01   INFO: unable to find 000000010000000100000001 in the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamInvalidError, "extra parameters found");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
