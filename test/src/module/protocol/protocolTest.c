/***********************************************************************************************************************************
Test Protocol
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/regExp.h"
#include "storage/storage.h"
#include "storage/posix/storage.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessPack.h"

/***********************************************************************************************************************************
Test protocol request handler
***********************************************************************************************************************************/
static unsigned int testServerProtocolErrorTotal = 0;

static void
testServerAssertProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        THROW(AssertError, "test assert");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerRequestSimpleProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerResultBool(server, true);
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerRequestComplexProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerResponseVar(server, varNewBool(false));
        protocolServerWriteLine(server, STRDEF("LINEOFTEXT"));
        protocolServerWriteLine(server, NULL);
        ioWriteFlush(protocolServerIoWrite(server));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
testServerErrorUntil0Protocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (testServerProtocolErrorTotal > 0)
        {
            testServerProtocolErrorTotal--;
            THROW(FormatError, "error-until-0");
        }

        protocolServerResponseVar(server, varNewBool(true));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Test ParallelJobCallback
***********************************************************************************************************************************/
typedef struct TestParallelJobCallback
{
    List *jobList;                                                  // List of jobs to process
    unsigned int jobIdx;                                            // Current index in the list to be processed
    bool clientSeen[2];                                             // Make sure the client idx was seen
} TestParallelJobCallback;

