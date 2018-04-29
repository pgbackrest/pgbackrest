/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
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
