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
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                strLstAddZ(argList, "--repo1-host=repo-host");
                strLstAddZ(argList, "--repo1-host-user=repo-host-user");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

                ProtocolServer *server = protocolServerNew(
                    STRDEF("test"), STRDEF("config"), ioFdReadNewOpen(STRDEF("client read"), HARNESS_FORK_CHILD_READ(), 2000),
                    ioFdWriteNewOpen(STRDEF("client write"), HARNESS_FORK_CHILD_WRITE(), 2000));

                static const ProtocolServerHandler commandHandler[] = {PROTOCOL_SERVER_HANDLER_OPTION_LIST};
                protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), STRDEF("config"),
                    ioFdReadNewOpen(STRDEF("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000),
                    ioFdWriteNewOpen(STRDEF("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000));

                VariantList *list = varLstNew();
                varLstAdd(list, varNewStr(STRDEF("repo1-host")));
                varLstAdd(list, varNewStr(STRDEF("repo1-host-user")));

                TEST_RESULT_STRLST_Z(
                    strLstNewVarLst(configOptionRemote(client, list)), "repo-host\nrepo-host-user\n", "get options");

                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
