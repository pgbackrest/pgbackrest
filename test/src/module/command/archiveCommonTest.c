/***********************************************************************************************************************************
Test Archive Common
***********************************************************************************************************************************/
#include <unistd.h>

#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

    // *****************************************************************************************************************************
    if (testBegin("archiveAsyncErrorClear() and archiveAsyncStatus()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        harnessCfgLoad(cfgCmdArchivePush, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        String *segment = strNew("000000010000000100000001");

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), false, "directory and status file not present");
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModeGet, segment, false), false, "directory and status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), false, "status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("clear archive file errors");

        const String *errorSegment = strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment));
        const String *errorGlobal = strNew(STORAGE_SPOOL_ARCHIVE_OUT "/global.error");

        storagePutP(storageNewWriteP(storageSpoolWrite(), errorSegment), NULL);
        storagePutP(storageNewWriteP(storageSpoolWrite(), errorGlobal), NULL);

        TEST_RESULT_VOID(archiveAsyncErrorClear(archiveModePush, segment), "clear error");

        TEST_RESULT_BOOL(storageExistsP(storageSpool(), errorSegment), false, "    check segment error");
        TEST_RESULT_BOOL(storageExistsP(storageSpool(), errorGlobal), false, "    check global error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            BUFSTRDEF(BOGUS_STR));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), FormatError,
            "000000010000000100000001.ok content must have at least two lines");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            BUFSTRDEF(BOGUS_STR "\n"));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), FormatError, "000000010000000100000001.ok message must be > 0");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            BUFSTRDEF(BOGUS_STR "\nmessage"));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), FormatError, "unable to convert base 10 string 'BOGUS' to int");

        storagePutP(storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))), NULL);
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "ok file");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            BUFSTRDEF("0\nwarning"));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "ok file with warning");
        harnessLogResult("P00   WARN: warning");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            BUFSTRDEF("25\nerror"));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "error status renamed to ok");
        harnessLogResult(
            "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");
        TEST_RESULT_VOID(
            storageRemoveP(
                storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), .errorOnMissing = true),
            "remove ok");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))), bufNew(0));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, true), AssertError,
            "status file '000000010000000100000001.error' has no content");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))),
            BUFSTRDEF("25\nmessage"));
        TEST_ERROR(archiveAsyncStatus(archiveModePush, segment, true), AssertError, "message");

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), false, "suppress error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/global.error")),
            BUFSTRDEF("102\nexecute error"));

        TEST_ERROR(archiveAsyncStatus(archiveModePush, strNew("anyfile"), true), ExecuteError, "execute error");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveAsyncStatusErrorWrite() and archiveAsyncStatusOkWrite()"))
    {
        StringList *argList = strLstNew();
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        String *walSegment = strNew("000000010000000100000001");

        TEST_RESULT_VOID(
            archiveAsyncStatusErrorWrite(archiveModeGet, walSegment, 25, strNew("error message")), "write error");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, strNew("archive/db/in/000000010000000100000001.error")))),
            "25\nerror message", "check error");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNew("archive/db/in/000000010000000100000001.error"), .errorOnMissing = true),
            "remove error");

        TEST_RESULT_VOID(
            archiveAsyncStatusErrorWrite(archiveModeGet, NULL, 25, strNew("global error message")), "write global error");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, strNew("archive/db/in/global.error")))),
            "25\nglobal error message", "check global error");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNew("archive/db/in/global.error"), .errorOnMissing = true),
            "remove global error");

        TEST_RESULT_VOID(
            archiveAsyncStatusOkWrite(archiveModeGet, walSegment, NULL), "write ok file");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, strNew("archive/db/in/000000010000000100000001.ok")))),
            "", "check ok");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNew("archive/db/in/000000010000000100000001.ok"), .errorOnMissing = true),
            "remove ok");

        TEST_RESULT_VOID(
            archiveAsyncStatusOkWrite(archiveModeGet, walSegment, strNew("WARNING")), "write ok file with warning");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, strNew("archive/db/in/000000010000000100000001.ok")))),
            "0\nWARNING", "check ok warning");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, strNew("archive/db/in/000000010000000100000001.ok"), .errorOnMissing = true),
            "remove ok");
    }

    // *****************************************************************************************************************************
    if (testBegin("walIsPartial()"))
    {
        TEST_RESULT_BOOL(walIsPartial(strNew("000000010000000100000001")), false, "not partial");
        TEST_RESULT_BOOL(walIsPartial(strNew("FFFFFFFFFFFFFFFFFFFFFFFF.partial")), true, "partial");
    }

    // *****************************************************************************************************************************
    if (testBegin("walIsSegment()"))
    {
        TEST_RESULT_BOOL(walIsSegment(strNew("000000010000000100000001")), true, "wal segment");
        TEST_RESULT_BOOL(walIsSegment(strNew("FFFFFFFFFFFFFFFFFFFFFFFF.partial")), true, "partial wal segment");
        TEST_RESULT_BOOL(walIsSegment(strNew("0000001A.history")), false, "history file");
    }

    // *****************************************************************************************************************************
    if (testBegin("walPath()"))
    {
        const String *pgPath = storagePathP(storageTest, STRDEF("pg"));
        storagePathCreateP(storageTest, pgPath);

        TEST_RESULT_STR_Z(walPath(strNew("/absolute/path"), pgPath, strNew("test")), "/absolute/path", "absolute path");

        THROW_ON_SYS_ERROR(chdir(strPtr(pgPath)) != 0, PathMissingError, "unable to chdir()");
        TEST_RESULT_STR(
            walPath(strNew("relative/path"), pgPath, strNew("test")), strNewFmt("%s/relative/path", strPtr(pgPath)),
            "relative path");


        const String *pgPathLink = storagePathP(storageTest, STRDEF("pg-link"));
        THROW_ON_SYS_ERROR_FMT(
            symlink(strPtr(pgPath), strPtr(pgPathLink)) == -1, FileOpenError,
            "unable to create symlink '%s' to '%s'", strPtr(pgPath), strPtr(pgPathLink));

        THROW_ON_SYS_ERROR(chdir(strPtr(pgPath)) != 0, PathMissingError, "unable to chdir()");
        TEST_RESULT_STR(
            walPath(strNew("relative/path"), pgPathLink, strNew("test")), strNewFmt("%s/relative/path", strPtr(pgPathLink)),
            "relative path");


        THROW_ON_SYS_ERROR(chdir("/") != 0, PathMissingError, "unable to chdir()");
        TEST_ERROR(
            walPath(strNew("relative/path"), pgPathLink, strNew("test")), AssertError,
            hrnReplaceKey("working path '/' is not the same path as '{[path]}/pg-link'"));

        TEST_ERROR(
            walPath(strNew("relative/path"), NULL, strNew("test")), OptionRequiredError,
            "option 'pg1-path' must be specified when relative wal paths are used\n"
                "HINT: is %f passed to test instead of %p?\n"
                "HINT: PostgreSQL may pass relative paths even with %p depending on the environment.");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentFind()"))
    {
        // Load configuration to set repo-path and stanza
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--" CFGOPT_PG1_PATH "=/path/to/pg");
        strLstAdd(argList, strNewFmt("--repo-path=%s", testPath()));
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_STR(walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678"), 0), NULL, "no path");

        storagePathCreateP(storageTest, strNew("archive/db/9.6-2/1234567812345678"));
        TEST_RESULT_STR(walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678"), 0), NULL, "no segment");
        TEST_ERROR(
            walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678"), 100), ArchiveTimeoutError,
            "WAL segment 123456781234567812345678 was not archived before the 100ms timeout\n"
            "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: check the PostgreSQL server log for errors.");

        // Check timeout by making the wal segment appear after 250ms
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                sleepMSec(250);

                storagePutP(
                    storageNewWriteP(
                        storageTest,
                        strNew(
                            "archive/db/9.6-2/1234567812345678/123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
                    NULL);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_STR_Z(
                    walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678"), 1000),
                    "123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "found segment");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        storagePutP(
            storageNewWriteP(
                storageTest,
                strNew("archive/db/9.6-2/1234567812345678/123456781234567812345678-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.gz")),
            NULL);

        TEST_ERROR(
            walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678"), 0),
            ArchiveDuplicateError,
            "duplicates found in archive for WAL segment 123456781234567812345678:"
                " 123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                ", 123456781234567812345678-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.gz"
                "\nHINT: are multiple primaries archiving to this stanza?");

        TEST_RESULT_STR(
            walSegmentFind(storageRepo(), strNew("9.6-2"), strNew("123456781234567812345678.partial"), 0), NULL,
            "did not find partial segment");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentNext()"))
    {
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("000000010000000100000001"), 16 * 1024 * 1024, PG_VERSION_10), "000000010000000100000002",
            "get next");
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_93), "0000000100000001000000FF",
            "get next");
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("0000009900000001000000FF"), 16 * 1024 * 1024, PG_VERSION_93), "000000990000000200000000",
            "get next overflow >= 9.3");
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_92), "000000010000000200000000",
            "get next overflow < 9.3");
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("000000010000000100000003"), 1024 * 1024 * 1024, PG_VERSION_11), "000000010000000200000000",
            "get next overflow >= 11/1GB");
        TEST_RESULT_STR_Z(
            walSegmentNext(strNew("000000010000006700000FFF"), 1024 * 1024, PG_VERSION_11), "000000010000006800000000",
            "get next overflow >= 11/1MB");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentRange()"))
    {
        TEST_RESULT_STR_Z(
            strLstJoin(walSegmentRange(strNew("000000010000000100000000"), 16 * 1024 * 1024, PG_VERSION_92, 1), "|"),
            "000000010000000100000000", "get single");
        TEST_RESULT_STR_Z(
            strLstJoin(walSegmentRange(strNew("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_92, 4), "|"),
            "0000000100000001000000FD|0000000100000001000000FE|000000010000000200000000|000000010000000200000001",
            "get range < 9.3");
        TEST_RESULT_STR_Z(
            strLstJoin(walSegmentRange(strNew("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_93, 4), "|"),
            "0000000100000001000000FD|0000000100000001000000FE|0000000100000001000000FF|000000010000000200000000",
            "get range >= 9.3");
        TEST_RESULT_STR_Z(
            strLstJoin(walSegmentRange(strNew("000000080000000A00000000"), 1024 * 1024 * 1024, PG_VERSION_11, 8), "|"),
            "000000080000000A00000000|000000080000000A00000001|000000080000000A00000002|000000080000000A00000003|"
            "000000080000000B00000000|000000080000000B00000001|000000080000000B00000002|000000080000000B00000003",
            "get range >= 11/1GB");
        TEST_RESULT_STR_Z(
            strLstJoin(walSegmentRange(strNew("000000070000000700000FFE"), 1024 * 1024, PG_VERSION_11, 4), "|"),
            "000000070000000700000FFE|000000070000000700000FFF|000000070000000800000000|000000070000000800000001",
            "get range >= 11/1MB");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
