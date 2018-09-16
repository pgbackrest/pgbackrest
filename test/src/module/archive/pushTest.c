/***********************************************************************************************************************************
Test Archive Push Command
***********************************************************************************************************************************/
#include "storage/driver/posix/storage.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

    // *****************************************************************************************************************************
    if (testBegin("cmdArchivePush()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--archive-timeout=1");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "archive-push");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchivePush(), ParamRequiredError, "WAL segment to push required");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "000000010000000100000001");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdArchivePush(), AssertError, "archive-push in C does not support synchronous mode");

        // Make sure the process times out when there is nothing to archive
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateNP(storageTest, strNewFmt("%s/db/archive_status", testPath()));

        strLstAdd(argList, strNewFmt("--spool-path=%s", testPath()));
        strLstAddZ(argList, "--archive-async");
        strLstAdd(argList, strNewFmt("--log-path=%s", testPath()));
        strLstAdd(argList, strNewFmt("--log-level-file=debug"));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/db", testPath()));
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");

        // Wait for the lock to release
        lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true);
        lockRelease(true);

        // Write out a bogus .error file to make sure it is ignored on the first loop
        // -------------------------------------------------------------------------------------------------------------------------
        // Remove the archive status path so async will error and not overwrite the bogus error file
        storagePathRemoveNP(storageTest, strNewFmt("%s/db/archive_status", testPath()));

        String *errorFile = storagePathNP(storageSpool(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.error"));
        storagePutNP(storageNewWriteNP(storageSpoolWrite(), errorFile), bufNewStr(strNew("25\n" BOGUS_STR)));

        TEST_ERROR(cmdArchivePush(), AssertError, BOGUS_STR);

        storageRemoveP(storageTest, errorFile, .errorOnMissing = true);

        // Write out a valid ok file and test for success
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok")),
            bufNewStr(strNew("")));

        TEST_RESULT_VOID(cmdArchivePush(), "successful push");
        harnessLogResult("P00   INFO: pushed WAL segment 000000010000000100000001 asynchronously");

        storageRemoveP(
            storageSpoolWrite(), strNew(STORAGE_SPOOL_ARCHIVE_OUT "/000000010000000100000001.ok"), .errorOnMissing = true);

        // Make sure the process times out when there is nothing to archive and it can't get a lock.  This test MUST go last since
        // the lock is lost and cannot be closed by the main process.
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            lockAcquire(cfgOptionStr(cfgOptLockPath), cfgOptionStr(cfgOptStanza), cfgLockType(), 30, true), "acquire lock");
        TEST_RESULT_VOID(lockClear(true), "clear lock");

        TEST_ERROR(
            cmdArchivePush(), ArchiveTimeoutError,
            "unable to push WAL segment '000000010000000100000001' asynchronously after 1 second(s)");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
