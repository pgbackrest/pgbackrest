/***********************************************************************************************************************************
Test Archive Common
***********************************************************************************************************************************/
#include <unistd.h>

#include "storage/helper.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessStorage.h"

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
    if (testBegin("archiveAsyncErrorClear() and archiveAsyncStatus()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--spool-path=" TEST_PATH);
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        harnessCfgLoad(cfgCmdArchivePush, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        const String *segment = STRDEF("000000010000000100000001");

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), false, "directory and status file not present");
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModeGet, segment, false, true), false, "directory and status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        mkdir(TEST_PATH "/archive", 0750);
        mkdir(TEST_PATH "/archive/db", 0750);
        mkdir(TEST_PATH "/archive/db/out", 0750);

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), false, "status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("clear archive file errors");

        const String *errorSegment = strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strZ(segment));
        const String *errorGlobal = STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/global.error");

        storagePutP(storageNewWriteP(storageSpoolWrite(), errorSegment), NULL);
        storagePutP(storageNewWriteP(storageSpoolWrite(), errorGlobal), NULL);

        TEST_RESULT_VOID(archiveAsyncErrorClear(archiveModePush, segment), "clear error");

        TEST_RESULT_BOOL(storageExistsP(storageSpool(), errorSegment), false, "    check segment error");
        TEST_RESULT_BOOL(storageExistsP(storageSpool(), errorGlobal), false, "    check global error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))),
            BUFSTRDEF(BOGUS_STR));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false, true), FormatError,
            "000000010000000100000001.ok content must have at least two lines");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))),
            BUFSTRDEF(BOGUS_STR "\n"));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false, true), FormatError,
            "000000010000000100000001.ok message must be > 0");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))),
            BUFSTRDEF(BOGUS_STR "\nmessage"));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false, true),
            FormatError, "unable to convert base 10 string 'BOGUS' to int");

        storagePutP(storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))), NULL);
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), true, "ok file");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))),
            BUFSTRDEF("0\nwarning"));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), true, "ok file with warning");
        harnessLogResult("P00   WARN: warning");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("ignore ok file warning");

        HRN_STORAGE_PUT_Z(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok", "0\nwarning 2");
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, false), true, "check status");
        TEST_RESULT_LOG("");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment))),
            BUFSTRDEF("25\nerror"));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), true, "error status renamed to ok");
        harnessLogResult(
            "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");
        TEST_RESULT_VOID(
            storageRemoveP(
                storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strZ(segment)), .errorOnMissing = true),
            "remove ok");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strZ(segment))), bufNew(0));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, true, true), AssertError,
            "status file '000000010000000100000001.error' has no content");

        storagePutP(
            storageNewWriteP(storageSpoolWrite(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strZ(segment))),
            BUFSTRDEF("25\nmessage"));
        TEST_ERROR(archiveAsyncStatus(archiveModePush, segment, true, true), AssertError, "message");

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false, true), false, "suppress error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutP(
            storageNewWriteP(storageSpoolWrite(), STRDEF(STORAGE_SPOOL_ARCHIVE_OUT "/global.error")),
            BUFSTRDEF("102\nexecute error"));

        TEST_ERROR(archiveAsyncStatus(archiveModePush, STRDEF("anyfile"), true, true), ExecuteError, "execute error");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveAsyncStatusErrorWrite() and archiveAsyncStatusOkWrite()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--spool-path=" TEST_PATH);
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--" CFGOPT_ARCHIVE_ASYNC);
        harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleAsync, argList);

        const String *walSegment = STRDEF("000000010000000100000001");

        TEST_RESULT_VOID(
            archiveAsyncStatusErrorWrite(archiveModeGet, walSegment, 25, STRDEF("error message")), "write error");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("archive/db/in/000000010000000100000001.error")))),
            "25\nerror message", "check error");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, STRDEF("archive/db/in/000000010000000100000001.error"), .errorOnMissing = true),
            "remove error");

        TEST_RESULT_VOID(
            archiveAsyncStatusErrorWrite(archiveModeGet, NULL, 25, STRDEF("global error message")), "write global error");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("archive/db/in/global.error")))),
            "25\nglobal error message", "check global error");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, STRDEF("archive/db/in/global.error"), .errorOnMissing = true),
            "remove global error");

        TEST_RESULT_VOID(
            archiveAsyncStatusOkWrite(archiveModeGet, walSegment, NULL), "write ok file");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("archive/db/in/000000010000000100000001.ok")))),
            "", "check ok");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, STRDEF("archive/db/in/000000010000000100000001.ok"), .errorOnMissing = true),
            "remove ok");

        TEST_RESULT_VOID(
            archiveAsyncStatusOkWrite(archiveModeGet, walSegment, STRDEF("WARNING")), "write ok file with warning");
        TEST_RESULT_STR_Z(
            strNewBuf(storageGetP(storageNewReadP(storageTest, STRDEF("archive/db/in/000000010000000100000001.ok")))),
            "0\nWARNING", "check ok warning");
        TEST_RESULT_VOID(
            storageRemoveP(storageTest, STRDEF("archive/db/in/000000010000000100000001.ok"), .errorOnMissing = true),
            "remove ok");
    }

    // *****************************************************************************************************************************
    if (testBegin("walIsPartial()"))
    {
        TEST_RESULT_BOOL(walIsPartial(STRDEF("000000010000000100000001")), false, "not partial");
        TEST_RESULT_BOOL(walIsPartial(STRDEF("FFFFFFFFFFFFFFFFFFFFFFFF.partial")), true, "partial");
    }

    // *****************************************************************************************************************************
    if (testBegin("walIsSegment()"))
    {
        TEST_RESULT_BOOL(walIsSegment(STRDEF("000000010000000100000001")), true, "wal segment");
        TEST_RESULT_BOOL(walIsSegment(STRDEF("FFFFFFFFFFFFFFFFFFFFFFFF.partial")), true, "partial wal segment");
        TEST_RESULT_BOOL(walIsSegment(STRDEF("0000001A.history")), false, "history file");
    }

    // *****************************************************************************************************************************
    if (testBegin("walPath()"))
    {
        const String *pgPath = storagePathP(storageTest, STRDEF("pg"));
        storagePathCreateP(storageTest, pgPath);

        TEST_RESULT_STR_Z(walPath(STRDEF("/absolute/path"), pgPath, STRDEF("test")), "/absolute/path", "absolute path");

        THROW_ON_SYS_ERROR(chdir(strZ(pgPath)) != 0, PathMissingError, "unable to chdir()");
        TEST_RESULT_STR(
            walPath(STRDEF("relative/path"), pgPath, STRDEF("test")), strNewFmt("%s/relative/path", strZ(pgPath)), "relative path");

        const String *pgPathLink = storagePathP(storageTest, STRDEF("pg-link"));
        THROW_ON_SYS_ERROR_FMT(
            symlink(strZ(pgPath), strZ(pgPathLink)) == -1, FileOpenError, "unable to create symlink '%s' to '%s'", strZ(pgPath),
            strZ(pgPathLink));

        THROW_ON_SYS_ERROR(chdir(strZ(pgPath)) != 0, PathMissingError, "unable to chdir()");
        TEST_RESULT_STR(
            walPath(STRDEF("relative/path"), pgPathLink, STRDEF("test")), strNewFmt("%s/relative/path", strZ(pgPathLink)),
            "relative path");

        THROW_ON_SYS_ERROR(chdir("/") != 0, PathMissingError, "unable to chdir()");
        TEST_ERROR(
            walPath(STRDEF("relative/path"), pgPathLink, STRDEF("test")), OptionInvalidValueError,
            "PostgreSQL working directory '/' is not the same as option pg1-path '" TEST_PATH "/pg-link'\n"
                "HINT: is the PostgreSQL data_directory configured the same as the pg1-path option?");

        TEST_ERROR(
            walPath(STRDEF("relative/path"), NULL, STRDEF("test")), OptionRequiredError,
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
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo-path=" TEST_PATH);
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(cfgCmdArchiveGet, argList);

        TEST_RESULT_STR(walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678"), 0), NULL, "no path");

        storagePathCreateP(storageTest, STRDEF("archive/db/9.6-2/1234567812345678"));
        TEST_RESULT_STR(
            walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678"), 0), NULL, "no segment");
        TEST_ERROR(
            walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678"), 100), ArchiveTimeoutError,
            "WAL segment 123456781234567812345678 was not archived before the 100ms timeout\n"
            "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: check the PostgreSQL server log for errors.\n"
            "HINT: run the 'start' command if the stanza was previously stopped.");

        // Check timeout by making the wal segment appear after 250ms
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                sleepMSec(250);

                storagePutP(
                    storageNewWriteP(
                        storageTest,
                        STRDEF(
                            "archive/db/9.6-2/1234567812345678/123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")),
                    NULL);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                TEST_RESULT_STR_Z(
                    walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678"), 1000),
                    "123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "found segment");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        storagePutP(
            storageNewWriteP(
                storageTest,
                STRDEF("archive/db/9.6-2/1234567812345678/123456781234567812345678-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.gz")),
            NULL);

        TEST_ERROR(
            walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678"), 0),
            ArchiveDuplicateError,
            "duplicates found in archive for WAL segment 123456781234567812345678:"
                " 123456781234567812345678-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                ", 123456781234567812345678-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.gz"
                "\nHINT: are multiple primaries archiving to this stanza?");

        TEST_RESULT_STR(
            walSegmentFind(storageRepo(), STRDEF("9.6-2"), STRDEF("123456781234567812345678.partial"), 0), NULL,
            "did not find partial segment");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentNext()"))
    {
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("000000010000000100000001"), 16 * 1024 * 1024, PG_VERSION_10), "000000010000000100000002",
            "get next");
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_93), "0000000100000001000000FF",
            "get next");
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("0000009900000001000000FF"), 16 * 1024 * 1024, PG_VERSION_93), "000000990000000200000000",
            "get next overflow >= 9.3");
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_92), "000000010000000200000000",
            "get next overflow < 9.3");
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("000000010000000100000003"), 1024 * 1024 * 1024, PG_VERSION_11), "000000010000000200000000",
            "get next overflow >= 11/1GB");
        TEST_RESULT_STR_Z(
            walSegmentNext(STRDEF("000000010000006700000FFF"), 1024 * 1024, PG_VERSION_11), "000000010000006800000000",
            "get next overflow >= 11/1MB");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentRange()"))
    {
        TEST_RESULT_STRLST_Z(
            walSegmentRange(STRDEF("000000010000000100000000"), 16 * 1024 * 1024, PG_VERSION_92, 1), "000000010000000100000000\n",
            "get single");
        TEST_RESULT_STRLST_Z(
            walSegmentRange(STRDEF("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_92, 4),
            "0000000100000001000000FD\n0000000100000001000000FE\n000000010000000200000000\n000000010000000200000001\n",
            "get range < 9.3");
        TEST_RESULT_STRLST_Z(
            walSegmentRange(STRDEF("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_93, 4),
            "0000000100000001000000FD\n0000000100000001000000FE\n0000000100000001000000FF\n000000010000000200000000\n",
            "get range >= 9.3");
        TEST_RESULT_STRLST_Z(
            walSegmentRange(STRDEF("000000080000000A00000000"), 1024 * 1024 * 1024, PG_VERSION_11, 8),
            "000000080000000A00000000\n000000080000000A00000001\n000000080000000A00000002\n000000080000000A00000003\n"
            "000000080000000B00000000\n000000080000000B00000001\n000000080000000B00000002\n000000080000000B00000003\n",
            "get range >= 11/1GB");
        TEST_RESULT_STRLST_Z(
            walSegmentRange(STRDEF("000000070000000700000FFE"), 1024 * 1024, PG_VERSION_11, 4),
            "000000070000000700000FFE\n000000070000000700000FFF\n000000070000000800000000\n000000070000000800000001\n",
            "get range >= 11/1MB");
    }

    // *****************************************************************************************************************************
    if (testBegin("archiveIdComparator()"))
    {
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("archiveId comparator sorting");

        StringList *list = strLstComparatorSet(strLstNew(), archiveIdComparator);

        strLstAddZ(list, "10-4");
        strLstAddZ(list, "11-10");
        strLstAddZ(list, "9.6-1");

        TEST_RESULT_STRLST_Z(strLstSort(list, sortOrderAsc), "9.6-1\n10-4\n11-10\n", "sort ascending");

        strLstAddZ(list, "9.4-2");
        TEST_RESULT_STRLST_Z(strLstSort(list, sortOrderDesc), "11-10\n10-4\n9.4-2\n9.6-1\n", "sort descending");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
