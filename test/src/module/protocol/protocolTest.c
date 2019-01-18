/***********************************************************************************************************************************
Test Protocol
***********************************************************************************************************************************/
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"

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
    if (testBegin("repoIsLocal()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(repoIsLocal(), true, "repo is local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-host=remote-host");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_BOOL(repoIsLocal(), false, "repo is remote");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolParam()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(
            strPtr(strLstJoin(protocolParam(remoteTypeRepo, 1), "|")),
            strPtr(
                strNew(
                    "-o|LogLevel=error|-o|Compression=no|-o|PasswordAuthentication=no|repo-host-user@repo-host"
                        "|pgbackrest --command=archive-get --process=0 --stanza=test1 --type=backup remote")),
            "remote protocol params");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-port=444");
        strLstAddZ(argList, "--repo1-host-config=/path/pgbackrest.conf");
        strLstAddZ(argList, "--repo1-host-config-include-path=/path/include");
        strLstAddZ(argList, "--repo1-host-config-path=/path/config");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        strLstAddZ(argList, "archive-get");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(
            strPtr(strLstJoin(protocolParam(remoteTypeRepo, 1), "|")),
            strPtr(
                strNew(
                    "-o|LogLevel=error|-o|Compression=no|-o|PasswordAuthentication=no|-p|444|repo-host-user@repo-host"
                        "|pgbackrest --command=archive-get --config=/path/pgbackrest.conf --config-include-path=/path/include"
                        " --config-path=/path/config --process=0 --stanza=test1 --type=backup remote")),
            "remote protocol params with replacements");
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolClient"))
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

                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("server read"), pipeWrite[0], 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("server write"), pipeRead[1]));
                ioWriteOpen(write);

                // Various bogus greetings
                ioWriteLine(write, strNew("bogus greeting"));
                ioWriteFlush(write);
                ioWriteLine(write, strNew("{\"name\":999}"));
                ioWriteFlush(write);
                ioWriteLine(write, strNew("{\"name\":null}"));
                ioWriteFlush(write);
                ioWriteLine(write, strNew("{\"name\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"bogus\"}"));
                ioWriteFlush(write);

                // Correct greeting with noop
                ioWriteLine(write, strNew("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"noop\"}", "noop");
                ioWriteLine(write, strNew("{\"out\":[]}"));
                ioWriteFlush(write);

                // Throw errors
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"noop\"}", "noop with error text");
                ioWriteLine(write, strNew("{\"err\":25,\"out\":\"sample error message\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"noop\"}", "noop with no error text");
                ioWriteLine(write, strNew("{\"err\":255}"));
                ioWriteFlush(write);

                // No output expected
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"noop\"}", "noop with parameters returned");
                ioWriteLine(write, strNew("{\"out\":[\"bogus\"]}"));
                ioWriteFlush(write);

                // Send output
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"test\"}", "test command");
                ioWriteLine(write, strNew("{\"out\":[\"value1\",\"value2\"]}"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"cmd\":\"exit\"}", "exit command");

                close(pipeRead[1]);
                close(pipeWrite[0]);
            }

            HARNESS_FORK_PARENT()
            {
                close(pipeRead[1]);
                close(pipeWrite[0]);

                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("client read"), pipeRead[0], 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("client write"), pipeWrite[1]));
                ioWriteOpen(write);

                // Various bogus greetings
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), JsonFormatError,
                    "expected '{' but found 'b'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "greeting key 'name' must be string type");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "unable to find greeting key 'name'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value 'pgBackRest' for greeting key 'name' but got 'bogus'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value 'test' for greeting key 'service' but got 'bogus'");
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), ProtocolError,
                    "expected value '" PROJECT_VERSION "' for greeting key 'version' but got 'bogus'");

                // Correct greeting
                ProtocolClient *client = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        client,
                        protocolClientMove(
                            protocolClientNew(strNew("test client"), strNew("test"), read, write), MEM_CONTEXT_OLD()),
                        "create client");
                    TEST_RESULT_VOID(protocolClientMove(NULL, MEM_CONTEXT_OLD()), "move null client");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolClientIoRead(client), client->read, "get read io");
                TEST_RESULT_PTR(protocolClientIoWrite(client), client->write, "get write io");

                // Throw errors
                TEST_ERROR(protocolClientNoOp(client), AssertError, "raised from test client: sample error message");
                TEST_ERROR(protocolClientNoOp(client), UnknownError, "raised from test client: no details available");
                TEST_ERROR(protocolClientNoOp(client), AssertError, "no output required by command");

                // Get command output
                const VariantList *output = NULL;
                KeyValue *command = kvPut(kvNew(), varNewStr(PROTOCOL_COMMAND_STR), varNewStr(strNew("test")));

                TEST_ASSIGN(output, protocolClientExecute(client, command, true), "execute command with output");
                TEST_RESULT_UINT(varLstSize(output), 2, "check output size");
                TEST_RESULT_STR(strPtr(varStr(varLstGet(output, 0))), "value1", "check value1");
                TEST_RESULT_STR(strPtr(varStr(varLstGet(output, 1))), "value2", "check value2");

                // Free client
                TEST_RESULT_VOID(protocolClientFree(client), "free client");
                TEST_RESULT_VOID(protocolClientFree(NULL), "free null client");

                close(pipeRead[0]);
                close(pipeWrite[1]);
            }
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolGet()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "/usr/bin/pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--db-timeout=9");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "archive-push");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        ProtocolClient *client = NULL;

        TEST_ASSIGN(client, protocolGet(remoteTypeRepo, 1), "get protocol");
        TEST_RESULT_PTR(protocolGet(remoteTypeRepo, 1), client, "get cached protocol");
        TEST_RESULT_VOID(protocolFree(), "free protocol objects");
        TEST_RESULT_VOID(protocolFree(), "free protocol objects again");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
