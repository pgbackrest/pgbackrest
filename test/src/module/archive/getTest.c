/***********************************************************************************************************************************
Test Archive Get Command
***********************************************************************************************************************************/
#include "postgres/interface.h"
#include "postgres/version.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "compress/gzipCompress.h"
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

    // *****************************************************************************************************************************
    if (testBegin("archiveGetCheck()"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control file
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        // Control and archive info mismatch
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("repo/archive/test1/archive.info")),
            bufNewZ(
                "[backrest]\n"
                "backrest-checksum=\"0a415a03fa3faccb4ac171759895478469e9e19e\"\n"
                "backrest-format=5\n"
                "backrest-version=\"2.06\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"));

        TEST_ERROR(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL), ArchiveMismatchError,
            "unable to retrieve the archive id for database version '10' and system-id '18072658121562454734'");

        // Nothing to find in empty archive dir
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("repo/archive/test1/archive.info")),
            bufNewZ(
                "[backrest]\n"
                "backrest-checksum=\"f7617b5c4c9f212f40b9bc3d8ec7f97edbbf96af\"\n"
                "backrest-format=5\n"
                "backrest-version=\"2.06\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":5555555555555555555,\"db-version\":\"9.4\"}\n"
                "2={\"db-id\":18072658121562454734,\"db-version\":\"10\"}\n"
                "3={\"db-id\":18072658121562454734,\"db-version\":\"9.6\"}\n"
                "4={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        TEST_RESULT_PTR(
            archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual, NULL, "no segment found");

        // Write segment into an older archive path
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-2/8765432187654321/876543218765432187654321-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            NULL);

        TEST_RESULT_STR(
            strPtr(archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual),
            "10-2/8765432187654321/876543218765432187654321-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "segment found");

        // Write segment into an newer archive path
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-4/8765432187654321/876543218765432187654321-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")),
            NULL);

        TEST_RESULT_STR(
            strPtr(archiveGetCheck(strNew("876543218765432187654321"), cipherTypeNone, NULL).archiveFileActual),
            "10-4/8765432187654321/876543218765432187654321-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "newer segment found");

        // Get history file
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_PTR(
            archiveGetCheck(strNew("00000009.history"), cipherTypeNone, NULL).archiveFileActual, NULL, "history file not found");

        storagePutNP(storageNewWriteNP(storageTest, strNew("repo/archive/test1/10-4/00000009.history")), NULL);

        TEST_RESULT_STR(
            strPtr(
                archiveGetCheck(
                    strNew("00000009.history"), cipherTypeNone, NULL).archiveFileActual), "10-4/00000009.history",
                    "history file found");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveGetFile()"))
    {
        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create pg_control file
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        // Create archive.info
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("repo/archive/test1/archive.info")),
            bufNewZ(
                "[backrest]\n"
                "backrest-checksum=\"8a041a4128eaa2c08a23dd1f04934627795946ff\"\n"
                "backrest-format=5\n"
                "backrest-version=\"2.06\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        // Nothing to copy
        // -------------------------------------------------------------------------------------------------------------------------
        String *archiveFile = strNew("01ABCDEF01ABCDEF01ABCDEF");
        String *walDestination = strNewFmt("%s/db/pg_wal/RECOVERYXLOG", testPath());
        storagePathCreateNP(storageTest, strPath(walDestination));

        TEST_RESULT_INT(archiveGetFile(archiveFile, walDestination, cipherTypeNone, NULL), 1, "WAL segment missing");

        // Create a WAL segment to copy
        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(16 * 1024 * 1024);
        memset(bufPtr(buffer), 0, bufSize(buffer));
        bufUsedSet(buffer, bufSize(buffer));

        storagePutNP(
            storageNewWriteNP(
                storageTest,
                strNew(
                    "repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
            buffer);

        TEST_RESULT_INT(archiveGetFile(archiveFile, walDestination, cipherTypeNone, NULL), 0, "WAL segment copied");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, walDestination), true, "  check exists");
        TEST_RESULT_INT(storageInfoNP(storageTest, walDestination).size, 16 * 1024 * 1024, "  check size");

        storageRemoveP(
            storageTest,
            strNew("repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            .errorOnMissing = true);
        storageRemoveP(storageTest, walDestination, .errorOnMissing = true);

        // Create a compressed WAL segment to copy
        // -------------------------------------------------------------------------------------------------------------------------
        StorageFileWrite *infoWrite = storageNewWriteNP(storageTest, strNew("repo/archive/test1/archive.info"));

        ioWriteFilterGroupSet(
            storageFileWriteIo(infoWrite),
            ioFilterGroupAdd(
                ioFilterGroupNew(),
                cipherBlockFilter(cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, bufNewStr(strNew("12345678")), NULL))));

        storagePutNP(
            infoWrite,
            bufNewZ(
                "[backrest]\n"
                "backrest-checksum=\"60bfcb0a5a2c91d203c11d7f1924e99dcdfa0b80\"\n"
                "backrest-format=5\n"
                "backrest-version=\"2.06\"\n"
                "\n"
                "[cipher]\n"
                "cipher-pass=\"worstpassphraseever\"\n"
                "\n"
                "[db:history]\n"
                "1={\"db-id\":18072658121562454734,\"db-version\":\"10\"}"));

        StorageFileWrite *destination = storageNewWriteNP(
            storageTest,
            strNew(
                "repo/archive/test1/10-1/01ABCDEF01ABCDEF/01ABCDEF01ABCDEF01ABCDEF-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.gz"));

        IoFilterGroup *filterGroup = ioFilterGroupNew();
        ioFilterGroupAdd(filterGroup, gzipCompressFilter(gzipCompressNew(3, false)));
        ioFilterGroupAdd(
            filterGroup,
            cipherBlockFilter(
                cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, bufNewStr(strNew("worstpassphraseever")), NULL)));
        ioWriteFilterGroupSet(storageFileWriteIo(destination), filterGroup);
        storagePutNP(destination, buffer);

        TEST_RESULT_INT(
            archiveGetFile(
                archiveFile, walDestination, cipherTypeAes256Cbc, strNew("12345678")), 0, "WAL segment copied");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, walDestination), true, "  check exists");
        TEST_RESULT_INT(storageInfoNP(storageTest, walDestination).size, 16 * 1024 * 1024, "  check size");
    }

    // *****************************************************************************************************************************
    if (testBegin("queueNeed()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        size_t queueSize = 16 * 1024 * 1024;
        size_t walSegmentSize = 16 * 1024 * 1024;

        TEST_ERROR_FMT(
            queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92),
            PathOpenError, "unable to open path '%s/spool/archive/test1/in' for read: [2] No such file or directory", testPath());

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN));

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000100000001|000000010000000100000002", "queue size smaller than min");

        // -------------------------------------------------------------------------------------------------------------------------
        queueSize = (16 * 1024 * 1024) * 3;

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000100000001"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000100000001|000000010000000100000002|000000010000000100000003", "empty queue");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *walSegmentBuffer = bufNew(walSegmentSize);
        memset(bufPtr(walSegmentBuffer), 0, walSegmentSize);

        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FE")), walSegmentBuffer);
        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/0000000100000001000000FF")), walSegmentBuffer);

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("0000000100000001000000FE"), false, queueSize, walSegmentSize, PG_VERSION_92), "|")),
            "000000010000000200000000|000000010000000200000001", "queue has wal < 9.3");

        TEST_RESULT_STR(
            strPtr(strLstJoin(storageListNP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), "|")),
            "0000000100000001000000FE", "check queue");

        // -------------------------------------------------------------------------------------------------------------------------
        walSegmentSize = 1024 * 1024;
        queueSize = walSegmentSize * 5;

        storagePutNP(storageNewWriteNP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/junk")), bufNewZ("JUNK"));
        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFE")), walSegmentBuffer);
        storagePutNP(
            storageNewWriteNP(
                storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_IN "/000000010000000A00000FFF")), walSegmentBuffer);

        TEST_RESULT_STR(
            strPtr(strLstJoin(queueNeed(strNew("000000010000000A00000FFD"), true, queueSize, walSegmentSize, PG_VERSION_11), "|")),
            "000000010000000B00000000|000000010000000B00000001|000000010000000B00000002", "queue has wal >= 9.3");

        TEST_RESULT_STR(
            strPtr(strLstJoin(strLstSort(storageListNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_IN)), sortOrderAsc), "|")),
            "000000010000000A00000FFE|000000010000000A00000FFF", "check queue");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchiveGet()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--log-level-file=debug"));
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "WAL segment to get required");
            }
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argListTemp = strLstDup(argList);
        String *walSegment = strNew("000000010000000100000001");
        strLstAdd(argListTemp, walSegment);
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_ERROR(cmdArchiveGet(), ParamRequiredError, "path to copy WAL segment required");
            }
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, strNew("db/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_10, .systemId = 0xFACEFACEFACEFACE}));

        storagePathCreateNP(storageTest, strNewFmt("%s/db/pg_wal", testPath()));

        String *walFile = strNewFmt("%s/db/pg_wal/RECOVERYXLOG", testPath());
        strLstAdd(argListTemp, walFile);
        strLstAdd(argListTemp, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        // Test this in a fork so we can use different Perl options in later tests
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_ERROR_FMT(
                    cmdArchiveGet(), FileMissingError,
                    "unable to open %s/archive/test1/archive.info or %s/archive/test1/archive.info.copy\n"
                    "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
                    "HINT: is archive_command configured in postgresql.conf?\n"
                    "HINT: has a stanza-create been performed?\n"
                    "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                        " scheme.",
                    strPtr(cfgOptionStr(cfgOptRepoPath)), strPtr(cfgOptionStr(cfgOptRepoPath)));
            }
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        argListTemp = strLstDup(argList);
        strLstAdd(argListTemp, strNewFmt("--pg1-path=%s/db", testPath()));
        strLstAddZ(argListTemp, "00000001.history");
        strLstAdd(argListTemp, walFile);
        strLstAddZ(argListTemp, "--archive-async");
        harnessCfgLoad(strLstSize(argListTemp), strLstPtr(argListTemp));

        // Test this in a fork so we can use different Perl options in later tests
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_ERROR_FMT(
                    cmdArchiveGet(), FileMissingError,
                    "unable to open %s/archive/test1/archive.info or %s/archive/test1/archive.info.copy\n"
                    "HINT: archive.info does not exist but is required to push/get WAL segments.\n"
                    "HINT: is archive_command configured in postgresql.conf?\n"
                    "HINT: has a stanza-create been performed?\n"
                    "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving"
                        " scheme.",
                    strPtr(cfgOptionStr(cfgOptRepoPath)), strPtr(cfgOptionStr(cfgOptRepoPath)));
            }
        }
        HARNESS_FORK_END();

        // Make sure the process times out when there is nothing to get
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--spool-path=%s/spool", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, walSegment);
        strLstAddZ(argList, "pg_wal/RECOVERYXLOG");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout getting WAL segment");
            }
        }
        HARNESS_FORK_END();

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive");

        // Check for missing WAL
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePutNP(
            storageNewWriteNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strPtr(walSegment))), NULL);

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "successful get of missing WAL");
            }
        }
        HARNESS_FORK_END();

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive");

        TEST_RESULT_BOOL(
            storageExistsNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s.ok", strPtr(walSegment))), false,
            "check OK file was removed");

        // Write out a WAL segment for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment))),
            bufNewZ("SHOULD-BE-A-REAL-WAL-FILE"));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");
            }
        }
        HARNESS_FORK_END();

        TEST_RESULT_VOID(harnessLogResult("P00   INFO: found 000000010000000100000001 in the archive"), "check log");

        TEST_RESULT_BOOL(
            storageExistsNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment))), false,
            "check WAL segment was removed from source");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, walFile), true, "check WAL segment was moved to destination");
        storageRemoveP(storageTest, walFile, .errorOnMissing = true);

        // Write more WAL segments (in this case queue should be full)
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--archive-get-queue-max=48");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        String *walSegment2 = strNew("000000010000000100000002");

        storagePutNP(
            storageNewWriteNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment))),
            bufNewZ("SHOULD-BE-A-REAL-WAL-FILE"));
        storagePutNP(
            storageNewWriteNP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strPtr(walSegment2))),
            bufNewZ("SHOULD-BE-A-REAL-WAL-FILE"));

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_RESULT_INT(cmdArchiveGet(), 0, "successful get");
            }
        }
        HARNESS_FORK_END();

        TEST_RESULT_VOID(harnessLogResult("P00   INFO: found 000000010000000100000001 in the archive"), "check log");

        TEST_RESULT_BOOL(storageExistsNP(storageTest, walFile), true, "check WAL segment was moved");

        // Make sure the process times out when it can't get a lock
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30000, true), "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                TEST_RESULT_INT(cmdArchiveGet(), 1, "timeout waiting for lock");
            }
        }
        HARNESS_FORK_END();

        harnessLogResult("P00   INFO: unable to find 000000010000000100000001 in the archive");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, BOGUS_STR);
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchiveGet(), ParamInvalidError, "extra parameters found");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
