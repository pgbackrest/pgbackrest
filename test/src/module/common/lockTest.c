/***********************************************************************************************************************************
Test Lock Handler
***********************************************************************************************************************************/
#include "common/time.h"
#include "storage/posix/storage.h"

#include "common/harnessFork.h"
#include "common/harnessStorage.h"

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

        lockLocal.execId = STRDEF("1-test");

        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLock, 0, true), "get lock");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLock), true, "lock file was created");

        lockLocal.file[lockTypeArchive].fd = lockFdTest;
        lockLocal.file[lockTypeArchive].name = strDup(archiveLock);
        TEST_RESULT_VOID(lockWriteData(lockTypeArchive), "write lock data");

        lockLocal.execId = STRDEF("2-test");

        TEST_ERROR(lockAcquireFile(archiveLock, 0, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?", strZ(archiveLock))));
        TEST_RESULT_BOOL(lockAcquireFile(archiveLock, 0, false) == -1, true, "lock is already held");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire file lock on the same exec-id");

        lockLocal.execId = STRDEF("1-test");

        TEST_RESULT_INT(lockAcquireFile(archiveLock, 0, true), -2, "allow lock with same exec id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail file lock on the same exec-id when lock file is empty");

        HRN_SYSTEM_FMT("echo '' > %s", strZ(archiveLock));

        TEST_ERROR(lockAcquireFile(archiveLock, 0, true), LockAcquireError,
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
        String *subPathLock = strNewZ(TEST_PATH "/sub1/sub2/db-backup" LOCK_FILE_EXT);

        TEST_ASSIGN(lockFdTest, lockAcquireFile(subPathLock, 0, true), "get lock in subpath");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), true, "lock file was created");
        TEST_RESULT_BOOL(lockFdTest != -1, true, "lock succeeds");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, subPathLock), "release lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, subPathLock), false, "lock file was removed");

        // -------------------------------------------------------------------------------------------------------------------------
        String *dirLock = strNewZ(TEST_PATH "/dir" LOCK_FILE_EXT);

        HRN_SYSTEM_FMT("mkdir -p 750 %s", strZ(dirLock));

        TEST_ERROR(
            lockAcquireFile(dirLock, 0, true), LockAcquireError,
            strZ(strNewFmt("unable to acquire lock on file '%s': Is a directory", strZ(dirLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *noPermLock = strNewZ(TEST_PATH "/noperm/noperm");

        HRN_SYSTEM_FMT("mkdir -p 750 %s", strZ(strPath(noPermLock)));
        HRN_SYSTEM_FMT("chmod 000 %s", strZ(strPath(noPermLock)));

        TEST_ERROR(
            lockAcquireFile(noPermLock, 100, true), LockAcquireError,
            strZ(
                strNewFmt(
                    "unable to acquire lock on file '%s': Permission denied\n"
                        "HINT: does the user running pgBackRest have permissions on the '%s' file?",
                    strZ(noPermLock), strZ(noPermLock))));

        // -------------------------------------------------------------------------------------------------------------------------
        String *backupLock = strNewZ(TEST_PATH "/main-backup" LOCK_FILE_EXT);

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_INT_NE(lockAcquireFile(backupLock, 0, true), -1, "lock on fork");

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

                lockLocal.execId = STRDEF("2-test");

                TEST_ERROR(
                    lockAcquireFile(backupLock, 0, true),
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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(lockRelease(true), AssertError, "no lock is held by this process");
        TEST_RESULT_BOOL(lockRelease(false), false, "release when there is no lock");

        // -------------------------------------------------------------------------------------------------------------------------
        lockLocal.execId = STRDEF("1-test");

        TEST_ASSIGN(lockFdTest, lockAcquireFile(archiveLockFile, 0, true), "archive lock by file");
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
        lockLocal.execId = STRDEF("1-test");

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeArchive, 0, true), true, "archive lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_ERROR(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeArchive, 0, false), AssertError,
            "lock is already held by this process");
        TEST_RESULT_VOID(lockRelease(true), "release archive lock");

        // // -------------------------------------------------------------------------------------------------------------------------
        lockLocal.execId = STRDEF("2-test");

        TEST_ASSIGN(lockFdTest, lockAcquireFile(backupLockFile, 0, true), "backup lock by file");

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
        lockLocal.execId = STRDEF("1-test");

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
        TEST_TITLE("acquire lock on the same exec-id and release");

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock");

        // Make it look there is no lock
        lockFdTest = lockLocal.file[lockTypeBackup].fd;
        String *lockFileTest = strDup(lockLocal.file[lockTypeBackup].name);
        lockLocal.held = lockTypeNone;

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock again");
        TEST_RESULT_VOID(lockRelease(true), "release backup lock");

        // Release lock manually
        lockReleaseFile(lockFdTest, lockFileTest);
    }

    // *****************************************************************************************************************************
    if (testBegin("lockRead*()"))
    {
        TEST_TITLE("missing lock file");

        TEST_RESULT_UINT(lockReadFileP(STRDEF(TEST_PATH "/missing.lock")).status, lockReadStatusMissing, "lock read");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unlocked file");

        HRN_STORAGE_PUT_EMPTY(storageTest, "unlocked.lock");
        TEST_RESULT_UINT(lockReadFileP(STRDEF(TEST_PATH "/unlocked.lock")).status, lockReadStatusUnlocked, "lock read");
        TEST_STORAGE_LIST(storageTest, NULL, "unlocked.lock\n", .remove = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid locked file");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_BOOL(
                    lockAcquire(TEST_PATH_STR, STRDEF("test"), STRDEF("test"), lockTypeBackup, 0, true), true, "acquire lock");

                // Overwrite lock file with bogus data
                THROW_ON_SYS_ERROR_FMT(
                    lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK,
                    (uint64_t)0, strZ(lockLocal.file[lockTypeBackup].name));

                IoWrite *const write = ioFdWriteNewOpen(lockLocal.file[lockTypeBackup].name, lockLocal.file[lockTypeBackup].fd, 0);

                ioCopyP(ioBufferReadNewOpen(BUFSTRDEF("BOGUS")), write);
                ioWriteClose(write);

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

                TEST_RESULT_UINT(
                    lockReadFileP(STRDEF(TEST_PATH "/test-backup.lock"), .remove = true).status, lockReadStatusInvalid,
                    "lock read");
                TEST_STORAGE_LIST(storageTest, NULL, NULL);

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid locked file");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_BOOL(
                    lockAcquire(TEST_PATH_STR, STRDEF("test"), STRDEF("test"), lockTypeBackup, 0, true), true, "acquire lock");

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

                LockReadResult result = {0};
                TEST_ASSIGN(result, lockRead(TEST_PATH_STR, STRDEF("test"), lockTypeBackup), "lock read");

                TEST_RESULT_BOOL(result.data.processId != 0, true, "check processId");
                TEST_RESULT_STR_Z(result.data.execId, "test", "check execId");

                TEST_STORAGE_LIST(storageTest, NULL, "test-backup.lock\n");

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
