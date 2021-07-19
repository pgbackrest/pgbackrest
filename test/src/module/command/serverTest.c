/***********************************************************************************************************************************
Test Server Command
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessServer.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    harnessLogLevelSet(logLevelDetail);

    // *****************************************************************************************************************************
    if (testBegin("cmdServer()"))
    {
        TEST_TITLE("!!!");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "client")
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
                hrnCfgArgRawZ(argList, cfgOptRepoHost, "localhost"); // !!! NEEDS TO BE GENERIC
                hrnCfgArgRawZ(argList, cfgOptRepoHostType, "tls");
                hrnCfgArgRawFmt(argList, cfgOptRepoHostPort, "%u", hrnServerPort(0));
                hrnCfgArgRawZ(argList, cfgOptStanza, "db");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

                ProtocolClient *client = NULL;
                TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "new client");

                TEST_RESULT_VOID(protocolClientCommandPut(client, protocolCommandNew(PROTOCOL_COMMAND_EXIT)), "exit");

                TEST_RESULT_VOID(protocolClientFree(client), "client free");

                HRN_FORK_CHILD_NOTIFY_PUT();
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "server")
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptTlsServerCert, HRN_PATH_REPO "/test/certificate/pgbackrest-test.crt");
                hrnCfgArgRawZ(argList, cfgOptTlsServerKey, HRN_PATH_REPO "/test/certificate/pgbackrest-test.key");
                hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                HRN_CFG_LOAD(cfgCmdServer, argList);

                // Get pid of this process to identify child process later
                pid_t pid = getpid();

                TEST_RESULT_VOID(cmdServer(1), "server");

                // If this is a child process then exit immediately
                if (pid != getpid())
                    exit(0);

                HRN_FORK_PARENT_NOTIFY_GET(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

    }

    FUNCTION_HARNESS_RETURN_VOID();
}
