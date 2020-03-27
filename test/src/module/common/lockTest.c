/***********************************************************************************************************************************
Test Lock Handler
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/posix/storage.h"

#include "common/harnessFork.h"

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
    if (testBegin("lockAcquireFile() and lockReleaseFile()"))
    {
        String *archiveLock = strNewFmt("%s/main-archive" LOCK_FILE_EXT, testPath());
        int lockHandleTest = -1;

        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(archiveLock)))), 0, "touch lock file");
        TEST_ASSIGN(lockHandleTest, lockAcquireFile(archiveLock, 0, true), "get lock");
        TEST_RESULT_BOOL(lockHandleTest != -1, true, "lock succeeds");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), true, "lock file was created");
        TEST_ERROR(lockAcquireFile(archiveLock, 0, true), LockAcquireError,
            strPtr(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strPtr(archiveLock))));

        TEST_ERROR(
            lockAcquireFile(archiveLock, 0, true), LockAcquireError,
            strPtr(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strPtr(archiveLock))));
        TEST_RESULT_BOOL(lockAcquireFile(archiveLock, 0, false) == -1, true, "lock is already held");

        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, archiveLock), "release lock");

        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, archiveLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), false, "lock file was removed");
        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, archiveLock), "release lock again without error");

        // -------------------------------------------------------------------------------------------------------------------------
        String *subPathLock = strNewFmt("%s/sub1/sub2/db-backup" LOCK_FILE_EXT, testPath());

        TEST_ASSIGN(lockHandleTest, lockAcquireFile(subPathLock, 0, true), "get lock in subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), true, "lock file was created");
        TEST_RESULT_BOOL(lockHandleTest != -1, true, "lock succeeds");
        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, subPathLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), false, "lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        String *dirLock = strNewFmt("%s/dir" LOCK_FILE_EXT, testPath());

        TEST_RESULT_INT(system(strPtr(strNewFmt("mkdir -p 750 %s", strPtr(dirLock)))), 0, "create dirtest.lock dir");

        TEST_ERROR(
            lockAcquireFile(dirLock, 0, true), LockAcquireError,
            strPtr(strNewFmt("unable to acquire lock on file '%s': Is a directory", strPtr(dirLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *noPermLock = strNewFmt("%s/noperm/noperm", testPath());
        TEST_RESULT_INT(system(strPtr(strNewFmt("mkdir -p 750 %s", strPtr(strPath(noPermLock))))), 0, "create noperm dir");
        TEST_RESULT_INT(system(strPtr(strNewFmt("chmod 000 %s", strPtr(strPath(noPermLock))))), 0, "chmod noperm dir");

        TEST_ERROR(
            lockAcquireFile(noPermLock, 100, true), LockAcquireError,
            strPtr(
                strNewFmt(
                    "unable to acquire lock on file '%s': Permission denied\n"
                        "HINT: does the user running pgBackRest have permissions on the '%s' file?",
                    strPtr(noPermLock), strPtr(noPermLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *backupLock = strNewFmt("%s/main-backup" LOCK_FILE_EXT, testPath());

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_INT_NE(lockAcquireFile(backupLock, 0, true), -1, "lock on fork");
                sleepMSec(500);
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                sleepMSec(250);
                TEST_ERROR(
                    lockAcquireFile(backupLock, 0, true),
                    LockAcquireError,
                    strPtr(
                        strNewFmt(
                            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                            "HINT: is another pgBackRest process running?", strPtr(backupLock))));
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("lockAcquire(), lockRelease(), and lockClear()"))
    {
        String *stanza = strNew("test");
        String *lockPath = strNew(testPath());
        String *archiveLockFile = strNewFmt("%s/%s-archive" LOCK_FILE_EXT, testPath(), strPtr(stanza));
        String *backupLockFile = strNewFmt("%s/%s-backup" LOCK_FILE_EXT, testPath(), strPtr(stanza));
        int lockHandleTest = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(lockRelease(true), AssertError, "no lock is held by this process");
        TEST_RESULT_BOOL(lockRelease(false), false, "release when there is no lock");

        TEST_ERROR(lockClear(true), AssertError, "no lock is held by this process");
        TEST_RESULT_BOOL(lockClear(false), false, "release when there is no lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockHandleTest, lockAcquireFile(archiveLockFile, 0, true), "archive lock by file");
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, lockTypeArchive, 0, false), false, "archive already locked");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, lockTypeArchive, 0, true), LockAcquireError,
            strPtr(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strPtr(archiveLockFile))));
        TEST_ERROR(
            lockAcquire(lockPath, stanza, lockTypeAll, 0, true), LockAcquireError,
            strPtr(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strPtr(archiveLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, archiveLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, lockTypeArchive, 0, true), true, "archive lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_ERROR(lockAcquire(lockPath, stanza, lockTypeArchive, 0, false), AssertError, "lock is already held by this process");
        TEST_RESULT_VOID(lockRelease(true), "release archive lock");

        // // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockHandleTest, lockAcquireFile(backupLockFile, 0, true), "backup lock by file");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, lockTypeBackup, 0, true), LockAcquireError,
            strPtr(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strPtr(backupLockFile))));
        TEST_ERROR(
            lockAcquire(lockPath, stanza, lockTypeAll, 0, true), LockAcquireError,
            strPtr(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strPtr(backupLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, archiveLockFile), "release lock");
        TEST_RESULT_VOID(lockReleaseFile(lockHandleTest, backupLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file was created");
        TEST_ERROR(
            lockAcquire(lockPath, stanza, lockTypeAll, 0, false), AssertError,
            "assertion 'failOnNoLock || lockType != lockTypeAll' failed");
        TEST_RESULT_VOID(lockRelease(true), "release all lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, lockTypeBackup, 0, true), true, "backup lock");

        lockHandleTest = lockHandle[lockTypeBackup];
        String *lockFileTest = strDup(lockFile[lockTypeBackup]);

        TEST_RESULT_VOID(lockClear(true), "clear backup lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file still exists");
        lockReleaseFile(lockHandleTest, lockFileTest);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(lockPath, stanza, lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_VOID(lockClear(true), "clear all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file still exists");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file still exists");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
