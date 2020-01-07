/***********************************************************************************************************************************
Test Remote Command
***********************************************************************************************************************************/
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageData = storagePosixNew(
        strNew(testDataPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("cmdRemote()"))
    {
        // No remote lock required
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                strLstAddZ(argList, "--command=info");
                strLstAddZ(argList, "--process=1");
                strLstAddZ(argList, "--type=backup");
                harnessCfgLoad(cfgCmdRemote, argList);

                cmdRemote(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                ProtocolClient *client = protocolClientNew(strNew("test"), PROTOCOL_SERVICE_REMOTE_STR, read, write);
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // Remote lock not required
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, testProjectExe());
                strLstAddZ(argList, "--command=archive-get-async");
                strLstAddZ(argList, "--process=0");
                strLstAddZ(argList, "--type=backup");
                strLstAddZ(argList, "--lock-path=/bogus");
                strLstAddZ(argList, "remote");
                harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

                cmdRemote(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                ProtocolClient *client = NULL;
                TEST_ASSIGN(client, protocolClientNew(strNew("test"), PROTOCOL_SERVICE_REMOTE_STR, read, write), "create client");
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // Remote lock required but errors out
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, testProjectExe());
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--command=archive-push-async");
                strLstAddZ(argList, "--process=0");
                strLstAddZ(argList, "--type=backup");
                strLstAddZ(argList, "--lock-path=/bogus");
                strLstAddZ(argList, "remote");
                harnessCfgLoadRaw(strLstSize(argList), strLstPtr(argList));

                cmdRemote(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                TEST_ERROR(
                    protocolClientNew(strNew("test"), PROTOCOL_SERVICE_REMOTE_STR, read, write), PathCreateError,
                    "raised from test: unable to create path '/bogus': [13] Permission denied");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // Remote lock required
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--command=archive-push-async");
                strLstAddZ(argList, "--process=0");
                strLstAddZ(argList, "--type=backup");
                harnessCfgLoad(cfgCmdRemote, argList);

                cmdRemote(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                ProtocolClient *client = NULL;
                TEST_ASSIGN(client, protocolClientNew(strNew("test"), PROTOCOL_SERVICE_REMOTE_STR, read, write), "create client");
                protocolClientNoOp(client);

                TEST_RESULT_BOOL(
                    storageExistsP(
                        storagePosixNew(strNew(testDataPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL),
                        STRDEF("lock/test-archive" LOCK_FILE_EXT)),
                    true, "lock exists");

                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stop file exists");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--command=archive-push-async");
                strLstAddZ(argList, "--process=0");
                strLstAddZ(argList, "--type=backup");
                harnessCfgLoad(cfgCmdRemote, argList);

                cmdRemote(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                storagePutP(storageNewWriteP(storageData, strNew("lock/all" STOP_FILE_EXT)), NULL);

                TEST_ERROR(
                    protocolClientNew(strNew("test"), PROTOCOL_SERVICE_REMOTE_STR, read, write), StopError,
                    "raised from test: stop file exists for all stanzas");

                storageRemoveP(storageData, strNew("lock/all" STOP_FILE_EXT));
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
