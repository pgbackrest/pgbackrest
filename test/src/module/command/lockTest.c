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
        hrnCfgArgRawZ(argList, cfgOptRepoRetentionFull, "1");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .noStd = true);

        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire backup lock");
        TEST_ERROR(cmdLockAcquireP(), AssertError, "lock is already held by this process");
        TEST_STORAGE_LIST(storageTest, NULL, "test-backup.lock\n", .comment = "check lock file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("write backup data");

        TEST_RESULT_VOID(
            cmdLockWriteP(.percentComplete = VARUINT(5555), .sizeComplete = VARUINT64(1754824), .size = VARUINT64(3159000)),
            "write backup data");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("read backup data");

        LockReadResult lockReadResult;
        TEST_ASSIGN(lockReadResult, cmdLockRead(lockTypeBackup, STRDEF("test")), "read backup data");
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
        HRN_CFG_LOAD(cfgCmdStanzaCreate, argList, .noStd = true);

        TEST_RESULT_VOID(cmdLockAcquireP(), "acquire stanza-create lock");
        TEST_STORAGE_LIST(storageTest, NULL, "test-archive.lock\ntest-backup.lock\n", .comment = "check lock file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("lock release");

        TEST_RESULT_VOID(cmdLockReleaseP(), "release locks");
        TEST_STORAGE_LIST(storageTest, NULL, NULL, .comment = "check lock file does not exist");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
