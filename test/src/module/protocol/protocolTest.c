/***********************************************************************************************************************************
Test Protocol
***********************************************************************************************************************************/
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "storage/storage.h"
#include "storage/driver/posix/storage.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test protocol request handler
***********************************************************************************************************************************/
bool
testServerProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, command);
        FUNCTION_HARNESS_PARAM(VARIANT_LIST, paramList);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, strNew("assert")))
        {
            THROW(AssertError, "test assert");
        }
        else if (strEq(command, strNew("request-simple")))
        {
            protocolServerResponse(server, varNewBool(true));
        }
        else if (strEq(command, strNew("request-complex")))
        {
            protocolServerResponse(server, varNewBool(false));
            ioWriteLine(protocolServerIoWrite(server), strNew("LINEOFTEXT"));
            ioWriteFlush(protocolServerIoWrite(server));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RESULT(BOOL, found);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

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
                        "|pgbackrest --command=archive-get --process=0 --type=backup remote")),
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
                        " --config-path=/path/config --process=0 --type=backup remote")),
            "remote protocol params with replacements");
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolCommand"))
    {
        ProtocolCommand *command = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(command, protocolCommandNew(strNew("command1")), "create command");
            TEST_RESULT_PTR(protocolCommandParamAdd(command, varNewStr(strNew("param1"))), command, "add param");
            TEST_RESULT_PTR(protocolCommandParamAdd(command, varNewStr(strNew("param2"))), command, "add param");

            TEST_RESULT_PTR(protocolCommandMove(command, MEM_CONTEXT_OLD()), command, "move protocol command");
            TEST_RESULT_PTR(protocolCommandMove(NULL, MEM_CONTEXT_OLD()), NULL, "move null protocol command");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_STR(strPtr(protocolCommandToLog(command)), "{command: command1}", "check log");
        TEST_RESULT_STR(
            strPtr(protocolCommandJson(command)), "{\"cmd\":\"command1\",\"param\":[\"param1\",\"param2\"]}", "check json");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(command, protocolCommandNew(strNew("command2")), "create command");
        TEST_RESULT_STR(strPtr(protocolCommandToLog(command)), "{command: command2}", "check log");
        TEST_RESULT_STR(strPtr(protocolCommandJson(command)), "{\"cmd\":\"command2\"}", "check json");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolCommandFree(command), "free command");
        TEST_RESULT_VOID(protocolCommandFree(NULL), "free null command");
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
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("server read"), HARNESS_FORK_CHILD_READ(), 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("server write"), HARNESS_FORK_CHILD_WRITE()));
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
                ioWriteLine(write, strNew("{}"));
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
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("client read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0)));
                ioWriteOpen(write);

                // Various bogus greetings
                TEST_ERROR(
                    protocolClientNew(strNew("test client"), strNew("test"), read, write), JsonFormatError,
                    "invalid type at 'bogus greeting'");
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

                TEST_ASSIGN(
                    output,
                    varVarLst(protocolClientExecute(client, protocolCommandNew(strNew("test")), true)),
                    "execute command with output");
                TEST_RESULT_UINT(varLstSize(output), 2, "check output size");
                TEST_RESULT_STR(strPtr(varStr(varLstGet(output, 0))), "value1", "check value1");
                TEST_RESULT_STR(strPtr(varStr(varLstGet(output, 1))), "value2", "check value2");

                // Free client
                TEST_RESULT_VOID(protocolClientFree(client), "free client");
                TEST_RESULT_VOID(protocolClientFree(NULL), "free null client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolServer"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("client read"), HARNESS_FORK_CHILD_READ(), 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("client write"), HARNESS_FORK_CHILD_WRITE()));
                ioWriteOpen(write);

                // Check greeting
                TEST_RESULT_STR(
                    strPtr(ioReadLine(read)), "{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}",
                    "check greeting");

                // Noop
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"noop\"}")), "write noop");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush noop");
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{}", "noop result");

                // Invalid command
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"bogus\"}")), "write bogus");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush bogus");
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"err\":39,\"out\":\"invalid command 'bogus'\"}", "bogus error");

                // Simple request
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"request-simple\"}")), "write simple request");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush simple request");
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"out\":true}", "simple request result");

                // Assert -- no response will come backup because the process loop will terminate
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"assert\"}")), "write assert");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush simple request");

                // Complex request -- after process loop has been restarted
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"request-complex\"}")), "write complex request");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush complex request");
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "{\"out\":false}", "complex request result");
                TEST_RESULT_STR(strPtr(ioReadLine(read)), "LINEOFTEXT", "complex request result");

                // Exit
                TEST_RESULT_VOID(ioWriteLine(write, strNew("{\"cmd\":\"exit\"}")), "write exit");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush exit");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadIo(ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000));
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteIo(ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0)));
                ioWriteOpen(write);

                // Send greeting
                ProtocolServer *server = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        server,
                        protocolServerMove(
                            protocolServerNew(strNew("test server"), strNew("test"), read, write), MEM_CONTEXT_OLD()),
                        "create server");
                    TEST_RESULT_VOID(protocolServerMove(NULL, MEM_CONTEXT_OLD()), "move null server");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolServerIoRead(server), server->read, "get read io");
                TEST_RESULT_PTR(protocolServerIoWrite(server), server->write, "get write io");

                TEST_RESULT_VOID(protocolServerHandlerAdd(server, testServerProtocol), "add handler");

                TEST_ERROR(protocolServerProcess(server), AssertError, "test assert");
                TEST_RESULT_VOID(protocolServerProcess(server), "run process loop again");

                TEST_RESULT_VOID(protocolServerFree(server), "free server");
                TEST_RESULT_VOID(protocolServerFree(NULL), "free null server");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolGet()"))
    {
        // Simple protocol start
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "/usr/bin/pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        ProtocolClient *client = NULL;

        TEST_ASSIGN(client, protocolGet(remoteTypeRepo, 1), "get protocol");
        TEST_RESULT_PTR(protocolGet(remoteTypeRepo, 1), client, "get cached protocol");
        TEST_RESULT_VOID(protocolFree(), "free protocol objects");
        TEST_RESULT_VOID(protocolFree(), "free protocol objects again");

        // Start protocol with local encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteNP(storageTest, strNew("pgbackrest.conf")),
            bufNewStr(
                strNew(
                    "[global]\n"
                    "repo1-cipher-type=aes-256-cbc\n"
                    "repo1-cipher-pass=acbd\n")));

        argList = strLstNew();
        strLstAddZ(argList, "/usr/bin/pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAdd(argList, strNewFmt("--config=%s/pgbackrest.conf", testPath()));
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoCipherPass)), "acbd", "check cipher pass before");
        TEST_ASSIGN(client, protocolGet(remoteTypeRepo, 1), "get protocol");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoCipherPass)), "acbd", "check cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free protocol objects");

        // Start protocol with remote encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteNP(storageTest, strNew("pgbackrest.conf")),
            bufNewStr(
                strNew(
                    "[global]\n"
                    "repo1-cipher-type=aes-256-cbc\n"
                    "repo1-cipher-pass=dcba\n")));

        argList = strLstNew();
        strLstAddZ(argList, "/usr/bin/pgbackrest");
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAdd(argList, strNewFmt("--repo1-host-config=%s/pgbackrest.conf", testPath()));
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", testPath()));
        strLstAddZ(argList, "info");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_PTR(cfgOptionStr(cfgOptRepoCipherPass), NULL, "check cipher pass before");
        TEST_ASSIGN(client, protocolGet(remoteTypeRepo, 1), "get protocol");
        TEST_RESULT_STR(strPtr(cfgOptionStr(cfgOptRepoCipherPass)), "dcba", "check cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free protocol objects");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
