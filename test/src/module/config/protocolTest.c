/***********************************************************************************************************************************
Test Configuration Protocol
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "protocol/client.h"
#include "protocol/server.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("configOptionProtocol() and configOptionRemote()"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                hrnCfgArgRawZ(argList, cfgOptRepoHost, "repo-host");
                hrnCfgArgRawZ(argList, cfgOptRepoHostUser, "repo-host-user");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

                ProtocolServer *server = protocolServerNew(
                    STRDEF("test"), STRDEF("config"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE());

                static const ProtocolServerHandler commandHandler[] = {PROTOCOL_SERVER_HANDLER_OPTION_LIST};
                protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), STRDEF("config"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0));

                VariantList *list = varLstNew();
                varLstAdd(list, varNewStr(STRDEF("repo1-host")));
                varLstAdd(list, varNewStr(STRDEF("repo1-host-user")));

                TEST_RESULT_STRLST_Z(
                    strLstNewVarLst(configOptionRemote(client, list)), "repo-host\nrepo-host-user\n", "get options");

                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
