/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include <stdlib.h>

#include "config/load.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("walStatus()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "archive-push");
        cfgLoad(strLstSize(argList), strLstPtr(argList));
        logInit(logLevelInfo, logLevelOff, false);

        // -------------------------------------------------------------------------------------------------------------------------
        String *segment = strNew("000000010000000100000001");

        TEST_RESULT_BOOL(walStatus(segment, false), false, "directory and status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);

        TEST_RESULT_BOOL(walStatus(segment, false), false, "status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), bufNewStr(strNew(BOGUS_STR)));
        TEST_ERROR(walStatus(segment, false), FormatError, "000000010000000100000001.ok content must have at least two lines");

        storagePut(
            storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), bufNewStr(strNew(BOGUS_STR "\n")));
        TEST_ERROR(walStatus(segment, false), FormatError, "000000010000000100000001.ok message must be > 0");

        storagePut(
            storageSpool(),
            strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), bufNewStr(strNew(BOGUS_STR "\nmessage")));
        TEST_ERROR(walStatus(segment, false), FormatError, "unable to convert str 'BOGUS' to int");

        storagePut(
            storageSpool(),
            strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), bufNewStr(strNew("0\nwarning")));
        TEST_RESULT_BOOL(walStatus(segment, false), true, "ok file with warning");
        testLogResult("P00   WARN: warning");

        storagePut(
            storageSpool(),
            strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)), bufNewStr(strNew("25\nerror")));
        TEST_RESULT_BOOL(walStatus(segment, false), true, "error status renamed to ok");
        testLogResult(
            "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment)), bufNewStr(strNew("")));
        TEST_ERROR(
            walStatus(segment, false), AssertError,
            strPtr(
                strNewFmt(
                    "multiple status files found in '%s/archive/db/out' for WAL segment '000000010000000100000001'", testPath())));

        unlink(strPtr(storagePath(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)))));
        TEST_ERROR(walStatus(segment, true), AssertError, "status file '000000010000000100000001.error' has no content");

        storagePut(
            storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment)), bufNewStr(strNew("25\nmessage")));
        TEST_ERROR(walStatus(segment, true), AssertError, "message");

        TEST_RESULT_BOOL(walStatus(segment, false), false, "suppress error");

        unlink(strPtr(storagePath(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment)))));
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdArchivePush()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "archive-push");
        cfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchivePush(), ParamRequiredError, "WAL segment to push required");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "000000010000000100000001");
        cfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchivePush(), AssertError, "archive-push in C does not support synchronous mode");

        // Test that a bogus perl bin generates the correct errors
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        cfgLoad(strLstSize(argList), strLstPtr(argList));
        logInit(logLevelInfo, logLevelOff, false);

        TRY_BEGIN()
        {
            cmdArchivePush();

            THROW(AssertError, "error should have been thrown");    // {uncoverable - test should not get here}
        }
        CATCH_ANY()
        {
            // Check expected error on the parent process
            TEST_RESULT_INT(errorCode(), errorTypeCode(&AssertError), "error code matches after failed Perl exec");
            TEST_RESULT_STR(errorMessage(), "perl exited with error 37", "error message matches after failed Perl exec");
        }
        TRY_END();

        // Make sure the process times out when there is nothing to archive
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        cfgLoad(strLstSize(argList), strLstPtr(argList));
        logInit(logLevelInfo, logLevelOff, false);

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");

        // Write out a bogus .error file to make sure it is ignored on the first loop
        // -------------------------------------------------------------------------------------------------------------------------
        String *errorFile = storagePath(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error"));

        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);
        storagePut(storageSpool(), errorFile, bufNewStr(strNew("25\n" BOGUS_STR)));

        TEST_ERROR(cmdArchivePush(), AssertError, BOGUS_STR);

        unlink(strPtr(errorFile));

        // Write out a valid ok file and test for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok"), bufNewStr(strNew("")));

        TEST_RESULT_VOID(cmdArchivePush(), "successful push");
        testLogResult("P00   INFO: pushed WAL segment 000000010000000100000001 asynchronously");
    }
}
