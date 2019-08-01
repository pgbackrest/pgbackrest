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
    if (testBegin("lockStopTest(), cmdStart()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(), "no stop files without stanza");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - no stanza, no stop files");
        harnessLogResult("P00   WARN: stop file does not exist");

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, strNew("all.stop")), NULL), "create stop file");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - no stanza, stop file exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("all.stop")), false, "    stop file removed");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAdd(argList, strNewFmt("--lock-path=%s", testPath()));
        strLstAddZ(argList, "start");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(lockStopTest(), "no stop files with stanza");
        TEST_RESULT_VOID(cmdStart(), "    cmdStart - stanza, no stop files");
        harnessLogResult("P00   WARN: stop file does not exist for stanza db");

        storagePutNP(storageNewWriteNP(storageTest, strNew("all.stop")), NULL);
        TEST_ERROR(lockStopTest(), StopError, "stop file exists for all stanzas");

        storagePutNP(storageNewWriteNP(storageTest, strNew("db.stop")), NULL);
        TEST_ERROR(lockStopTest(), StopError, "stop file exists for stanza db");

        TEST_RESULT_VOID(cmdStart(), "cmdStart - stanza, stop file exists");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("db.stop")), false, "    stanza stop file removed");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("all.stop")), true, "    all stop file not removed");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdStop()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s/lockpath", testPath()));
        strLstAddZ(argList, "stop");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStop(), "no stanza, create stop file");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("%s/lockpath", testPath() )), "get path info");
        TEST_RESULT_INT(info.mode, 0770, "    check mode");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNew("lockpath/all.stop")), true, "    all stop file created");
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("%s/lockpath/all.stop", testPath() )), "get path info");
        TEST_RESULT_INT(info.mode, 0640, "    check mode");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
