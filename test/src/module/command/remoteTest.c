/***********************************************************************************************************************************
Test Remote Command
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "protocol/client.h"
#include "protocol/server.h"
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
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
    const Storage *const hrnStorage = storagePosixNewP(HRN_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("cmdRemote()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no lock is required because process is > 0 (not the main remote)");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
                hrnCfgArgRawZ(argList, cfgOptProcess, "1");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdInfo, argList, .role = cfgCmdRoleRemote);

                cmdRemote(
                    protocolServerNew(
                        PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                    ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                    ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000));
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("no remote lock is required for this command");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptProcess, "0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                hrnCfgArgRawZ(argList, cfgOptLockPath, "/bogus");
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleRemote, .noStd = true);

                cmdRemote(
                    protocolServerNew(
                        PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000)),
                    "create client");
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote lock is required but lock path is invalid");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawZ(argList, cfgOptProcess, "0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                hrnCfgArgRawZ(argList, cfgOptLockPath, "/bogus");
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote, .noStd = true);

                cmdRemote(
                    protocolServerNew(
                        PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000)), "new client");

                TEST_ERROR(
                    protocolClientNoOp(client), PathCreateError,
                    "raised from test: unable to create path '/bogus': [13] Permission denied");

                // Do not send the exit command before freeing since the server has already errored
                TEST_RESULT_VOID(protocolClientNoExit(client), "client no exit");
                TEST_RESULT_VOID(protocolClientFree(client), "client free");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote lock is required and succeeds");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawZ(argList, cfgOptProcess, "0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                hrnCfgArgRawZ(argList, cfgOptRepo, "1");
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote);

                cmdRemote(
                    protocolServerNew(
                        PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000)),
                    "create client");
                protocolClientNoOp(client);

                TEST_STORAGE_EXISTS(hrnStorage, "lock/test-archive" LOCK_FILE_EXT, .comment = "lock exists");

                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote lock is required but stop file exists");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test");
                hrnCfgArgRawZ(argList, cfgOptProcess, "0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote);

                cmdRemote(
                    protocolServerNew(
                        PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                HRN_STORAGE_PUT_EMPTY(hrnStorage, "lock/all" STOP_FILE_EXT);

                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000)), "new client");

                TEST_ERROR(protocolClientNoOp(client), StopError, "raised from test: stop file exists for all stanzas");

                // Do not send the exit command before freeing since the server has already errored
                TEST_RESULT_VOID(protocolClientNoExit(client), "client no exit");
                TEST_RESULT_VOID(protocolClientFree(client), "client free");

                HRN_STORAGE_REMOVE(hrnStorage, "lock/all" STOP_FILE_EXT);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
