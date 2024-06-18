/***********************************************************************************************************************************
Test Lock Handler
***********************************************************************************************************************************/
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
    const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("lock*()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire lock");

        const String *const lockFile1Name = STRDEF("test1" LOCK_FILE_EXT);

        TEST_RESULT_VOID(lockInit(TEST_PATH_STR, STRDEF("1-test")), "init lock module");
        TEST_RESULT_BOOL(lockAcquireP(lockFile1Name), true, "acquire lock");
        TEST_ERROR_FMT(lockAcquireP(lockFile1Name), AssertError, "lock on file 'test1.lock' already held");

        TEST_ERROR(lockAcquireP(TEST_PATH_STR), LockAcquireError, "unable to acquire lock on file '" TEST_PATH "': Is a directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read errors");

        TEST_ERROR(
            lockReadData(STRDEF(BOGUS_STR), 999999999), FileReadError,
            "unable to read lock file 'BOGUS': unable to read from BOGUS: [9] Bad file descriptor");
        TEST_RESULT_UINT(lockReadP(STRDEF(BOGUS_STR)).status, lockReadStatusMissing, "missing lock file");

        HRN_STORAGE_PUT_EMPTY(storageTest, "unlocked.lock");
        TEST_RESULT_UINT(lockReadP(STRDEF("unlocked.lock"), .remove = true).status, lockReadStatusUnlocked, "unlocked lock file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read lock file (no data set)");

        LockReadResult lockReadResult;
        TEST_ASSIGN(lockReadResult, lockReadP(lockFile1Name), "read lock file");
        TEST_RESULT_UINT(lockReadResult.status, lockReadStatusValid, "check status");
        TEST_RESULT_PTR(lockReadResult.data.percentComplete, NULL, "check percent complete is NULL");
        TEST_RESULT_PTR(lockReadResult.data.sizeComplete, NULL, "check size complete is NULL");
        TEST_RESULT_PTR(lockReadResult.data.size, NULL, "check size is NULL");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalidate lock file by truncating");

        LockFile *lockFile1 = lstFind(lockLocal.lockList, &lockFile1Name);
        THROW_ON_SYS_ERROR_FMT(
            ftruncate(lockFile1->fd, 0) == -1, FileWriteError, "unable to truncate '" TEST_PATH "/%s'", strZ(lockFile1Name));

        TEST_RESULT_UINT(lockReadP(lockFile1Name).status, lockReadStatusInvalid, "invalid lock file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write errors");

        TEST_ERROR(lockWriteP(STRDEF(BOGUS_STR)), AssertError, "lock file 'BOGUS' not found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write lock file");

        TEST_RESULT_VOID(
            lockWriteP(
                lockFile1Name, .percentComplete = VARUINT(5555), .sizeComplete = VARUINT64(1754824), .size = VARUINT64(3159000)),
            "write lock data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read lock file (data set)");

        TEST_ASSIGN(lockReadResult, lockReadP(lockFile1Name), "read lock file");
        TEST_RESULT_UINT(lockReadResult.status, lockReadStatusValid, "check status");
        TEST_RESULT_UINT(varUInt(lockReadResult.data.percentComplete), 5555, "check percent complete");
        TEST_RESULT_UINT(varUInt64(lockReadResult.data.sizeComplete), 1754824, "check size complete");
        TEST_RESULT_UINT(varUInt64(lockReadResult.data.size), 3159000, "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("permissions error throw regardless of failOnLock");

        const String *const noPermLock = strNewZ(TEST_PATH "/noperm/noperm");

        HRN_SYSTEM_FMT("mkdir -p %s", strZ(strPath(noPermLock)));
        HRN_SYSTEM_FMT("chmod 000 %s", strZ(strPath(noPermLock)));

        TEST_ERROR_FMT(
            lockAcquireP(noPermLock), LockAcquireError,
            "unable to acquire lock on file '%s': Permission denied\n"
            "HINT: does '" TEST_USER ":" TEST_GROUP "' running pgBackRest have permissions on the '%s' file?",
            strZ(noPermLock), strZ(noPermLock));
        TEST_ERROR_FMT(
            lockAcquireP(noPermLock, .returnOnNoLock = true), LockAcquireError,
            "unable to acquire lock on file '%s': Permission denied\n"
            "HINT: does '" TEST_USER ":" TEST_GROUP "' running pgBackRest have permissions on the '%s' file?",
            strZ(noPermLock), strZ(noPermLock));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire file lock on the same exec-id");

        const String *const lockFileExecName = STRDEF("sub/exec" LOCK_FILE_EXT);
        const String *const lockFileExec2Name = STRDEF("sub/exec2" LOCK_FILE_EXT);

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                TEST_RESULT_BOOL(lockAcquireP(lockFileExecName), true, "lock on fork");
                TEST_RESULT_BOOL(lockAcquireP(lockFileExec2Name), true, "lock on fork");

                // Corrupt exec2.lock
                HRN_SYSTEM_FMT("echo '' > " TEST_PATH "/%s", strZ(lockFileExec2Name));

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
                    lockAcquireP(lockFileExecName), LockAcquireError,
                    "unable to acquire lock on file '" TEST_PATH "/%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?",
                    strZ(lockFileExecName));

                TEST_RESULT_BOOL(lockAcquireP(lockFileExecName, .returnOnNoLock = true), false, "fail but return");

                lockLocal.execId = STRDEF("1-test");

                TEST_RESULT_BOOL(lockAcquireP(lockFileExecName), true, "succeed on same execId");

                TEST_ERROR_FMT(
                    lockAcquireP(lockFileExec2Name), LockAcquireError,
                    "unable to acquire lock on file '" TEST_PATH "/%s': Resource temporarily unavailable\n"
                    "HINT: is another pgBackRest process running?",
                    strZ(lockFileExec2Name));

                // Notify child to release lock
                HRN_FORK_PARENT_NOTIFY_PUT(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lock release");

        TEST_RESULT_VOID(lockReleaseP(), "release locks");
        TEST_ERROR(lockReleaseP(), AssertError, "no lock is held by this process");
        TEST_RESULT_VOID(lockReleaseP(.returnOnNoLock = true), "ignore no lock held");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
