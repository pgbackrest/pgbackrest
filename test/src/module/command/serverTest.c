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
                HRN_CFG_LOAD(cfgCmdBackup, argList);

                ProtocolClient *client = NULL;
                TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "new client");

                TEST_RESULT_VOID(protocolClientCommandPut(client, protocolCommandNew(PROTOCOL_COMMAND_EXIT)), "exit");

                TEST_RESULT_VOID(protocolClientFree(client), "client free");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "server")
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptTlsServerCert, HRN_PATH_REPO "/test/certificate/pgbackrest-test.crt");
                hrnCfgArgRawZ(argList, cfgOptTlsServerKey, HRN_PATH_REPO "/test/certificate/pgbackrest-test.key");
                hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                HRN_CFG_LOAD(cfgCmdServer, argList);

                TEST_RESULT_VOID(cmdServer(3), "server");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

    }

    FUNCTION_HARNESS_RETURN_VOID();
}
