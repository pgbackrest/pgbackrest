/***********************************************************************************************************************************
Test Local Command
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "protocol/client.h"
#include "protocol/server.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdLocal()"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                hrnCfgArgRawZ(argList, cfgOptProcess, "1");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleLocal);

                cmdLocal(
                    protocolServerNew(
                        PROTOCOL_SERVICE_LOCAL_STR, PROTOCOL_SERVICE_LOCAL_STR, HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                ProtocolClient *client = protocolClientNew(
                    STRDEF("test"), PROTOCOL_SERVICE_LOCAL_STR, HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0));
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
