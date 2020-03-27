/***********************************************************************************************************************************
Storage Performance

Test the performance of various storage functions, in particular when implemented remotely.

Generally speaking, the starting values should be high enough to "blow up" in terms of execution time if there are performance
problems without taking very long if everything is running smoothly. These starting values can then be scaled up for profiling and
stress testing as needed.  In general we hope to scale to 1000 without running out of memory on the test systems or taking an undue
amount of time.  It should be noted that in this context scaling to 1000 is nowhere near to turning it up to 11.
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"

#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "common/object.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "storage/remote/protocol.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Dummy functions and interface for constructing test drivers
***********************************************************************************************************************************/
static StorageInfo
storageTestDummyInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    (void)thisVoid; (void)file; (void)level; (void)param; return (StorageInfo){.exists = false};
}

static bool
storageTestDummyInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    (void)thisVoid; (void)path; (void)level; (void)callback; (void)callbackData; (void)param; return false;
}

static StorageRead *
storageTestDummyNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    (void)thisVoid; (void)file; (void)ignoreMissing; (void)param; return NULL;
}

static StorageWrite *
storageTestDummyNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    (void)thisVoid; (void)file; (void)param; return NULL;
}

static bool
storageTestDummyPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    (void)thisVoid; (void)path; (void)recurse; (void)param; return false;
}

static void
storageTestDummyRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    (void)thisVoid; (void)file; (void)param;
}

static const StorageInterface storageInterfaceTestDummy =
{
    .info = storageTestDummyInfo,
    .infoList = storageTestDummyInfoList,
    .newRead = storageTestDummyNewRead,
    .newWrite = storageTestDummyNewWrite,
    .pathRemove = storageTestDummyPathRemove,
    .remove = storageTestDummyRemove,
};

/***********************************************************************************************************************************
Dummy callback functions
***********************************************************************************************************************************/
static void
storageTestDummyInfoListCallback(void *data, const StorageInfo *info)
{
    (void)data;
    (void)info;

    // Do some work in the mem context to blow up the total time if this is not efficient
    memResize(memNew(16), 32);
}

/***********************************************************************************************************************************
Driver to test storageInfoList
***********************************************************************************************************************************/
typedef struct
{
    STORAGE_COMMON_MEMBER;
    uint64_t fileTotal;
} StorageTestPerfInfoList;

static bool
storageTestPerfInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StorageTestPerfInfoList);
    (void)path; (void)level; (void)param;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (uint64_t fileIdx = 0; fileIdx < this->fileTotal; fileIdx++)
            {
                callback(callbackData, &(StorageInfo){.exists = true});
                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    MEM_CONTEXT_TEMP_END();

    return this->fileTotal != 0;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("storageInfoList()"))
    {
        // One million files represents a fairly large cluster
        CHECK(testScale() <= 2000);
        uint64_t fileTotal = (uint64_t)1000000 * testScale();

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                // Create a basic configuration so the remote storage driver can determine the storage type
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
                strLstAddZ(argList, "--" CFGOPT_PROCESS "=0");
                strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
                harnessCfgLoadRole(cfgCmdArchivePush, cfgCmdRoleRemote, argList);

                // Create a driver to test remote performance of storageInfoList() and inject it into storageRepo()
                StorageTestPerfInfoList driver =
                {
                    .interface = storageInterfaceTestDummy,
                    .fileTotal = fileTotal,
                };

                driver.interface.infoList = storageTestPerfInfoList;

                storageHelper.storageRepo = storageNew(STRDEF("TEST"), STRDEF("/"), 0, 0, false, NULL, &driver, driver.interface);

                // Setup handler for remote storage protocol
                IoRead *read = ioHandleReadNew(strNew("storage server read"), HARNESS_FORK_CHILD_READ(), 60000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("storage server write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                ProtocolServer *server = protocolServerNew(strNew("storage test server"), strNew("test"), read, write);
                protocolServerHandlerAdd(server, storageRemoteProtocol);
                protocolServerProcess(server);

            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                // Create client
                IoRead *read = ioHandleReadNew(strNew("storage client read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 60000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("storage client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                ProtocolClient *client = protocolClientNew(strNew("storage test client"), strNew("test"), read, write);

                // Create remote storage
                Storage *storageRemote = storageRemoteNew(
                    STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL, client, 1);

                // Storage info list
                TEST_RESULT_VOID(
                    storageInfoListP(storageRemote, NULL, storageTestDummyInfoListCallback, NULL),
                    "list %" PRIu64 " remote files", fileTotal);

                // Free client
                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
