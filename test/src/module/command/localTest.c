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
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("cmdLocal()"))
    {
        // Create pipes for testing.  Read/write is from the perspective of the client.
        int pipeRead[2];
        int pipeWrite[2];
        THROW_ON_SYS_ERROR(pipe(pipeRead) == -1, KernelError, "unable to read test pipe");
        THROW_ON_SYS_ERROR(pipe(pipeWrite) == -1, KernelError, "unable to write test pipe");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
                strLstAddZ(argList, "--process=1");
                strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_REPO);
                harnessCfgLoadRole(cfgCmdArchiveGet, cfgCmdRoleLocal, argList);

                cmdLocal(HARNESS_FORK_CHILD_READ(), HARNESS_FORK_CHILD_WRITE());
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                ProtocolClient *client = protocolClientNew(strNew("test"), PROTOCOL_SERVICE_LOCAL_STR, read, write);
                protocolClientNoOp(client);
                protocolClientFree(client);
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
