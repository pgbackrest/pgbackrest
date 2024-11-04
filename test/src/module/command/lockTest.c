/***********************************************************************************************************************************
Test Command Lock Handler
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
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
    if (testBegin("cmdLock*()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire backup command lock");

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptLockPath, TEST_PATH);
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 2, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, "/repo2");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .noStd = true);

        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire backup lock");
        TEST_ERROR(cmdLockAcquireP(), AssertError, "lock is already held by this process");
        TEST_STORAGE_LIST(storageTest, NULL, "test-backup-1.lock\n", .comment = "check lock file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write backup data");

        TEST_RESULT_VOID(
            cmdLockWriteP(.percentComplete = VARUINT(5555), .sizeComplete = VARUINT64(1754824), .size = VARUINT64(3159000)),
            "write backup data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read backup data");

        LockReadResult lockReadResult;
        TEST_ASSIGN(lockReadResult, cmdLockRead(lockTypeBackup, STRDEF("test"), 0), "read backup data");
        TEST_RESULT_UINT(lockReadResult.status, lockReadStatusValid, "check status");
        TEST_RESULT_UINT(varUInt(lockReadResult.data.percentComplete), 5555, "check percent complete");
        TEST_RESULT_UINT(varUInt64(lockReadResult.data.sizeComplete), 1754824, "check size complete");
        TEST_RESULT_UINT(varUInt64(lockReadResult.data.size), 3159000, "check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lock release");

        TEST_RESULT_VOID(cmdLockReleaseP(), "release locks");
        TEST_STORAGE_LIST(storageTest, NULL, NULL, .comment = "check lock file does not exist");

        TEST_ERROR(cmdLockReleaseP(), AssertError, "no lock is held by this process");
        TEST_RESULT_VOID(cmdLockReleaseP(.returnOnNoLock = true), "ignore no lock held");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire stanza-create command lock");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptLockPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 1, "/repo1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoPath, 2, "/repo2");
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList, .noStd = true);

        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire stanza-create lock");
        TEST_STORAGE_LIST(
            storageTest, NULL, "test-archive-1.lock\ntest-archive-2.lock\ntest-backup-1.lock\ntest-backup-2.lock\n",
            .comment = "check lock files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lock release");

        TEST_RESULT_VOID(cmdLockReleaseP(), "release locks");
        TEST_STORAGE_LIST(storageTest, NULL, NULL, .comment = "check lock file does not exist");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("acquire backup:remote command lock");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawZ(argList, cfgOptLockPath, TEST_PATH);
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/pg1");
        hrnCfgArgRawZ(argList, cfgOptLock, "test-lock-99.lock");
        hrnCfgArgRawZ(argList, cfgOptLock, "other-lock.lock");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleRemote, .noStd = true);

        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire backup:remote lock");
        TEST_STORAGE_LIST(storageTest, NULL, "other-lock.lock\ntest-lock-99.lock\n", .comment = "check lock files");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lock release");

        TEST_RESULT_VOID(cmdLockReleaseP(), "release locks");
        TEST_STORAGE_LIST(storageTest, NULL, NULL, .comment = "check lock file does not exist");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
