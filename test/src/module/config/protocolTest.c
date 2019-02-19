/***********************************************************************************************************************************
Test Configuration Protocol
***********************************************************************************************************************************/
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
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
    if (testBegin("configProtocol() and configProtocolOption()"))
    {
        // Create pipes for testing.  Read/write is from the perspective of the client.
        int pipeRead[2];
        int pipeWrite[2];
        THROW_ON_SYS_ERROR(pipe(pipeRead) == -1, KernelError, "unable to read test pipe");
        THROW_ON_SYS_ERROR(pipe(pipeWrite) == -1, KernelError, "unable to write test pipe");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                close(pipeRead[0]);
                close(pipeWrite[1]);

                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("client read"), pipeWrite[0], 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("client write"), pipeRead[1]));
                ioWriteOpen(write);

                StringList *argList = strLstNew();
                strLstAddZ(argList, "pgbackrest");
                strLstAddZ(argList, "--stanza=test1");
                strLstAddZ(argList, "--repo1-host=repo-host");
                strLstAddZ(argList, "--repo1-host-user=repo-host-user");
                strLstAddZ(argList, "archive-get");
                harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

                ProtocolServer *server = protocolServerNew(strNew("test"), strNew("config"), read, write);
                protocolServerHandlerAdd(server, configProtocol);
                protocolServerProcess(server);

                close(pipeRead[1]);
                close(pipeWrite[0]);
            }

            HARNESS_FORK_PARENT()
            {
                close(pipeRead[1]);
                close(pipeWrite[0]);

                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("server read"), pipeRead[0], 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("server write"), pipeWrite[1]));
                ioWriteOpen(write);

                ProtocolClient *client = protocolClientNew(strNew("test"), strNew("config"), read, write);

                VariantList *list = varLstNew();
                varLstAdd(list, varNewStr(strNew("repo1-host")));
                varLstAdd(list, varNewStr(strNew("repo1-host-user")));

                TEST_RESULT_STR(
                    strPtr(strLstJoin(strLstNewVarLst(configProtocolOption(client, list)), "|")), "repo-host|repo-host-user",
                    "get options");

                protocolClientFree(client);

                close(pipeRead[0]);
                close(pipeWrite[1]);
            }
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
