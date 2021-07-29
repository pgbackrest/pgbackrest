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
#include "common/harnessError.h"
#include "common/harnessFork.h"
#include "common/harnessPack.h"

/***********************************************************************************************************************************
Test protocol server command handlers
***********************************************************************************************************************************/
#define TEST_PROTOCOL_COMMAND_ASSERT                                STRID5("assert", 0x2922ce610)

__attribute__((__noreturn__)) static void
testCommandAssertProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    hrnErrorThrowP();

    // No FUNCTION_HARNESS_RETURN_VOID() because the function does not return
}

#define TEST_PROTOCOL_COMMAND_ERROR                                 STRID5("error", 0x127ca450)

__attribute__((__noreturn__)) static void
testCommandErrorProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    hrnErrorThrowP(.errorType = &FormatError);

    // No FUNCTION_HARNESS_RETURN_VOID() because the function does not return
}

#define TEST_PROTOCOL_COMMAND_SIMPLE                                STRID5("c-simple", 0x2b20d4cf630)

static void
testCommandRequestSimpleProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        protocolServerDataPut(server, pckWriteStrP(protocolPackNew(), STRDEF("output")));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

#define TEST_PROTOCOL_COMMAND_COMPLEX                               STRID5("c-complex", 0x182b20d78f630)

static void
testCommandRequestComplexProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TEST_RESULT_UINT(pckReadU32P(param), 87, "param check");
        TEST_RESULT_STR_Z(pckReadStrP(param), "data", "param check");

        TEST_RESULT_VOID(protocolServerDataPut(server, NULL), "sync");

        TEST_RESULT_BOOL(pckReadBoolP(protocolServerDataGet(server)), true, "data get");
        TEST_RESULT_UINT(pckReadModeP(protocolServerDataGet(server)), 0644, "data get");
        TEST_RESULT_PTR(protocolServerDataGet(server), NULL, "data end get");

        TEST_RESULT_VOID(protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), true)), "data put");
        TEST_RESULT_VOID(protocolServerDataPut(server, pckWriteI32P(protocolPackNew(), -1)), "data put");
        TEST_RESULT_VOID(protocolServerDataEndPut(server), "data end put");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

#define TEST_PROTOCOL_COMMAND_RETRY                                 STRID5("retry", 0x19950b20)

static unsigned int testCommandRetryTotal = 1;

static void
testCommandRetryProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(PACK_READ, param);
        FUNCTION_HARNESS_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_HARNESS_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (testCommandRetryTotal > 0)
        {
            testCommandRetryTotal--;
            THROW(FormatError, "error-until-0");
        }

        protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), true));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

