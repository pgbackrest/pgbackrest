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
                strLstAddZ(argList, "--stanza=test1");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                strLstAddZ(argList, "--repo1-host=repo-host");
                strLstAddZ(argList, "--repo1-host-user=repo-host-user");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

                ProtocolServer *server = protocolServerNew(
                    STRDEF("test"), STRDEF("config"), ioFdReadNewOpen(STRDEF("client read"), HRN_FORK_CHILD_READ_FD(), 2000),
                    ioFdWriteNewOpen(STRDEF("client write"), HRN_FORK_CHILD_WRITE_FD(), 2000));

                static const ProtocolServerHandler commandHandler[] = {PROTOCOL_SERVER_HANDLER_OPTION_LIST};
                protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), STRDEF("config"),
                    ioFdReadNewOpen(STRDEF("server read"), HRN_FORK_PARENT_READ_FD(0), 2000),
                    ioFdWriteNewOpen(STRDEF("server write"), HRN_FORK_PARENT_WRITE_FD(0), 2000));

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
