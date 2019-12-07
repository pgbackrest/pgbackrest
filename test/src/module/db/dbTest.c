/***********************************************************************************************************************************
Test Database
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessLog.h"
#include "common/harnessPq.h"

#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // PQfinish() is strictly checked
    harnessPqScriptStrictSet(true);

    // *****************************************************************************************************************************
    if (testBegin("Db and dbProtocol()"))
    {
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, true)
            {
                IoRead *read = ioHandleReadNew(strNew("client read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("client write"), HARNESS_FORK_CHILD_WRITE());
                ioWriteOpen(write);

                // Set options
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                strLstAddZ(argList, "--pg1-path=/path/to/pg");
                strLstAddZ(argList, "--command=backup");
                strLstAddZ(argList, "--type=db");
                strLstAddZ(argList, "--process=0");
                harnessCfgLoad(cfgCmdRemote, argList);

                // Set script
                harnessPqScriptSet((HarnessPq [])
                {
                    HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
                    HRNPQ_MACRO_SET_SEARCH_PATH(1),
                    HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_84, "/pgdata", NULL, NULL),
                    HRNPQ_MACRO_CLOSE(1),

                    HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
                    HRNPQ_MACRO_SET_SEARCH_PATH(1),
                    HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_84, "/pgdata", NULL, NULL),
                    HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000030000000200000003"),
                    HRNPQ_MACRO_CLOSE(1),

                    HRNPQ_MACRO_DONE()
                });

                // Create server
                ProtocolServer *server = NULL;

                TEST_ASSIGN(server, protocolServerNew(strNew("db test server"), strNew("test"), read, write), "create server");
                TEST_RESULT_VOID(protocolServerHandlerAdd(server, dbProtocol), "add handler");
                TEST_RESULT_VOID(protocolServerProcess(server), "run process loop");
                TEST_RESULT_VOID(protocolServerFree(server), "free server");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioHandleReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioHandleWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0));
                ioWriteOpen(write);

                // Create client
                ProtocolClient *client = NULL;
                Db *db = NULL;

                TEST_ASSIGN(client, protocolClientNew(strNew("db test client"), strNew("test"), read, write), "create client");

                // Open and free database
                TEST_ASSIGN(db, dbNew(NULL, client, strNew("test")), "create db");
                TEST_RESULT_VOID(dbOpen(db), "open db");
                TEST_RESULT_VOID(dbFree(db), "free db");

                // Open the database, but don't free it so the server is force to do it on shutdown
                TEST_ASSIGN(db, dbNew(NULL, client, strNew("test")), "create db");
                TEST_RESULT_VOID(dbOpen(db), "open db");
                TEST_RESULT_STR(strPtr(dbWalSwitch(db)), "000000030000000200000003", "    wal switch");
                TEST_RESULT_VOID(memContextCallbackClear(db->memContext), "clear context so close is not called");

                TEST_RESULT_VOID(protocolClientFree(client), "free client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("dbGet()"))
    {
        DbGetResult result = {0};

        // Error connecting to primary
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR(dbGet(true, true, false), DbConnectError, "unable to find primary cluster - cannot proceed");
        harnessLogResult(
            "P00   WARN: unable to check pg-1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432': error");

        // Only cluster is a standby
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
            HRNPQ_MACRO_SET_SEARCH_PATH(1),
            HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_94, "/pgdata", NULL, NULL),
            HRNPQ_MACRO_SET_APPLICATION_NAME(1),
            HRNPQ_MACRO_IS_STANDBY_QUERY(1, true),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(true, true, false), DbConnectError, "unable to find primary cluster - cannot proceed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby cluster required but not found");

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
            HRNPQ_MACRO_SET_SEARCH_PATH(1),
            HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_94, "/pgdata", NULL, NULL),
            HRNPQ_MACRO_SET_APPLICATION_NAME(1),
            HRNPQ_MACRO_IS_STANDBY_QUERY(1, false),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(false, false, true), DbConnectError, "unable to find standby cluster - cannot proceed");

        // Primary cluster found
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_84(1, "dbname='postgres' port=5432", "/pgdata", NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(result, dbGet(true, true, false), "get primary only");

        TEST_RESULT_INT(result.primaryId, 1, "    check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "    check primary");
        TEST_RESULT_INT(result.standbyId, 0, "    check standby id");
        TEST_RESULT_BOOL(result.standby == NULL, true, "    check standby");
        TEST_RESULT_INT(dbPgVersion(result.primary), PG_VERSION_84, "    version set");
        TEST_RESULT_STR(strPtr(dbPgDataPath(result.primary)), "/pgdata", "    path set");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");

        // More than one primary found
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg1");
        strLstAddZ(argList, "--pg8-path=/path/to/pg2");
        strLstAddZ(argList, "--pg8-port=5433");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_84(1, "dbname='postgres' port=5432", "/pgdata", NULL, NULL),
            HRNPQ_MACRO_OPEN_84(8, "dbname='postgres' port=5433", "/pgdata", NULL, NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(true, true, false), DbConnectError, "more than one primary cluster found");

        // Two standbys found but no primary
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_92(8, "dbname='postgres' port=5433", "/pgdata", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(false, true, false), DbConnectError, "unable to find primary cluster - cannot proceed");

        // Two standbys and primary not required
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_92(8, "dbname='postgres' port=5433", "/pgdata", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(result, dbGet(false, false, false), "get standbys");

        TEST_RESULT_INT(result.primaryId, 0, "    check primary id");
        TEST_RESULT_BOOL(result.primary == NULL, true, "    check primary");
        TEST_RESULT_INT(result.standbyId, 1, "    check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "    check standby");

        TEST_RESULT_VOID(dbFree(result.standby), "free standby");

        // Primary and standby found
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/path/to/pg1");
        strLstAddZ(argList, "--pg4-path=/path/to/pg4");
        strLstAddZ(argList, "--pg4-port=5433");
        strLstAddZ(argList, "--pg5-host=localhost");
        strLstAdd(argList, strNewFmt("--pg5-host-user=%s", testUser()));
        strLstAddZ(argList, "--pg5-path=/path/to/pg5");
        strLstAddZ(argList, "--pg8-path=/path/to/pg8");
        strLstAddZ(argList, "--pg8-port=5434");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_92(1, "dbname='postgres' port=5432", "/pgdata", true, NULL, NULL),

            // pg-4 error
            {.session = 4, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 4, .function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.session = 4, .function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.session = 4, .function = HRNPQ_FINISH},

            HRNPQ_MACRO_OPEN_92(8, "dbname='postgres' port=5434", "/pgdata", false, NULL, NULL),

            HRNPQ_MACRO_CREATE_RESTORE_POINT(8, "2/3"),
            HRNPQ_MACRO_WAL_SWITCH(8, "xlog", "000000010000000200000003"),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(result, dbGet(false, true, false), "get primary and standy");

        hrnLogReplaceAdd("No such file or directory.*$", NULL, "NO SUCH FILE OR DIRECTORY", false);
        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg-4: [DbConnectError] unable to connect to 'dbname='postgres' port=5433': error\n"
            "P00   WARN: unable to check pg-5: [DbConnectError] raised from remote-0 protocol on 'localhost':"
                " unable to connect to 'dbname='postgres' port=5432': could not connect to server: [NO SUCH FILE OR DIRECTORY]");

        TEST_RESULT_INT(result.primaryId, 8, "    check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "    check primary");
        TEST_RESULT_STR(strPtr(dbArchiveMode(result.primary)), "on", "    dbArchiveMode");
        TEST_RESULT_STR(strPtr(dbArchiveCommand(result.primary)), PROJECT_BIN, "    dbArchiveCommand");
        TEST_RESULT_STR(strPtr(dbWalSwitch(result.primary)), "000000010000000200000003", "    wal switch");
        TEST_RESULT_INT(result.standbyId, 1, "    check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "    check standby");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");
        TEST_RESULT_VOID(dbFree(result.standby), "free standby");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
