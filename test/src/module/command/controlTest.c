/***********************************************************************************************************************************
Test Command Control
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("lockStopFileName()"))
    {
        // Load configuration so lock path is set
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--lock-path=/path/to/lock");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(strPtr(lockStopFileName(NULL)), "/path/to/lock/all.stop", "stop file for all stanzas");
        TEST_RESULT_STR(strPtr(lockStopFileName(strNew("db"))), "/path/to/lock/db.stop", "stop file for on stanza");
    }

    // *****************************************************************************************************************************
    if (testBegin("lockStopTest()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(false), "no stop files without stanza");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(false), "no stop files with stanza");
        TEST_RESULT_BOOL(lockStopTest(true), false, "no stop file and is expected for the stanza");

        storagePutNP(storageNewWriteNP(storageTest, strNew("all.stop")), NULL);
        TEST_ERROR(lockStopTest(false), StopError, "stop file exists for all stanzas");
        TEST_RESULT_BOOL(lockStopTest(true), false, "stop file exists for all stanzas but not the stanza");

        storagePutNP(storageNewWriteNP(storageTest, strNew("db.stop")), NULL);
        TEST_ERROR(lockStopTest(false), StopError, "stop file exists for stanza db");
        TEST_RESULT_BOOL(lockStopTest(true), true, "stop file exists and is expected for the stanza");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
