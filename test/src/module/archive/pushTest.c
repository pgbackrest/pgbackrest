/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include <stdlib.h>

#include "config/load.h"
#include "version.h"

#include "common/harnessConfig.h"

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
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // -------------------------------------------------------------------------------------------------------------------------
        String *segment = strNew("000000010000000100000001");

        TEST_RESULT_BOOL(walStatus(segment, false), false, "directory and status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);

        TEST_RESULT_BOOL(walStatus(segment, false), false, "status file not present");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR)));
        TEST_ERROR(walStatus(segment, false), FormatError, "000000010000000100000001.ok content must have at least two lines");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR "\n")));
        TEST_ERROR(walStatus(segment, false), FormatError, "000000010000000100000001.ok message must be > 0");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew(BOGUS_STR "\nmessage")));
        TEST_ERROR(walStatus(segment, false), FormatError, "unable to convert str 'BOGUS' to int");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew("0\nwarning")));
        TEST_RESULT_BOOL(walStatus(segment, false), true, "ok file with warning");
        testLogResult("P00   WARN: warning");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment))),
            bufNewStr(strNew("25\nerror")));
        TEST_RESULT_BOOL(walStatus(segment, false), true, "error status renamed to ok");
        testLogResult(
            "P00   WARN: WAL segment '000000010000000100000001' was not pushed due to error [25] and was manually skipped: error");

        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))),
            bufNewStr(strNew("")));
        TEST_ERROR(
            walStatus(segment, false), AssertError,
            strPtr(
                strNewFmt(
                    "multiple status files found in '%s/archive/db/out' for WAL segment '000000010000000100000001'", testPath())));

        unlink(strPtr(storagePathNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.ok", strPtr(segment)))));
        TEST_ERROR(walStatus(segment, true), AssertError, "status file '000000010000000100000001.error' has no content");

        storagePutNP(
            storageNewWriteNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment))),
            bufNewStr(strNew("25\nmessage")));
        TEST_ERROR(walStatus(segment, true), AssertError, "message");

        TEST_RESULT_BOOL(walStatus(segment, false), false, "suppress error");

        unlink(strPtr(storagePathNP(storageSpool(), strNewFmt(STORAGE_SPOOL_ARCHIVE_OUT "/%s.error", strPtr(segment)))));
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

        // Make sure the process times out when there is nothing to archive
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAddZ(argList, "--log-level-console=off");
        strLstAddZ(argList, "--log-level-stderr=off");
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");

        // Make sure the process times out when there is nothing to archive and it can't get a lock
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true), "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");

        // Write out a bogus .error file to make sure it is ignored on the first loop
        // -------------------------------------------------------------------------------------------------------------------------
        String *errorFile = storagePathNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error"));

        mkdir(strPtr(strNewFmt("%s/archive", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db", testPath())), 0750);
        mkdir(strPtr(strNewFmt("%s/archive/db/out", testPath())), 0750);
        storagePutNP(storageNewWriteNP(storageSpool(), errorFile), bufNewStr(strNew("25\n" BOGUS_STR)));

        TEST_ERROR(cmdArchivePush(), AssertError, BOGUS_STR);

        unlink(strPtr(errorFile));

        // Write out a valid ok file and test for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok")),
            bufNewStr(strNew("")));

        TEST_RESULT_VOID(cmdArchivePush(), "successful push");
        testLogResult("P00   INFO: pushed WAL segment 000000010000000100000001 asynchronously");
    }
}
