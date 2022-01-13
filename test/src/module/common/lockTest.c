/***********************************************************************************************************************************
Test Lock Handler
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/posix/storage.h"

#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("lockAcquireFile() and lockReleaseFile()"))
    {
        const String *archiveLock = STRDEF(TEST_PATH "/main-archive" LOCK_FILE_EXT);
        int lockFdTest = -1;

        HRN_SYSTEM_FMT("touch %s", strZ(archiveLock));
        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLock, STRDEF("1-test"), 0, true), "get lock");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), true, "lock file was created");
        TEST_ERROR(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strZ(archiveLock))));
        TEST_RESULT_BOOL(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, false) == -1, true, "lock is already held");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire file lock on the same exec-id");

        TEST_RESULT_INT(lockAcquireFile(archiveLock, STRDEF("1-test"), 0, true), -2, "allow lock with same exec id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail file lock on the same exec-id when lock file is empty");

        HRN_SYSTEM_FMT("echo '' > %s", strZ(archiveLock));

        TEST_ERROR(lockAcquireFile(archiveLock, STRDEF("2-test"), 0, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strZ(archiveLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock");

        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), false, "lock file was removed");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLock), "release lock again without error");

        // -------------------------------------------------------------------------------------------------------------------------
        const String *subPathLock = STRDEF(TEST_PATH "/sub1/sub2/db-backup" LOCK_FILE_EXT);

        TEST_ASSIGN(lockFdTest, lockAcquireFile(subPathLock, STRDEF("1-test"), 0, true), "get lock in subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), true, "lock file was created");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, subPathLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), false, "lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        const String *dirLock = STRDEF(TEST_PATH "/dir" LOCK_FILE_EXT);

        HRN_SYSTEM_FMT("mkdir -p 750 %s", strZ(dirLock));

        TEST_ERROR(
            lockAcquireFile(dirLock, STRDEF("1-test"), 0, true), LockAcquireError,
            strZ(strNewFmt("unable to acquire lock on file '%s': Is a directory", strZ(dirLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        const String *noPermLock = STRDEF(TEST_PATH "/noperm/noperm");
        HRN_SYSTEM_FMT("mkdir -p 750 %s", strZ(strPath(noPermLock)));
        HRN_SYSTEM_FMT("chmod 000 %s", strZ(strPath(noPermLock)));

        TEST_ERROR(
            lockAcquireFile(noPermLock, STRDEF("1-test"), 100, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Permission denied\n"
                        "HINT: does the user running pgBackRest have permissions on the '%s' file?",
                    strZ(noPermLock), strZ(noPermLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        const String *backupLock = STRDEF(TEST_PATH "/main-backup" LOCK_FILE_EXT);

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(lockAcquireFile(backupLock, STRDEF("1-test"), 0, true), -1, "lock on fork");

                // Notify parent that lock has been acquired
                HRN_FORK_CHILD_NOTIFY_PUT();

                // Wait for parent to allow release lock
                HRN_FORK_CHILD_NOTIFY_GET();
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Wait for child to acquire lock
                HRN_FORK_PARENT_NOTIFY_GET(0);

                TEST_ERROR(
                    lockAcquireFile(backupLock, STRDEF("2-test"), 0, true),
                    LockAcquireError,
                    strZ(
                        strNewFmt(
                            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                            "HINT: is another pgBackRest process running?", strZ(backupLock))));

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("lockAcquire(), lockRelease()"))
    {
        const String *stanza = STRDEF("test");
        String *archiveLockFile = strNewFmt(TEST_PATH "/%s-archive" LOCK_FILE_EXT, strZ(stanza));
        String *backupLockFile = strNewFmt(TEST_PATH "/%s-backup" LOCK_FILE_EXT, strZ(stanza));
        int lockFdTest = -1;
        const Buffer *buffer = NULL;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(lockRelease(true), AssertError, "no lock is held by this process");
        TEST_RESULT_BOOL(lockRelease(false), false, "release when there is no lock");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 85.58389), false, "no lockMemContext");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLockFile, STRDEF("1-test"), 0, true), "archive lock by file");
        TEST_RESULT_BOOL(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeArchive, 0, false), false, "archive already locked");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeArchive, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(archiveLockFile))));
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(archiveLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeArchive, 0, true), true, "archive lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeArchive, 0, false), AssertError,
            "lock is already held by this process");
        TEST_RESULT_VOID(lockRelease(true), "release archive lock");

        // // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(lockFdTest, lockAcquireFile(backupLockFile, STRDEF("1-test"), 0, true), "backup lock by file");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeBackup, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(backupLockFile))));
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            strZ(strNewFmt(
                "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                "HINT: is another pgBackRest process running?", strZ(backupLockFile))));
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLockFile), "release lock");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, backupLockFile), "release lock");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file was created");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeAll, 0, false), AssertError,
            "assertion 'failOnNoLock || lockType != lockTypeAll' failed");
        TEST_RESULT_VOID(lockRelease(true), "release all locks");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), false, "archive lock file was removed");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), false, "backup lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file was created");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 0.0), true,
            "backup lock, lock file write 1 succeeded");
        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test-backup.lock"))), "get file contents");
        const StringList *parse = strLstNewSplitZ(strNewBuf(buffer), LF_Z);
        TEST_RESULT_BOOL(
            strEq(varStr(kvGet(jsonToKv(strLstGet(parse,1)), varNewStr(STRDEF("execId")))), STRDEF("1-test")), true,
            "verify 1 execId");
        TEST_RESULT_BOOL(
            strEq(varStr(kvGet(jsonToKv(strLstGet(parse,1)), varNewStr(STRDEF("percentageComplete")))), STRDEF("0.00")), true,
            "verify percentageComplete 0.00");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 50.85938), true,
            "backup lock, lock file write 2 succeeded");
        TEST_ASSIGN(buffer, storageGetP(storageNewReadP(storageTest, STRDEF(TEST_PATH "/test-backup.lock"))), "get file contents");
        parse = strLstNewSplitZ(strNewBuf(buffer), LF_Z);
        TEST_RESULT_BOOL(
            strEq(varStr(kvGet(jsonToKv(strLstGet(parse,1)), varNewStr(STRDEF("execId")))), STRDEF("1-test")), true,
            "verify 2 execId");
        TEST_RESULT_BOOL(
            strEq(varStr(kvGet(jsonToKv(strLstGet(parse,1)), varNewStr(STRDEF("percentageComplete")))), STRDEF("50.86")), true,
            "verify percentageComplete 50.86");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeAll, 0, false), AssertError,
            "assertion 'failOnNoLock || lockType != lockTypeAll' failed");
        TEST_RESULT_VOID(lockRelease(true), "release all locks");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 0.0), false, "no locks, lock file write failed");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), false, "archive lock file was removed");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), false, "backup lock file was removed");

        // Test with only archive lock
        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeArchive, 0, true), true, "archive lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 0.0), false, "archive lock, lock file write noop");
        TEST_RESULT_VOID(lockRelease(true), "release all locks");

        // Test with all lock
        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "all lock file was created");
        TEST_RESULT_BOOL(
            writeLockPercentageComplete(TEST_PATH_STR, stanza, STRDEF("1-test"), 0.0), true, "all lock, lock file write succeeded");
        TEST_RESULT_VOID(lockRelease(true), "release all locks");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire lock on the same exec-id and release");

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock");

        // Make it look there is no lock
        lockFdTest = lockFd[lockTypeBackup];
        String *lockFileTest = strDup(lockFile[lockTypeBackup]);
        lockTypeHeld = lockTypeNone;

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock again");
        TEST_RESULT_VOID(lockRelease(true), "release backup lock");

        // Release lock manually
        lockReleaseFile(lockFdTest, lockFileTest);
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
