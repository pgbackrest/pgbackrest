/***********************************************************************************************************************************
Test Archive Common
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("archiveAsyncStatus()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "archive-push");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

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
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR)));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), FormatError,
            "000000010000000100000001.ok content must have at least two lines");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR "\n")));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), FormatError, "000000010000000100000001.ok message must be > 0");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR "\nmessage")));
        TEST_ERROR(archiveAsyncStatus(archiveModePush, segment, false), FormatError, "unable to convert string 'BOGUS' to int");

        storagePutNP(storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))), NULL);
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "ok file");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew("0\nwarning")));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "ok file with warning");
        testLogResult("P00   WARN: warning");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew("25\nerror")));
        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), true, "error status renamed to ok");
        testLogResult(
            "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))),
            bufNewStr(strNew("")));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, false), AssertError,
            strPtr(
                strNewFmt(
                    "multiple status files found in '%s/archive/db/out' for WAL segment '000000010000000100000001'", testPath())));

        unlink(strPtr(storagePathNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)))));
        TEST_ERROR(
            archiveAsyncStatus(archiveModePush, segment, true), AssertError,
            "status file '000000010000000100000001.error' has no content");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))),
            bufNewStr(strNew("25\nmessage")));
        TEST_ERROR(archiveAsyncStatus(archiveModePush, segment, true), AssertError, "message");

        TEST_RESULT_BOOL(archiveAsyncStatus(archiveModePush, segment, false), false, "suppress error");

        unlink(strPtr(storagePathNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment)))));
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentNext()"))
    {
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("000000010000000100000001"), 16 * 1024 * 1024, PG_VERSION_10)),
            "000000010000000100000002", "get next");
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_93)),
            "0000000100000001000000FF", "get next");
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("0000009900000001000000FF"), 16 * 1024 * 1024, PG_VERSION_93)),
            "000000990000000200000000", "get next overflow >= 9.3");
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("0000000100000001000000FE"), 16 * 1024 * 1024, PG_VERSION_92)),
            "000000010000000200000000", "get next overflow < 9.3");
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("000000010000000100000003"), 1024 * 1024 * 1024, PG_VERSION_11)),
            "000000010000000200000000", "get next overflow >= 11/1GB");
        TEST_RESULT_STR(
            strPtr(walSegmentNext(strNew("000000010000006700000FFF"), 1024 * 1024, PG_VERSION_11)),
            "000000010000006800000000", "get next overflow >= 11/1MB");
    }

    // *****************************************************************************************************************************
    if (testBegin("walSegmentRange()"))
    {
        TEST_RESULT_STR(
            strPtr(strLstJoin(walSegmentRange(strNew("000000010000000100000000"), 16 * 1024 * 1024, PG_VERSION_92, 1), "|")),
            "000000010000000100000000", "get single");
        TEST_RESULT_STR(
            strPtr(strLstJoin(walSegmentRange(strNew("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_92, 4), "|")),
            "0000000100000001000000FD|0000000100000001000000FE|000000010000000200000000|000000010000000200000001",
            "get range < 9.3");
        TEST_RESULT_STR(
            strPtr(strLstJoin(walSegmentRange(strNew("0000000100000001000000FD"), 16 * 1024 * 1024, PG_VERSION_93, 4), "|")),
            "0000000100000001000000FD|0000000100000001000000FE|0000000100000001000000FF|000000010000000200000000",
            "get range >= 9.3");
        TEST_RESULT_STR(
            strPtr(strLstJoin(walSegmentRange(strNew("000000080000000A00000000"), 1024 * 1024 * 1024, PG_VERSION_11, 8), "|")),
            "000000080000000A00000000|000000080000000A00000001|000000080000000A00000002|000000080000000A00000003|"
            "000000080000000B00000000|000000080000000B00000001|000000080000000B00000002|000000080000000B00000003",
            "get range >= 11/1GB");
        TEST_RESULT_STR(
            strPtr(strLstJoin(walSegmentRange(strNew("000000070000000700000FFE"), 1024 * 1024, PG_VERSION_11, 4), "|")),
            "000000070000000700000FFE|000000070000000700000FFF|000000070000000800000000|000000070000000800000001",
            "get range >= 11/1MB");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