#define TEST_PROTOCOL_SERVER_HANDLER_LIST                                                                                          \
    {.command = TEST_PROTOCOL_COMMAND_ASSERT, .handler = testCommandAssertProtocol},                                               \
    {.command = TEST_PROTOCOL_COMMAND_ERROR, .handler = testCommandErrorProtocol},                                                 \
    {.command = TEST_PROTOCOL_COMMAND_SIMPLE, .handler = testCommandRequestSimpleProtocol},                                        \
    {.command = TEST_PROTOCOL_COMMAND_COMPLEX, .handler = testCommandRequestComplexProtocol},                                      \
    {.command = TEST_PROTOCOL_COMMAND_RETRY, .handler = testCommandRetryProtocol},

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
        hrnCfgArgKeyRawZ(argList, cfgOptRepoType, 2, "posix");      // Will not be passed to the remote
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
    if (testBegin("ProtocolClient, ProtocolCommand, and ProtocolServer"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                // Various bogus greetings
                // -----------------------------------------------------------------------------------------------------------------
                ioWriteStrLine(HRN_FORK_CHILD_WRITE(), STRDEF("bogus greeting"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());
                ioWriteStrLine(HRN_FORK_CHILD_WRITE(), STRDEF("{\"name\":999}"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());
                ioWriteStrLine(HRN_FORK_CHILD_WRITE(), STRDEF("{\"name\":null}"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());
                ioWriteStrLine(HRN_FORK_CHILD_WRITE(), STRDEF("{\"name\":\"bogus\"}"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());
                ioWriteStrLine(HRN_FORK_CHILD_WRITE(), STRDEF("{\"name\":\"pgBackRest\",\"service\":\"bogus\"}"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());
                ioWriteStrLine(
                    HRN_FORK_CHILD_WRITE(), STRDEF("{\"name\":\"pgBackRest\",\"service\":\"test\",\"version\":\"bogus\"}"));
                ioWriteFlush(HRN_FORK_CHILD_WRITE());

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("server");

                ProtocolServer *server = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        server,
                        protocolServerMove(
                            protocolServerNew(STRDEF("test server"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()),
                            memContextPrior()),
                        "new server");
                    TEST_RESULT_VOID(protocolServerMove(NULL, memContextPrior()), "move null server");
                }
                MEM_CONTEXT_TEMP_END();

                const ProtocolServerHandler commandHandler[] = {TEST_PROTOCOL_SERVER_HANDLER_LIST};

                // This cannot run in a TEST* macro because tests are run by the command handlers
                protocolServerProcess(server, NULL, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("server with retries");

                VariantList *retryList = varLstNew();
                varLstAdd(retryList, varNewUInt64(0));

                TEST_ASSIGN(
                    server, protocolServerNew(STRDEF("test server"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()),
                    "new server");

                // This cannot run in a TEST* macro because tests are run by the command handlers
                protocolServerProcess(server, retryList, commandHandler, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandHandler));
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("bogus greetings");

                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    JsonFormatError, "expected '{' at 'bogus greeting'");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    ProtocolError, "greeting key 'name' must be string type");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    ProtocolError, "unable to find greeting key 'name'");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    ProtocolError,
                    "expected value 'pgBackRest' for greeting key 'name' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    ProtocolError,
                    "expected value 'test' for greeting key 'service' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");
                TEST_ERROR(
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    ProtocolError,
                    "expected value '" PROJECT_VERSION "' for greeting key 'version' but got 'bogus'\n"
                    "HINT: is the same version of " PROJECT_NAME " installed on the local and remote host?");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("new client with successful handshake");

                ProtocolClient *client = NULL;

                MEM_CONTEXT_TEMP_BEGIN()
                {
                    TEST_ASSIGN(
                        client,
                        protocolClientMove(
                            protocolClientNew(
                                STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                            memContextPrior()),
                        "new client");
                    TEST_RESULT_VOID(protocolClientMove(NULL, memContextPrior()), "move null client");
                }
                MEM_CONTEXT_TEMP_END();

                TEST_RESULT_INT(protocolClientIoReadFd(client), ioReadFd(client->pub.read), "get read fd");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("invalid command");

                TEST_ERROR(
                    protocolClientExecute(client, protocolCommandNew(strIdFromZ(stringIdBit6, "BOGUS")), false), ProtocolError,
                    "raised from test client: invalid command 'BOGUS' (0x38eacd271)");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("command throws assert");

                TEST_ERROR(
                    protocolClientExecute(client, protocolCommandNew(TEST_PROTOCOL_COMMAND_ASSERT), false), AssertError,
                    "raised from test client: ERR_MESSAGE\nERR_STACK_TRACE");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("command throws error");

                TEST_ERROR(
                    protocolClientExecute(client, protocolCommandNew(TEST_PROTOCOL_COMMAND_ERROR), false), FormatError,
                    "raised from test client: ERR_MESSAGE");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("command throws error in debug log level");

                harnessLogLevelSet(logLevelDebug);

                TEST_ERROR(
                    protocolClientExecute(client, protocolCommandNew(TEST_PROTOCOL_COMMAND_ERROR), false), FormatError,
                    "raised from test client: ERR_MESSAGE\nERR_STACK_TRACE");

                harnessLogLevelReset();

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("noop command");

                TEST_RESULT_VOID(protocolClientNoOp(client), "noop");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("simple command");

                TEST_RESULT_STR_Z(
                    pckReadStrP(protocolClientExecute(client, protocolCommandNew(TEST_PROTOCOL_COMMAND_SIMPLE), true)), "output",
                    "execute");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("complex command");

                // Put the command to the server
                ProtocolCommand *command = NULL;
                TEST_ASSIGN(command, protocolCommandNew(TEST_PROTOCOL_COMMAND_COMPLEX), "command");
                TEST_RESULT_VOID(pckWriteU32P(protocolCommandParam(command), 87), "param");
                TEST_RESULT_VOID(pckWriteStrP(protocolCommandParam(command), STRDEF("data")), "param");
                TEST_RESULT_VOID(protocolClientCommandPut(client, command), "command put");

                // Read null data to indicate that the server has started the command and is read to receive data
                TEST_RESULT_PTR(protocolClientDataGet(client), NULL, "command started and ready for data");

                // Write data to the server
                TEST_RESULT_VOID(protocolClientDataPut(client, pckWriteBoolP(protocolPackNew(), true)), "data put");
                TEST_RESULT_VOID(protocolClientDataPut(client, pckWriteModeP(protocolPackNew(), 0644)), "data put");
                TEST_RESULT_VOID(protocolClientDataPut(client, NULL), "data end put");

                // Get data from the server
                TEST_RESULT_BOOL(pckReadBoolP(protocolClientDataGet(client)), true, "data get");
                TEST_RESULT_INT(pckReadI32P(protocolClientDataGet(client)), -1, "data get");
                TEST_RESULT_VOID(protocolClientDataEndGet(client), "data end get");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free client");

                TEST_RESULT_VOID(protocolClientFree(client), "free");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("new client with server retries");

                TEST_ASSIGN(
                    client,
                    protocolClientNew(STRDEF("test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    "new client");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("command with retry");

                TEST_RESULT_BOOL(
                    pckReadBoolP(protocolClientExecute(client, protocolCommandNew(TEST_PROTOCOL_COMMAND_RETRY), true)), true,
                    "execute");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("free client");

                TEST_RESULT_VOID(protocolClientFree(client), "free");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("ProtocolParallel and ProtocolParallelJob"))
    {
        TEST_TITLE("job state transitions");

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
        HRN_FORK_BEGIN(.timeout = 5000)
        {
            // Local 1
            HRN_FORK_CHILD_BEGIN(.prefix = "local server")
            {
                ProtocolServer *server = NULL;
                TEST_ASSIGN(
                    server,
                    protocolServerNew(STRDEF("local server 1"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()),
                    "local server 1");

                // Command with output
                TEST_RESULT_UINT(protocolServerCommandGet(server).id, strIdFromZ(stringIdBit5, "c-one"), "c-one command get");

                // Wait for notify from parent
                HRN_FORK_CHILD_NOTIFY_GET();

                TEST_RESULT_VOID(protocolServerDataPut(server, pckWriteU32P(protocolPackNew(), 1)), "data end put");
                TEST_RESULT_VOID(protocolServerDataEndPut(server), "data end put");

                // Wait for exit
                TEST_RESULT_UINT(protocolServerCommandGet(server).id, PROTOCOL_COMMAND_EXIT, "noop command get");
            }
            HRN_FORK_CHILD_END();

            // Local 2
            HRN_FORK_CHILD_BEGIN(.prefix = "local server")
            {
                ProtocolServer *server = NULL;
                TEST_ASSIGN(
                    server,
                    protocolServerNew(STRDEF("local server 2"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()),
                    "local server 2");

                // Command with output
                TEST_RESULT_UINT(protocolServerCommandGet(server).id, strIdFromZ(stringIdBit5, "c2"), "c2 command get");

                // Wait for notify from parent
                HRN_FORK_CHILD_NOTIFY_GET();

                TEST_RESULT_VOID(protocolServerDataPut(server, pckWriteU32P(protocolPackNew(), 2)), "data end put");
                TEST_RESULT_VOID(protocolServerDataEndPut(server), "data end put");

                // Command with error
                TEST_RESULT_UINT(protocolServerCommandGet(server).id, strIdFromZ(stringIdBit5, "c-three"), "c-three command get");
                TEST_RESULT_VOID(protocolServerError(server, 39, STRDEF("very serious error"), STRDEF("stack")), "error put");

                // Wait for exit
                CHECK(protocolServerCommandGet(server).id == PROTOCOL_COMMAND_EXIT);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "local client")
            {
                TestParallelJobCallback data = {.jobList = lstNewP(sizeof(ProtocolParallelJob *))};
                ProtocolParallel *parallel = NULL;
                TEST_ASSIGN(parallel, protocolParallelNew(2000, testParallelJobCallback, &data), "create parallel");
                TEST_RESULT_STR_Z(protocolParallelToLog(parallel), "{state: pending, clientTotal: 0, jobTotal: 0}", "check log");

                // Add client
                ProtocolClient *client[HRN_FORK_CHILD_MAX];

                for (unsigned int clientIdx = 0; clientIdx < HRN_FORK_PROCESS_TOTAL(); clientIdx++)
                {
                    TEST_ASSIGN(
                        client[clientIdx],
                        protocolClientNew(
                            strNewFmt("local client %u", clientIdx), STRDEF("test"), HRN_FORK_PARENT_READ(clientIdx),
                            HRN_FORK_PARENT_WRITE(clientIdx)),
                        strZ(strNewFmt("local client %u new", clientIdx)));
                    TEST_RESULT_VOID(
                        protocolParallelClientAdd(parallel, client[clientIdx]), strZ(strNewFmt("local client %u add", clientIdx)));
                }

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error on add without an fd");

                // Fake a client without a read fd
                ProtocolClient clientError = {.pub = {.read = ioBufferReadNew(bufNew(0))}, .name = STRDEF("test")};

                TEST_ERROR(protocolParallelClientAdd(parallel, &clientError), AssertError, "client with read fd is required");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("add jobs");

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

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("process jobs with no result");

                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");
                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("result for job 2");

                // Notify child to complete command
                HRN_FORK_PARENT_NOTIFY_PUT(1);

                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job2", "check key is job2");
                TEST_RESULT_BOOL(
                    protocolParallelJobProcessId(job) >= 1 && protocolParallelJobProcessId(job) <= 2, true,
                    "check process id is valid");
                TEST_RESULT_UINT(pckReadU32P(protocolParallelJobResult(job)), 2, "check result is 2");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("error for job 3");

                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job3", "check key is job3");
                TEST_RESULT_INT(protocolParallelJobErrorCode(job), 39, "check error code");
                TEST_RESULT_STR_Z(
                    protocolParallelJobErrorMessage(job), "raised from local client 1: very serious error",
                    "check error message");
                TEST_RESULT_PTR(protocolParallelJobResult(job), NULL, "check result is null");

                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no more results");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("process jobs with no result");

                TEST_RESULT_INT(protocolParallelProcess(parallel), 0, "process jobs");
                TEST_RESULT_PTR(protocolParallelResult(parallel), NULL, "check no result");
                TEST_RESULT_BOOL(protocolParallelDone(parallel), false, "check not done");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("result for job 1");

                // Notify child to complete command
                HRN_FORK_PARENT_NOTIFY_PUT(0);

                TEST_RESULT_INT(protocolParallelProcess(parallel), 1, "process jobs");

                TEST_ASSIGN(job, protocolParallelResult(parallel), "get result");
                TEST_RESULT_STR_Z(varStr(protocolParallelJobKey(job)), "job1", "check key is job1");
                TEST_RESULT_UINT(pckReadU32P(protocolParallelJobResult(job)), 1, "check result is 1");

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

                for (unsigned int clientIdx = 0; clientIdx < HRN_FORK_PROCESS_TOTAL(); clientIdx++)
                    TEST_RESULT_VOID(protocolClientFree(client[clientIdx]), strZ(strNewFmt("free client %u", clientIdx)));
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
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
