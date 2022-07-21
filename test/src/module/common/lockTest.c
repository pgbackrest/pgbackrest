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
        TEST_RESULT_VOID(lockWriteDataP(lockTypeArchive), "write lock data");

        lockLocal.execId = STRDEF("2-test");

        TEST_ERROR_FMT(
            lockAcquireFile(archiveLock, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?",
            strZ(archiveLock));
        TEST_RESULT_BOOL(lockAcquireFile(archiveLock, 0, false) == -1, true, "lock is already held");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire file lock on the same exec-id");

        lockLocal.execId = STRDEF("1-test");

        TEST_RESULT_INT(lockAcquireFile(archiveLock, 0, true), -2, "allow lock with same exec id");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("fail file lock on the same exec-id when lock file is empty");

        HRN_SYSTEM_FMT("echo '' > %s", strZ(archiveLock));

        TEST_ERROR_FMT(
            lockAcquireFile(archiveLock, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?",
            strZ(archiveLock));

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

        HRN_SYSTEM_FMT("mkdir -p %s", strZ(dirLock));

        TEST_ERROR_FMT(
            lockAcquireFile(dirLock, 0, true), LockAcquireError, "unable to acquire lock on file '%s': Is a directory",
            strZ(dirLock));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permissions error throw regardless of failOnLock");

        String *noPermLock = strNewZ(TEST_PATH "/noperm/noperm");

        HRN_SYSTEM_FMT("mkdir -p %s", strZ(strPath(noPermLock)));
        HRN_SYSTEM_FMT("chmod 000 %s", strZ(strPath(noPermLock)));

        TEST_ERROR_FMT(
            lockAcquireFile(noPermLock, 100, true), LockAcquireError,
            "unable to acquire lock on file '%s': Permission denied\n"
            "HINT: does '" TEST_USER ":" TEST_GROUP "' running pgBackRest have permissions on the '%s' file?",
            strZ(noPermLock), strZ(noPermLock));
        TEST_ERROR_FMT(
            lockAcquireFile(noPermLock, 100, false), LockAcquireError,
            "unable to acquire lock on file '%s': Permission denied\n"
            "HINT: does '" TEST_USER ":" TEST_GROUP "' running pgBackRest have permissions on the '%s' file?",
            strZ(noPermLock), strZ(noPermLock));

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

                TEST_ERROR_FMT(
                    lockAcquireFile(backupLock, 0, true), LockAcquireError,
                    "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?",
                    strZ(backupLock));

                TEST_RESULT_VOID(lockAcquireFile(backupLock, 0, false), "success when failOnLock = false");

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
        TEST_ERROR_FMT(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeArchive, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?",
            strZ(archiveLockFile));
        TEST_ERROR_FMT(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?", strZ(archiveLockFile));
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

        TEST_ERROR_FMT(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeBackup, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?", strZ(backupLockFile));
        TEST_ERROR_FMT(
            lockAcquire(TEST_PATH_STR, stanza, STRDEF("2-test"), lockTypeAll, 0, true), LockAcquireError,
            "unable to acquire lock on file '%s': Resource temporarily unavailable\n"
            "HINT: is another pgBackRest process running?", strZ(backupLockFile));
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, archiveLockFile), "release archive lock");
        TEST_RESULT_VOID(lockReleaseFile(lockFdTest, backupLockFile), "release backup lock");

        // -------------------------------------------------------------------------------------------------------------------------
        lockLocal.execId = STRDEF("1-test");

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeAll, 0, true), true, "all lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, archiveLockFile), true, "archive lock file was created");
        TEST_RESULT_BOOL(storageExistsP(storageTest, backupLockFile), true, "backup lock file was created");

        // Seek to start of file before read
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));
        TEST_RESULT_STR(
            lockReadFileData(backupLockFile, lockLocal.file[lockTypeBackup].fd).execId, STRDEF("1-test"), "verify execId");

        TEST_RESULT_VOID(lockWriteDataP(lockTypeBackup), "write lock data");
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));
        TEST_RESULT_PTR(
            lockReadFileData(backupLockFile, lockLocal.file[lockTypeBackup].fd).percentComplete, NULL, "verify percentComplete");

        TEST_RESULT_VOID(lockWriteDataP(lockTypeBackup, .percentComplete = VARUINT(5555)), "write lock data");
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));
        TEST_RESULT_UINT(
            varUInt(lockReadFileData(backupLockFile, lockLocal.file[lockTypeBackup].fd).percentComplete), 5555,
            "verify percentComplete");

        TEST_RESULT_VOID(lockWriteDataP(lockTypeBackup, .percentComplete = VARUINT(8888)), "write lock data");
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));
        TEST_RESULT_UINT(
            varUInt(lockReadFileData(backupLockFile, lockLocal.file[lockTypeBackup].fd).percentComplete), 8888,
            "verify percentComplete");

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
        TEST_TITLE("execId && pid valid file");

        const String *stanza = STRDEF("1-test");
        lockLocal.execId = STRDEF("1-test");

        TEST_RESULT_BOOL(lockAcquire(TEST_PATH_STR, stanza, STRDEF("1-test"), lockTypeBackup, 0, true), true, "backup lock");
        TEST_RESULT_BOOL(storageExistsP(storageTest, lockLocal.file[lockTypeBackup].name), true, "backup lock file was created");

        // Overwrite backup lock file with execId of 1-test and pid of 12345
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));

        ftruncate(lockLocal.file[lockTypeBackup].fd, 0);

        IoWrite *const write = ioFdWriteNewOpen(lockLocal.file[lockTypeBackup].name, lockLocal.file[lockTypeBackup].fd, 0);

        ioCopyP(ioBufferReadNewOpen(BUFSTRDEF("{\"execId\":\"1-test\",\"pid\":12345}")), write);
        ioWriteClose(write);

        // Seek to start of file before read
        THROW_ON_SYS_ERROR_FMT(
            lseek(lockLocal.file[lockTypeBackup].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
            strZ(lockLocal.file[lockTypeBackup].name));

        LockReadResult result = {0};
        TEST_ASSIGN(result, lockReadFileP(lockLocal.file[lockTypeBackup].name), "lock read");
        TEST_RESULT_STR(result.data.execId, STRDEF("1-test"), "lock read execId 1-test");
        TEST_RESULT_UINT((uint64_t)result.data.processId, 12345, "lock read pid 12345");
        TEST_RESULT_VOID(lockRelease(true), "release backup lock");

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
        TEST_TITLE("valid locked file");

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