static ProtocolParallelJob *testParallelJobCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    TestParallelJobCallback *listData = data;

    // Mark the client idx as seen
    listData->clientSeen[clientIdx] = true;

    // Get a new job if there are any left
    if (listData->jobIdx < lstSize(listData->jobList))
    {
        ProtocolParallelJob *job = *(ProtocolParallelJob **)lstGet(listData->jobList, listData->jobIdx);
        listData->jobIdx++;

        FUNCTION_TEST_RETURN(protocolParallelJobMove(job, memContextCurrent()));
    }

    FUNCTION_TEST_RETURN(NULL);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("repoIsLocal() and pgIsLocal()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-path=/repo-local");
        strLstAddZ(argList, "--repo4-path=/remote-host-new");
        strLstAddZ(argList, "--repo4-host=remote-host-new");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .noStd = true);

        TEST_RESULT_BOOL(repoIsLocal(0), true, "repo is local");
        TEST_RESULT_VOID(repoIsLocalVerify(), "    local verified");
        TEST_RESULT_VOID(repoIsLocalVerifyIdx(0), "    local by index verified");
        TEST_ERROR(
            repoIsLocalVerifyIdx(cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1), HostInvalidError,
            "archive-get command must be run on the repository host");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-host=remote-host");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .noStd = true);

        TEST_RESULT_BOOL(repoIsLocal(0), false, "repo is remote");
        TEST_ERROR(repoIsLocalVerify(), HostInvalidError, "archive-get command must be run on the repository host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1 is local");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/path/to");
        strLstAddZ(argList, "--repo1-retention-full=1");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .noStd = true);

        TEST_RESULT_BOOL(pgIsLocal(0), true, "pg is local");
        TEST_RESULT_VOID(pgIsLocalVerify(), "verify pg is local");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg1 is not local");

        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test1");
        hrnCfgArgRawZ(argList, cfgOptPgHost, "test1");
        strLstAddZ(argList, "--pg1-path=/path/to");
        HRN_CFG_LOAD(cfgCmdRestore, argList, .noStd = true);

        TEST_RESULT_BOOL(pgIsLocal(0), false, "pg1 is remote");
        TEST_ERROR(pgIsLocalVerify(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("pg7 is not local");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/bogus");
        strLstAddZ(argList, "--pg7-path=/path/to");
        strLstAddZ(argList, "--pg7-host=test1");
        hrnCfgArgRawZ(argList, cfgOptPg, "7");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypePg);
        strLstAddZ(argList, "--process=0");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleLocal, .noStd = true);

        TEST_RESULT_BOOL(pgIsLocal(1), false, "pg7 is remote");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolHelperClientFree()"))
    {
        TEST_TITLE("free with errors output as warnings");

        // Create and free a mem context to give us an error to use
        MemContext *memContext = memContextNew("test");
        memContextFree(memContext);

        // Create bogus client and exec with the freed memcontext to generate errors
        ProtocolClient client = {.pub = {.memContext = memContext}, .name = STRDEF("test")};
        Exec exec = {.pub = {.memContext = memContext}, .name = STRDEF("test"), .command = strNewZ("test")};
        ProtocolHelperClient protocolHelperClient = {.client = &client, .exec = &exec};

        TEST_RESULT_VOID(protocolHelperClientFree(&protocolHelperClient), "free");

        TEST_RESULT_LOG(
            "P00   WARN: cannot free inactive context\n"
            "P00   WARN: cannot free inactive context");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolLocalParam()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolLocalParam(protocolStorageTypeRepo, 0, 0),
            "--exec-id=1-test\n--log-level-console=off\n--log-level-file=off\n--log-level-stderr=error\n--pg1-path=/path/to/pg\n"
                "--process=0\n--remote-type=repo\n--stanza=test1\narchive-get:local\n",
            "local repo protocol params");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/pg");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--log-subprocess");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolLocalParam(protocolStorageTypePg, 0, 1),
            "--exec-id=1-test\n--log-level-console=off\n--log-level-file=info\n--log-level-stderr=error\n--log-subprocess\n--pg=1\n"
                "--pg1-path=/pg\n--process=1\n--remote-type=pg\n--stanza=test1\nbackup:local\n",
            "local pg protocol params");
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolRemoteParam() and protocolRemoteParamSsh()"))
    {
        storagePutP(storageNewWriteP(storageTest, STRDEF("pgbackrest.conf")), bufNew(0));

        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        // Local config settings should never be passed to the remote
        strLstAddZ(argList, "--config=" TEST_PATH "/pgbackrest.conf");
        strLstAddZ(argList, "--config-include-path=" TEST_PATH);
        strLstAddZ(argList, "--config-path=" TEST_PATH);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\nrepo-host-user@repo-host\n"
                TEST_PROJECT_EXE " --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/pg --process=0 --remote-type=repo --repo=1 --stanza=test1 archive-get:remote\n",
            "remote protocol params");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--log-subprocess");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/unused");            // Will be passed to remote (required)
        hrnCfgArgRawZ(argList, cfgOptPgPort, "777");                // Not passed to remote (required but has default)
        strLstAddZ(argList, "--repo1-host=repo-host");
        strLstAddZ(argList, "--repo1-host-port=444");
        strLstAddZ(argList, "--repo1-host-config=/path/pgbackrest.conf");
        strLstAddZ(argList, "--repo1-host-config-include-path=/path/include");
        strLstAddZ(argList, "--repo1-host-config-path=/path/config");
        strLstAddZ(argList, "--repo1-host-user=repo-host-user");
        HRN_CFG_LOAD(cfgCmdCheck, argList, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\n-p\n444\nrepo-host-user@repo-host\n"
                TEST_PROJECT_EXE " --config=/path/pgbackrest.conf --config-include-path=/path/include --config-path=/path/config"
                " --exec-id=1-test --log-level-console=off --log-level-file=info --log-level-stderr=error --log-subprocess"
                " --pg1-path=/unused --process=0 --remote-type=repo --repo=1 --stanza=test1 check:remote\n",
            "remote protocol params with replacements");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--process=3");
        hrnCfgArgRawZ(argList, cfgOptRepo, "1");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypeRepo);
        strLstAddZ(argList, "--repo1-host=repo-host");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleLocal, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypeRepo, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npgbackrest@repo-host\n"
                TEST_PROJECT_EXE " --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/pg --process=3 --remote-type=repo --repo=1 --stanza=test1 archive-get:remote\n",
            "remote protocol params for backup local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg1-host=pg1-host");
        strLstAddZ(argList, "--repo1-retention-full=1");
        HRN_CFG_LOAD(cfgCmdBackup, argList, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypePg, 0),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg1-host\n"
                TEST_PROJECT_EXE " --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/1 --process=0 --remote-type=pg --stanza=test1 backup:remote\n",
            "remote protocol params for db backup");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--process=4");
        hrnCfgArgRawZ(argList, cfgOptPg, "2");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg1-socket-path=/socket3");
        strLstAddZ(argList, "--pg1-port=1111");
        strLstAddZ(argList, "--pg2-path=/path/to/2");
        strLstAddZ(argList, "--pg2-host=pg2-host");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypePg);
        HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleLocal, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypePg, 1),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg2-host\n"
                TEST_PROJECT_EXE " --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/2 --process=4 --remote-type=pg --stanza=test1 backup:remote\n",
            "remote protocol params for db local");

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--process=4");
        hrnCfgArgRawZ(argList, cfgOptPg, "3");
        strLstAddZ(argList, "--pg1-path=/path/to/1");
        strLstAddZ(argList, "--pg3-path=/path/to/3");
        strLstAddZ(argList, "--pg3-host=pg3-host");
        strLstAddZ(argList, "--pg3-socket-path=/socket3");
        strLstAddZ(argList, "--pg3-port=3333");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypePg);
        HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleLocal, .noStd = true);

        TEST_RESULT_STRLST_Z(
            protocolRemoteParamSsh(protocolStorageTypePg, 1),
            "-o\nLogLevel=error\n-o\nCompression=no\n-o\nPasswordAuthentication=no\npostgres@pg3-host\n"
                TEST_PROJECT_EXE " --exec-id=1-test --log-level-console=off --log-level-file=off --log-level-stderr=error"
                " --pg1-path=/path/to/3 --pg1-port=3333 --pg1-socket-path=/socket3 --process=4 --remote-type=pg --stanza=test1"
                " backup:remote\n",
            "remote protocol params for db local");
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolClient"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(STRDEF("server read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Various bogus greetings
                ioWriteStrLine(write, STRDEF("bogus greeting"));
                ioWriteFlush(write);
                ioWriteStrLine(write, STRDEF("{\"name\":999}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, STRDEF("{\"name\":null}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, STRDEF("{\"name\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, STRDEF("{\"name\":\"pgBackRest\",\"service\":\"bogus\"}"));
                ioWriteFlush(write);
                ioWriteStrLine(write, STRDEF("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"bogus\"}"));
                ioWriteFlush(write);

                // Correct greeting with noop
                ioWriteStrLine(write, STRDEF("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop");
                ioWriteStrLine(write, STRDEF("{}"));
                ioWriteFlush(write);

                // Throw errors
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop with error text");
                ioWriteStrLine(write, STRDEF("{\"err\":25,\"out\":\"sample error message\",\"errStack\":\"stack data\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop with no error text");
                ioWriteStrLine(write, STRDEF("{\"err\":255}"));
                ioWriteFlush(write);

                // No output expected
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop with parameters returned");
                ioWriteStrLine(write, STRDEF("{\"out\":[\"bogus\"]}"));
                ioWriteFlush(write);

                // Send output !!! REMOVE WHEN NO VAR
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:test", "test command");
                ioWriteStrLine(write, STRDEF(".OUTPUT"));
                ioWriteStrLine(write, STRDEF("{\"out\":[\"value1\",\"value2\"]}"));
                ioWriteFlush(write);

                // Send output
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:test", "test command");

                PackWrite *resultPayloadPack = pckWriteNewBuf(bufNew(512));
                pckWriteStrP(resultPayloadPack, STRDEF("value1"));
                pckWriteStrP(resultPayloadPack, STRDEF("value2"));
                pckWriteEndP(resultPayloadPack);

                PackWrite *resultPack = pckWriteNew(write);
                pckWriteU32P(resultPack, protocolServerTypeResult, .defaultWrite = true);
                pckWritePackP(resultPack, resultPayloadPack);
                pckWriteEndP(resultPack);

                PackWrite *responsePack = pckWriteNew(write);
                pckWriteU32P(responsePack, protocolServerTypeResponse, .defaultWrite = true);
                pckWriteEndP(responsePack);

                ioWriteFlush(write);

                // invalid line
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:invalid-line", "invalid line command");
                ioWrite(write, LF_BUF);
                ioWriteFlush(write);

                // error instead of output
                TEST_RESULT_STR_Z(
                    hrnPackToStr(pckReadNew(read)), "1:str:err-i-o", "error instead of output command");
                ioWriteStrLine(write, STRDEF("{\"err\":255}"));
                ioWriteFlush(write);

                // unexpected output
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:unexp-output", "unexpected output");
                ioWriteStrLine(write, STRDEF("{}"));
                ioWriteFlush(write);

                // invalid prefix
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:i-pr", "invalid prefix");
                ioWriteStrLine(write, STRDEF("~line"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:exit", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(STRDEF("client read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("client write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Various bogus greetings
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), JsonFormatError,
                    "expected '{' at 'bogus greeting'");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), ProtocolError,
                    "greeting key 'name' must be string type");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), ProtocolError,
                    "unable to find greeting key 'name'");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), ProtocolError,
                    "expected value 'pgBackRest' for greeting key 'name' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), ProtocolError,
                    "expected value 'test' for greeting key 'service' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), ProtocolError,
                    "expected value '" PROJECT_VERSION "' for greeting key 'version' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");

                // Correct greeting
                ProtocolClient *client = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        client,
                        protocolClientMove(
                            protocolClientNew(STRDEF("test client"), STRDEF("test"), read, write), memContextPrior()),
                        "create client");
                    TEST_RESULT_VOID(protocolClientMove(NULL, memContextPrior()), "move null client");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolClientIoRead(client), client->pub.read, "get read io");
                TEST_RESULT_PTR(protocolClientIoWrite(client), client->pub.write, "get write io");

                // Throw errors
                TEST_ERROR(
                    protocolClientNoOp(client), AssertError,
                    "raised from test client: sample error message\nstack data");

                harnessLogLevelSet(logLevelDebug);
                TEST_ERROR(
                    protocolClientNoOp(client), UnknownError,
                    "raised from test client: no details available\nno stack trace available");
                harnessLogLevelReset();

                // No output expected
                TEST_ERROR(protocolClientNoOp(client), AssertError, "no output required by command");

                // Get command output !!! REMOVE WHEN NO VAR
                const VariantList *output = NULL;

                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "test"))),
                    "execute command with output");
                TEST_RESULT_STR_Z(protocolClientReadLine(client), "OUTPUT", "check output");
                TEST_ASSIGN(output, varVarLst(protocolClientReadOutputVar(client, true)), "execute command with output");
                TEST_RESULT_UINT(varLstSize(output), 2, "check output size");
                TEST_RESULT_STR_Z(varStr(varLstGet(output, 0)), "value1", "check value1");
                TEST_RESULT_STR_Z(varStr(varLstGet(output, 1)), "value2", "check value2");

                // Get command output
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "test"))),
                    "execute command with output");
                TEST_RESULT_STR_Z(hrnPackToStr(protocolClientResult(client)), "1:str:value1, 2:str:value2", "    result");
                TEST_RESULT_VOID(protocolClientResponse(client), "    response");

                // Invalid line
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "invalid-line"))),
                    "execute command that returns invalid line");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "unexpected empty line");

                // Error instead of output
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "err-i-o"))),
                    "execute command that returns error instead of output");
                TEST_ERROR(protocolClientReadLine(client), UnknownError, "raised from test client: no details available");

                // Unexpected output
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "unexp-output"))),
                    "execute command that returns unexpected output");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "expected error but got output");

                // Invalid prefix
                TEST_RESULT_VOID(
                    protocolClientWriteCommand(client, protocolCommandNew(strIdFromZ(stringIdBit5, "i-pr"))),
                    "execute command that returns an invalid prefix");
                TEST_ERROR(protocolClientReadLine(client), FormatError, "invalid prefix in '~line'");

                // Free client
                TEST_RESULT_VOID(protocolClientFree(client), "free client");
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
                IoRead *read = ioFdReadNew(STRDEF("client read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("client write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Check greeting
                TEST_RESULT_STR_Z(
                    ioReadLine(read), "{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}",
                    "check greeting");

                // Noop
                ProtocolCommand *command = protocolCommandNew(strIdFromZ(stringIdBit5, "noop"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write noop");
                TEST_RESULT_STR_Z(ioReadLine(read), "{}", "noop result");

                TEST_RESULT_VOID(protocolCommandFree(command), "free protocol");

                // Invalid command
                KeyValue *result = NULL;

                command = protocolCommandNew(strIdFromZ(stringIdBit5, "bogus"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write bogus");
                TEST_ASSIGN(result, varKv(jsonToVar(ioReadLine(read))), "parse error result");
                TEST_RESULT_INT(varIntForce(kvGet(result, VARSTRDEF("err"))), 39, "    check code");
                TEST_RESULT_STR_Z(
                    varStr(kvGet(result, VARSTRDEF("out"))), "invalid command 'bogus' (0x13a9de20)", "    check message");
                TEST_RESULT_BOOL(kvGet(result, VARSTRDEF("errStack")) != NULL, true, "    check stack exists");

                // Simple request
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "r-s"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write simple request");
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:u32:0, 2:pack:<1:bool:true>", "simple request result");
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:u32:1", "simple request response");

                // Throw an assert error which will include a stack trace
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "assert"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write assert");
                TEST_ASSIGN(result, varKv(jsonToVar(ioReadLine(read))), "parse error result");
                TEST_RESULT_INT(varIntForce(kvGet(result, VARSTRDEF("err"))), 25, "    check code");
                TEST_RESULT_STR_Z(varStr(kvGet(result, VARSTRDEF("out"))), "test assert", "    check message");
                TEST_RESULT_BOOL(kvGet(result, VARSTRDEF("errStack")) != NULL, true, "    check stack exists");

                // Complex request -- after process loop has been restarted
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "r-c"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write complex request");
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"out\":false}", "complex request result");
                TEST_RESULT_STR_Z(ioReadLine(read), ".LINEOFTEXT", "complex request result");
                TEST_RESULT_STR_Z(ioReadLine(read), ".", "complex request result");

                // Exit
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "exit"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write exit");
                TEST_RESULT_VOID(ioWriteFlush(write), "flush exit");

                // Retry errors until success
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "ezero"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write error-until-0");
                TEST_RESULT_STR_Z(ioReadLine(read), "{\"out\":true}", "error-until-0 result");

                // Exit
                command = protocolCommandNew(strIdFromZ(stringIdBit5, "exit"));
                pckWriteEndP(protocolCommandParam(command));

                TEST_RESULT_VOID(protocolCommandWrite(command, write), "write exit");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(STRDEF("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Send greeting
                ProtocolServer *server = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        server,
                        protocolServerMove(
                            protocolServerNew(STRDEF("test server"), STRDEF("test"), read, write), memContextPrior()),
                        "create server");
                    TEST_RESULT_VOID(protocolServerMove(NULL, memContextPrior()), "move null server");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_PTR(protocolServerIoRead(server), server->pub.read, "get read io");
                TEST_RESULT_PTR(protocolServerIoWrite(server), server->pub.write, "get write io");

                ProtocolServerHandler commandHandler[] =
                {
                    {.command = strIdFromZ(stringIdBit5, "assert"), .handler = testServerAssertProtocol},
                    {.command = strIdFromZ(stringIdBit5, "r-s"), .handler = testServerRequestSimpleProtocol},
                    {.command = strIdFromZ(stringIdBit5, "r-c"), .handler = testServerRequestComplexProtocol},
                    {.command = strIdFromZ(stringIdBit5, "ezero"), .handler = testServerErrorUntil0Protocol},
                };

                TEST_RESULT_VOID(
                    protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler)),
                    "run process loop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("run process loop with retries");

                VariantList *retryInterval = varLstNew();
                varLstAdd(retryInterval, varNewUInt64(0));
                varLstAdd(retryInterval, varNewUInt64(50));

                testServerProtocolErrorTotal = 2;

                TEST_RESULT_VOID(
                    protocolServerProcess(server, retryInterval, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler)),
                    "run process loop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free server");

                TEST_RESULT_VOID(protocolServerFree(server), "free");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolParallel and ProtocolParallelJob"))
    {
        ProtocolParallelJob *job = NULL;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(
                job,
                protocolParallelJobNew(VARSTRDEF("test"), protocolCommandNew(strIdFromZ(stringIdBit5, "c"))), "new job");
            TEST_RESULT_PTR(protocolParallelJobMove(job, memContextPrior()), job, "move job");
            TEST_RESULT_PTR(protocolParallelJobMove(NULL, memContextPrior()), NULL, "move null job");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_ERROR(
            protocolParallelJobStateSet(job, protocolParallelJobStateDone), AssertError,
            "invalid state transition from 'pending' to 'done'");
        TEST_RESULT_VOID(protocolParallelJobStateSet(job, protocolParallelJobStateRunning), "transition to running");
        TEST_ERROR(
            protocolParallelJobStateSet(job, protocolParallelJobStatePending), AssertError,
            "invalid state transition from 'running' to 'pending'");

        // Free job
        TEST_RESULT_VOID(protocolParallelJobFree(job), "free job");

        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            // Local 1
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(STRDEF("server read"), HARNESS_FORK_CHILD_READ(), 10000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Greeting with noop
                ioWriteStrLine(write, STRDEF("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop");
                ioWriteStrLine(write, STRDEF("{}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(
                    hrnPackToStr(pckReadNew(read)), "1:str:c-one, 2:pack:<1:str:param1, 2:str:param2>", "command1");
                sleepMSec(4000);
                ioWriteStrLine(write, STRDEF("{\"out\":1}"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:exit", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            // Local 2
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioFdReadNew(STRDEF("server read"), HARNESS_FORK_CHILD_READ(), 10000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(STRDEF("server write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Greeting with noop
                ioWriteStrLine(write, STRDEF("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"" PROJECT_VERSION "\"}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:noop", "noop");
                ioWriteStrLine(write, STRDEF("{}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:c2, 2:pack:<1:str:param1>", "command2");
                sleepMSec(1000);
                ioWriteStrLine(write, STRDEF("{\"out\":2}"));
                ioWriteFlush(write);

                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:c-three, 2:pack:<1:str:param1>", "command3");

                ioWriteStrLine(write, STRDEF("{\"err\":39,\"out\":\"very serious error\"}"));
                ioWriteFlush(write);

                // Wait for exit
                TEST_RESULT_STR_Z(hrnPackToStr(pckReadNew(read)), "1:str:exit", "exit command");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                // -----------------------------------------------------------------------------------------------------------------
                TestParallelJobCallback data = {.jobList = lstNewP(sizeof(ProtocolParallelJob *))};
                ProtocolParallel *parallel = NULL;
                TEST_ASSIGN(parallel, protocolParallelNew(2000, testParallelJobCallback, &data), "create parallel");
                TEST_RESULT_STR_Z(protocolParallelToLog(parallel), "{state: pending, clientTotal: 0, jobTotal: 0}", "check log");

                // Add client
                unsigned int clientTotal = 2;
                ProtocolClient *client[HARNESS_FORK_CHILD_MAX];

                for (unsigned int clientIdx = 0; clientIdx < clientTotal; clientIdx++)
                {
                    IoRead *read = ioFdReadNew(
                        strNewFmt("client %u read", clientIdx), HARNESS_FORK_PARENT_READ_PROCESS(clientIdx), 2000);
                    ioReadOpen(read);
                    IoWrite *write = ioFdWriteNew(
                        strNewFmt("client %u write", clientIdx), HARNESS_FORK_PARENT_WRITE_PROCESS(clientIdx), 2000);
                    ioWriteOpen(write);

                    TEST_ASSIGN(
                        client[clientIdx],
                        protocolClientNew(strNewFmt("test client %u", clientIdx), STRDEF("test"), read, write),
                        strZ(strNewFmt("create client %u", clientIdx)));
                    TEST_RESULT_VOID(
                        protocolParallelClientAdd(parallel, client[clientIdx]), strZ(strNewFmt("add client %u", clientIdx)));
                }

                // Attempt to add client without an fd
                const String *protocolString = STRDEF(
                    "{\"name\":\"pgBackRest\",\"service\":\"error\",\"version\":\"" PROJECT_VERSION "\"}\n"
                    "{}\n");

                IoRead *read = ioBufferReadNew(BUFSTR(protocolString));
                ioReadOpen(read);
                IoWrite *write = ioBufferWriteNew(bufNew(1024));
                ioWriteOpen(write);

                ProtocolClient *clientError = protocolClientNew(STRDEF("error"), STRDEF("error"), read, write);
                TEST_ERROR(protocolParallelClientAdd(parallel, clientError), AssertError, "client with read fd is required");
                protocolClientFree(clientError);

                // Add jobs
                ProtocolCommand *command = protocolCommandNew(strIdFromZ(stringIdBit5, "c-one"));
                pckWriteStrP(protocolCommandParam(command), STRDEF("param1"));
                pckWriteStrP(protocolCommandParam(command), STRDEF("param2"));

                ProtocolParallelJob *job = protocolParallelJobNew(varNewStr(STRDEF("job1")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                command = protocolCommandNew(strIdFromZ(stringIdBit5, "c2"));
                pckWriteStrP(protocolCommandParam(command), STRDEF("param1"));

                job = protocolParallelJobNew(varNewStr(STRDEF("job2")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                command = protocolCommandNew(strIdFromZ(stringIdBit5, "c-three"));
                pckWriteStrP(protocolCommandParam(command), STRDEF("param1"));

                job = protocolParallelJobNew(varNewStr(STRDEF("job3")), command);
                TEST_RESULT_VOID(lstAdd(data.jobList, &job), "add job");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job2", "check key is job2");
                TEST_RESULT_BOOL(
                    protocolParallelJobProcessId(job) >= 1 && protocolParallelJobProcessId(job) <= 2, true,
                    "check process id is valid");
                TEST_RESULT_INT(varIntForce(protocolParallelJobResult(job)), 2, "check result is 2");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job3", "check key is job3");
                TEST_RESULT_INT(protocolParallelJobErrorCode(job), 39, "check error code");
                TEST_RESULT_STR_Z(
                    protocolParallelJobErrorMessage(job), "raised from test client 1: very serious error",
                    "check error message");
                TEST_RESULT_PTR(protocolParallelJobResult(job), NULL, "check result is null");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), false, "check not done");

                // Process jobs
                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job1", "check key is job1");
                TEST_RESULT_INT(varIntForce(protocolParallelJobResult(job)), 1, "check result is 1");

                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check done");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check still done");

                TEST_RESULT_VOID(protocolParallelFree(parallel), "free parallel");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("process zero jobs");

                data = (TestParallelJobCallback){.jobList = lstNewP(sizeof(ProtocolParallelJob *))};
                TEST_ASSIGN(parallel, protocolParallelNew(2000, testParallelJobCallback, &data), "create parallel");
                TEST_RESULT_VOID(protocolParallelClientAdd(parallel, client[0]), "add client");

                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process zero jobs");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), true, "check done");

                TEST_RESULT_VOID(protocolParallelFree(parallel), "free parallel");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free clients");

                for (unsigned int clientIdx = 0; clientIdx < clientTotal; clientIdx++)
                    TEST_RESULT_VOID(protocolClientFree(client[clientIdx]), strZ(strNewFmt("free client %u", clientIdx)));
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("protocolGet()"))
    {
        // Call remote free before any remotes exist
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolRemoteFree(1), "free remote (non exist)");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("free local that does not exist");

        TEST_RESULT_VOID(protocolLocalFree(2), "free");

        // Call keep alive before any remotes exist
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(protocolKeepAlive(), "keep alive");

        // Simple protocol start
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAddZ(argList, "--repo1-host-user=" TEST_USER);
        strLstAddZ(argList, "--repo1-path=" TEST_PATH);
        HRN_CFG_LOAD(cfgCmdInfo, argList);

        ProtocolClient *client = NULL;

        TEST_RESULT_VOID(protocolFree(), "free protocol objects before anything has been created");

        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get remote protocol");
        TEST_RESULT_PTR(protocolRemoteGet(protocolStorageTypeRepo, 0), client, "get remote cached protocol");
        TEST_RESULT_PTR(protocolHelper.clientRemote[0].client, client, "check position in cache");
        TEST_RESULT_VOID(protocolKeepAlive(), "keep alive");
        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");
        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects again");

        // Start protocol with local encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteP(storageTest, STRDEF("pgbackrest.conf")),
            BUFSTRDEF(
                "[global]\n"
                "repo1-cipher-type=aes-256-cbc\n"
                "repo1-cipher-pass=acbd\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--config=" TEST_PATH "/pgbackrest.conf");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAddZ(argList, "--repo1-host-user=" TEST_USER);
        strLstAddZ(argList, "--repo1-path=" TEST_PATH);
        strLstAddZ(argList, "--process=999");
        hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypePg);
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList, .role = cfgCmdRoleLocal);

        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoCipherPass), "acbd", "check cipher pass before");
        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get remote protocol");
        TEST_RESULT_PTR(protocolHelper.clientRemote[0].client, client, "check position in cache");
        TEST_RESULT_STR_Z(cfgOptionStr(cfgOptRepoCipherPass), "acbd", "check cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");

        // Start protocol with remote encryption settings
        // -------------------------------------------------------------------------------------------------------------------------
        storagePut(
            storageNewWriteP(storageTest, STRDEF("pgbackrest.conf")),
            BUFSTRDEF(
                "[global]\n"
                "repo1-cipher-type=aes-256-cbc\n"
                "repo1-cipher-pass=dcba\n"
                "repo2-cipher-type=aes-256-cbc\n"
                "repo2-cipher-pass=xxxx\n"));

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--pg1-path=/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-host-config=" TEST_PATH "/pgbackrest.conf");
        strLstAddZ(argList, "--repo1-host=localhost");
        strLstAddZ(argList, "--repo1-host-user=" TEST_USER);
        strLstAddZ(argList, "--repo1-path=" TEST_PATH);
        strLstAddZ(argList, "--repo2-host-config=" TEST_PATH "/pgbackrest.conf");
        strLstAddZ(argList, "--repo2-host=localhost");
        strLstAddZ(argList, "--repo2-host-user=" TEST_USER);
        strLstAddZ(argList, "--repo2-path=" TEST_PATH "2");
        HRN_CFG_LOAD(cfgCmdCheck, argList);

        TEST_RESULT_PTR(cfgOptionIdxStrNull(cfgOptRepoCipherPass, 0), NULL, "check repo1 cipher pass before");
        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypeRepo, 0), "get repo1 remote protocol");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoCipherPass, 0), "dcba", "check repo1 cipher pass after");

        TEST_RESULT_PTR(cfgOptionIdxStrNull(cfgOptRepoCipherPass, 1), NULL, "check repo2 cipher pass before");
        TEST_RESULT_VOID(protocolRemoteGet(protocolStorageTypeRepo, 1), "get repo2 remote protocol");
        TEST_RESULT_STR_Z(cfgOptionIdxStr(cfgOptRepoCipherPass, 1), "xxxx", "check repo2 cipher pass after");

        TEST_RESULT_VOID(protocolFree(), "free remote protocol objects");

        // Start remote protocol
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-host=localhost");
        strLstAddZ(argList, "--pg1-host-user=" TEST_USER);
        strLstAddZ(argList, "--pg1-path=" TEST_PATH);
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        TEST_ASSIGN(client, protocolRemoteGet(protocolStorageTypePg, 0), "get remote protocol");

        TEST_RESULT_VOID(protocolFree(), "free local and remote protocol objects");

        // Start local protocol
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=db");
        hrnCfgArgRawZ(argList, cfgOptPgPath, "/path/to/pg");
        strLstAddZ(argList, "--protocol-timeout=10");
        strLstAddZ(argList, "--process-max=2");
        HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

        TEST_ASSIGN(client, protocolLocalGet(protocolStorageTypeRepo, 0, 1), "get local protocol");
        TEST_RESULT_PTR(protocolLocalGet(protocolStorageTypeRepo, 0, 1), client, "get local cached protocol");
        TEST_RESULT_PTR(protocolHelper.clientLocal[0].client, client, "check location in cache");

        TEST_RESULT_VOID(protocolFree(), "free local and remote protocol objects");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
