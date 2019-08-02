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
        String *lockPath = strNewFmt("%s/lockpath", testPath());
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAdd(argList, strNewFmt("--lock-path=%s", strPtr(lockPath)));
        strLstAddZ(argList, "stop");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStop(), "no stanza, create stop file");
        StorageInfo info = {0};
        TEST_ASSIGN(info, storageInfoNP(storageTest, lockPath), "    get path info");
// CSHANG How come info.name is always NULL? TEST_RESULT_STR(strPtr(info.name), strPtr(lockPath), "path name");
        TEST_RESULT_INT(info.mode, 0770, "    check path mode");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s/all.stop", strPtr(lockPath))), true,
            "    all stop file created");
        TEST_ASSIGN(info, storageInfoNP(storageTest, strNewFmt("%s/all.stop", strPtr(lockPath))), "    get file info");
        TEST_RESULT_INT(info.mode, 0640, "    check file mode");
// TEST_RESULT_STR(strPtr(info.name), "all.stop", "    file name");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_RESULT_VOID(cmdStop(), "no stanza, stop file already exists");
        // harnessLogResult("P00   WARN: stop file already exists for all stanzas");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("lockpath/all.stop")), "remove stop file");
        TEST_RESULT_INT(system(strPtr(strNewFmt("sudo chmod 444 %s", strPtr(lockPath)))), 0, "change perms");
        TEST_ERROR_FMT(cmdStop(), FileOpenError,
            "unable to stat '%s/all.stop': [13] Permission denied", strPtr(lockPath));
        TEST_RESULT_VOID(storagePathRemoveP(storageTest, lockPath, .recurse = true, .errorOnMissing = true),
            "    remove the lock path");
// CSHANG If I comment out "WARN: stop file already exists for all stanzas" above this, then below "stanza, stop file already exists" test passes. If I do not, then it fails with
// EXPECTED VOID RESULT FROM STATEMENT: cmdStop()
//     BUT GOT FileWriteError: unable to write log to file: [9] Bad file descriptor
// when it tries to write the warning in LOG_WARN. I tried using --dev-test, but still errors. I tried using --log-level-test=trace but I  get common/log:logWrite:290:(test build required for parameters)
        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--stanza=db");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdStop(), "stanza, create stop file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s/db.stop", strPtr(lockPath))), true,
            "    stanza stop file created");
        StringList *lockPathList = NULL;
        TEST_ASSIGN(lockPathList, storageListP(storageTest, strNew("lockpath"), .errorOnMissing = true), "    get file list");
        TEST_RESULT_INT(strLstSize(lockPathList), 1, "    only file in lock path");
        TEST_RESULT_STR(strPtr(strLstGet(lockPathList, 0)), "db.stop", "    stanza stop exists");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(cmdStop(), "stanza, stop file already exists");
        harnessLogResult("P00   WARN: stop file already exists for stanza db");
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("lockpath/db.stop")), "    remove stop file");

        // -------------------------------------------------------------------------------------------------------------------------
        strLstAddZ(argList, "--force");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        TEST_RESULT_VOID(cmdStop(), "stanza, create stop file, force");
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("lockpath/db.stop")), "    remove stop file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/empty.lock", strPtr(lockPath))), NULL),
            "create empty lock file");
        TEST_RESULT_VOID(cmdStop(), "    stanza, create stop file, force - empty lock file");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s/db.stop", strPtr(lockPath))), true,
            "    stanza stop file created");
        TEST_RESULT_BOOL(storageExistsNP(storageTest, strNewFmt("%s/empty.lock", strPtr(lockPath))), false,
            "    lock file was removed");

        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, strNewFmt("%s/empty.lock", strPtr(lockPath))), NULL),
            "create empty lock file");
        TEST_RESULT_VOID(storageRemoveNP(storageTest, strNew("lockpath/db.stop")), "    remove stop file");
        int lockHandle = open(strPtr(strNewFmt("%s/empty.lock", strPtr(lockPath))), O_RDONLY, 0);
        TEST_RESULT_BOOL(lockHandle != -1, true, "    file handle aquired");
        TEST_RESULT_INT(flock(lockHandle, LOCK_EX | LOCK_NB), 0, "    lock the file"); // CSHANG Why does this not lock the file?
        TEST_RESULT_VOID(cmdStop(), "    stanza, create stop file, force - empty lock file, processId == NULL");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
