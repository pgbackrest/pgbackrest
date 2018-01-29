/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include <stdlib.h>

#include "config/load.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void testRun()
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
        int processId = getpid();

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
        String *perlBin = strNewFmt("%s/perl-test.sh", testPath());

        strLstAdd(argList, strNewFmt("--perl-bin=%s", strPtr(perlBin)));
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        cfgLoad(strLstSize(argList), strLstPtr(argList));
        logInit(logLevelInfo, logLevelOff, false);

        TRY_BEGIN()
        {
            cmdArchivePush();

            THROW(AssertError, "error should have been thrown");    // {uncoverable - test should not get here}
        }
        CATCH_ANY()
        {
            // Exit with error if this is the child process
            if (getpid() != processId)
                exit(errorCode());

            // Check expected error on the parent process
            TEST_RESULT_INT(errorCode(), errorTypeCode(&AssertError), "error code matches after failed Perl exec");
            TEST_RESULT_STR(errorMessage(), "perl exited with error 25", "error message matches after failed Perl exec");
        }
        TRY_END();

        // Write a blank script for the perl bin and make sure the process times out
        // -------------------------------------------------------------------------------------------------------------------------
        Storage *storage = storageNew(strNew(testPath()), 0750, 65536, NULL);
        storagePut(storage, perlBin, bufNewStr(strNew("")));

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");

        // Write out a bogus .error file to make sure it is ignored on the first loop.  The perl bin will write the real one when it
        // executes.
        // -------------------------------------------------------------------------------------------------------------------------
        String *errorFile = storagePath(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error"));

        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);
        storagePut(storageSpool(), errorFile, bufNewStr(strNew("")));

        storagePut(storage, perlBin, bufNewStr(strNewFmt(
            "set -e\n"
            "echo '25' > %s\n"
            "echo 'generic error message' >> %s\n",
            strPtr(errorFile), strPtr(errorFile))));

        TEST_ERROR(cmdArchivePush(), AssertError, "generic error message");

        unlink(strPtr(errorFile));

        // Modify script to write out a valid ok file
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(storage, perlBin, bufNewStr(strNewFmt(
            "set -e\n"
            "touch %s\n",
            strPtr(storagePath(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok"))))));

        TEST_RESULT_VOID(cmdArchivePush(), "successful push");
        testLogResult("P00   INFO: pushed WAL segment 000000010000000100000001 asynchronously");
    }
}
