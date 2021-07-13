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

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
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
            HRN_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                strLstAddZ(argList, "--process=1");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdInfo, argList, .role = cfgCmdRoleRemote);

                cmdRemote(HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                    ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_PROCESS(0), 2000),
                    ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_PROCESS(0), 2000));
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
            HRN_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--process=0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                strLstAddZ(argList, "--lock-path=/bogus");
                strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleRemote, .noStd = true);

                cmdRemote(HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_PROCESS(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_PROCESS(0), 2000)),
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
            HRN_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--process=0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                strLstAddZ(argList, "--lock-path=/bogus");
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote, .noStd = true);

                cmdRemote(HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                TEST_ERROR(
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_PROCESS(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_PROCESS(0), 2000)),
                    PathCreateError, "raised from test: unable to create path '/bogus': [13] Permission denied");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote lock is required and succeeds");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--process=0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                hrnCfgArgRawZ(argList, cfgOptRepo, "1");
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote);

                cmdRemote(HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = NULL;
                TEST_ASSIGN(
                    client,
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_PROCESS(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_PROCESS(0), 2000)),
                    "create client");
                protocolClientNoOp(client);

                TEST_RESULT_BOOL(
                    storageExistsP(storagePosixNewP(HRN_PATH_STR), STRDEF("lock/test-archive" LOCK_FILE_EXT)),
                    true, "lock exists");

                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("remote lock is required but stop file exists");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test");
                strLstAddZ(argList, "--process=0");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleRemote);

                cmdRemote(HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                storagePutP(storageNewWriteP(hrnStorage, STRDEF("lock/all" STOP_FILE_EXT)), NULL);

                TEST_ERROR(
                    protocolClientNew(
                        STRDEF("test"), PROTOCOL_SERVICE_REMOTE_STR,
                        ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_PROCESS(0), 2000),
                        ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_PROCESS(0), 2000)),
                    StopError, "raised from test: stop file exists for all stanzas");

                storageRemoveP(hrnStorage, STRDEF("lock/all" STOP_FILE_EXT));
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
