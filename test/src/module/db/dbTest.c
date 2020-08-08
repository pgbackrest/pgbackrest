/***********************************************************************************************************************************
Test Database
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessLog.h"
#include "common/harnessPq.h"

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/type/json.h"

/***********************************************************************************************************************************
Macro to check that replay is making progress -- this does not seem useful enough to be included in the pq harness header
***********************************************************************************************************************************/
#define HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS(                                                                                \
    sessionParam, walNameParam, lsnNameParam, targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam,              \
    replayProgressParam, sleepParam)                                                                                               \
    {.session = sessionParam,                                                                                                      \
        .function = HRNPQ_SENDQUERY,                                                                                               \
        .param =                                                                                                                   \
            "[\"select replayLsn::text,\\n"                                                                                        \
            "       (replayLsn > '" targetLsnParam "')::bool as targetReached,\\n"                                                 \
            "       (replayLsn > '" replayLastLsnParam "')::bool as replayProgress\\n"                                             \
            "  from pg_catalog.pg_last_" walNameParam "_replay_" lsnNameParam "() as replayLsn\"]",                                \
        .resultInt = 1, .sleep = sleepParam},                                                                                      \
    {.session = sessionParam, .function = HRNPQ_CONSUMEINPUT},                                                                     \
    {.session = sessionParam, .function = HRNPQ_ISBUSY},                                                                           \
    {.session = sessionParam, .function = HRNPQ_GETRESULT},                                                                        \
    {.session = sessionParam, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                       \
    {.session = sessionParam, .function = HRNPQ_NTUPLES, .resultInt = 1},                                                          \
    {.session = sessionParam, .function = HRNPQ_NFIELDS, .resultInt = 3},                                                          \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_FTYPE, .param = "[2]", .resultInt = HRNPQ_TYPE_BOOL},                              \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = replayLsnParam},                            \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = cvtBoolToConstZ(targetReachedParam)},       \
    {.session = sessionParam, .function = HRNPQ_GETVALUE, .param = "[0,2]", .resultZ = cvtBoolToConstZ(replayProgressParam)},      \
    {.session = sessionParam, .function = HRNPQ_CLEAR},                                                                            \
    {.session = sessionParam, .function = HRNPQ_GETRESULT, .resultNull = true}

#define HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(                                                                          \
    sessionParam, targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam, replayProgressParam, sleepParam)         \
    HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS(                                                                                    \
        sessionParam, "wal", "lsn", targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam, replayProgressParam,   \
        sleepParam)

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
                IoRead *read = ioFdReadNew(strNew("client read"), HARNESS_FORK_CHILD_READ(), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("client write"), HARNESS_FORK_CHILD_WRITE(), 2000);
                ioWriteOpen(write);

                // Set options
                StringList *argList = strLstNew();
                strLstAddZ(argList, "--stanza=test1");
                strLstAddZ(argList, "--pg1-path=/path/to/pg");
                strLstAddZ(argList, "--" CFGOPT_REMOTE_TYPE "=" PROTOCOL_REMOTE_TYPE_PG);
                strLstAddZ(argList, "--process=0");
                harnessCfgLoadRole(cfgCmdBackup, cfgCmdRoleRemote, argList);

                // Set script
                harnessPqScriptSet((HarnessPq [])
                {
                    HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
                    HRNPQ_MACRO_SET_SEARCH_PATH(1),
                    HRNPQ_MACRO_SET_CLIENT_ENCODING(1),
                    HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_84, "/pgdata", NULL, NULL),
                    HRNPQ_MACRO_CLOSE(1),

                    HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432"),
                    HRNPQ_MACRO_SET_SEARCH_PATH(1),
                    HRNPQ_MACRO_SET_CLIENT_ENCODING(1),
                    HRNPQ_MACRO_VALIDATE_QUERY(1, PG_VERSION_84, "/pgdata", NULL, NULL),
                    HRNPQ_MACRO_WAL_SWITCH(1, "xlog", "000000030000000200000003"),
                    HRNPQ_MACRO_CLOSE(1),

                    HRNPQ_MACRO_DONE()
                });

                // Create server
                ProtocolServer *server = NULL;

                TEST_ASSIGN(server, protocolServerNew(strNew("db test server"), strNew("test"), read, write), "create server");
                TEST_RESULT_VOID(protocolServerHandlerAdd(server, dbProtocol), "add handler");
                TEST_RESULT_VOID(protocolServerProcess(server, NULL), "run process loop");
                TEST_RESULT_VOID(protocolServerFree(server), "free server");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                IoRead *read = ioFdReadNew(strNew("server read"), HARNESS_FORK_PARENT_READ_PROCESS(0), 2000);
                ioReadOpen(read);
                IoWrite *write = ioFdWriteNew(strNew("server write"), HARNESS_FORK_PARENT_WRITE_PROCESS(0), 2000);
                ioWriteOpen(write);

                // Create client
                ProtocolClient *client = NULL;
                Db *db = NULL;

                TEST_ASSIGN(client, protocolClientNew(strNew("db test client"), strNew("test"), read, write), "create client");

                // Open and free database
                TEST_ASSIGN(db, dbNew(NULL, client, strNew("test")), "create db");
                TEST_RESULT_VOID(dbOpen(db), "open db");
                TEST_RESULT_VOID(dbFree(db), "free db");

                // Open the database, but don't free it so the server is forced to do it on shutdown
                TEST_ASSIGN(db, dbNew(NULL, client, strNew("test")), "create db");
                TEST_RESULT_VOID(dbOpen(db), "open db");
                TEST_RESULT_STR_Z(dbWalSwitch(db), "000000030000000200000003", "    wal switch");
                TEST_RESULT_VOID(memContextCallbackClear(db->memContext), "clear context so close is not called");

                TEST_RESULT_VOID(protocolClientFree(client), "free client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("dbBackupStart(), dbBackupStop(), dbTime(), dbList(), dbTablespaceList(), and dbReplayWait()"))
    {
        StringList *argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/pg1");
        harnessCfgLoad(cfgCmdBackup, argList);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 8.3 start backup with no start fast");

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_LE_91(1, "dbname='postgres' port=5432", PG_VERSION_83, "/pg1", NULL, NULL),

            // Get advisory lock
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),

            // Start backup with no start fast
            HRNPQ_MACRO_START_BACKUP_83(1, "1/1", "000000010000000100000001"),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        DbGetResult db = {.primaryId = 0};
        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, false, false).lsn, "1/1", "start backup");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start/stop backup");

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, "/pg1", false, NULL, NULL),

            // Get start time
            HRNPQ_MACRO_TIME_QUERY(1, 1000),

            // Start backup errors on advisory lock
            HRNPQ_MACRO_ADVISORY_LOCK(1, false),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_IS_IN_BACKUP(1, false),
            HRNPQ_MACRO_START_BACKUP_84_95(1, false, "2/3", "000000010000000200000003"),
            HRNPQ_MACRO_DATABASE_LIST_1(1, "test1"),
            HRNPQ_MACRO_TABLESPACE_LIST_0(1),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, "2/4", "000000010000000200000004"),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");

        TEST_RESULT_UINT(dbTimeMSec(db.primary), 1000, "check time");

        TEST_ERROR(
            dbBackupStart(db.primary, false, false), LockAcquireError,
            "unable to acquire pgBackRest advisory lock\n"
            "HINT: is another pgBackRest backup already running on this cluster?");

        DbBackupStartResult backupStartResult = {.lsn = NULL};
        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, true), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "2/3", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000010000000200000003", "check wal segment name");

        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(dbList(db.primary))), "[[16384,\"test1\",13777]]", "check db list");
        TEST_RESULT_STR_Z(jsonFromVar(varNewVarLst(dbTablespaceList(db.primary))), "[]", "check tablespace list");

        DbBackupStopResult backupStopResult = {.lsn = NULL};
        TEST_ASSIGN(backupStopResult, dbBackupStop(db.primary), "stop backup");
        TEST_RESULT_STR_Z(backupStopResult.lsn, "2/4", "check lsn");
        TEST_RESULT_STR_Z(backupStopResult.walSegmentName, "000000010000000200000004", "check wal segment name");
        TEST_RESULT_STR_Z(backupStopResult.backupLabel, NULL, "check backup label is not set");
        TEST_RESULT_STR_Z(backupStopResult.tablespaceMap, NULL, "check tablespace map is not set");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start/stop backup where backup is in progress");

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, "/pg1", false, NULL, NULL),

            // Start backup when backup is in progress
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_IS_IN_BACKUP(1, true),

            // Stop old backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, "1/1", "000000010000000100000001"),

            // Start backup
            HRNPQ_MACRO_START_BACKUP_84_95(1, true, "2/5", "000000010000000200000005"),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_LE_95(1, "2/6", "000000010000000200000006"),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, true, true).lsn, "2/5", "start backup");

        TEST_RESULT_LOG(
            "P00   WARN: the cluster is already in backup mode but no pgBackRest backup process is running."
                " pg_stop_backup() will be called so a new backup can be started.");

        TEST_RESULT_STR_Z(dbBackupStop(db.primary).lsn, "2/6", "stop backup");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.6 start/stop backup");

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, "/pg1", false, NULL, NULL),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_96(1, false, "3/3", "000000010000000300000003"),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_96(1, "3/4", "000000010000000300000004", false),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(true, true, false), "get primary");

        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, true), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "3/3", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000010000000300000003", "check wal segment name");

        TEST_ASSIGN(backupStopResult, dbBackupStop(db.primary), "stop backup");
        TEST_RESULT_STR_Z(backupStopResult.lsn, "3/4", "check lsn");
        TEST_RESULT_STR_Z(backupStopResult.walSegmentName, "000000010000000300000004", "check wal segment name");
        TEST_RESULT_STR_Z(backupStopResult.backupLabel, "BACKUP_LABEL_DATA", "check backup label");
        TEST_RESULT_STR_Z(backupStopResult.tablespaceMap, NULL, "check tablespace map is not set");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start backup from standby");

        argList = strLstNew();
        strLstAddZ(argList, "--stanza=test1");
        strLstAddZ(argList, "--repo1-retention-full=1");
        strLstAddZ(argList, "--pg1-path=/pg1");
        strLstAddZ(argList, "--pg2-path=/pg2");
        strLstAddZ(argList, "--pg2-port=5433");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_95, "/pg1", false, NULL, NULL),

            // Connect to standby
            HRNPQ_MACRO_OPEN_GE_92(2, "dbname='postgres' port=5433", PG_VERSION_95, "/pg2", true, NULL, NULL),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_84_95(1, false, "5/4", "000000050000000500000004"),

            // Wait for standby to sync
            HRNPQ_MACRO_REPLAY_WAIT_LE_95(2, "5/4"),

            // Close standby
            HRNPQ_MACRO_CLOSE(2),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(false, true, true), "get primary and standby");

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, false, false).lsn, "5/4", "start backup");
        TEST_RESULT_VOID(dbReplayWait(db.standby, STRDEF("5/4"), 1000), "sync standby");

        TEST_RESULT_VOID(dbFree(db.standby), "free standby");
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 10 start/stop backup from standby");

        harnessPqScriptSet((HarnessPq [])
        {
            // Connect to primary
            HRNPQ_MACRO_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_10, "/pg1", false, NULL, NULL),

            // Connect to standby
            HRNPQ_MACRO_OPEN_GE_96(2, "dbname='postgres' port=5433", PG_VERSION_10, "/pg2", true, NULL, NULL),

            // Start backup
            HRNPQ_MACRO_ADVISORY_LOCK(1, true),
            HRNPQ_MACRO_START_BACKUP_GE_10(1, false, "5/5", "000000050000000500000005"),

            // Standby returns NULL lsn
            {.session = 2,
                .function = HRNPQ_SENDQUERY,
                .param =
                    "[\"select replayLsn::text,\\n"
                    "       (replayLsn > '5/5')::bool as targetReached\\n"
                    "  from pg_catalog.pg_last_wal_replay_lsn() as replayLsn\"]",
                .resultInt = 1},
            {.session = 2, .function = HRNPQ_CONSUMEINPUT},
            {.session = 2, .function = HRNPQ_ISBUSY},
            {.session = 2, .function = HRNPQ_GETRESULT},
            {.session = 2, .function = HRNPQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 2, .function = HRNPQ_NTUPLES, .resultInt = 1},
            {.session = 2, .function = HRNPQ_NFIELDS, .resultInt = 2},
            {.session = 2, .function = HRNPQ_FTYPE, .param = "[0]", .resultInt = HRNPQ_TYPE_TEXT},
            {.session = 2, .function = HRNPQ_FTYPE, .param = "[1]", .resultInt = HRNPQ_TYPE_BOOL},
            {.session = 2, .function = HRNPQ_GETVALUE, .param = "[0,0]", .resultZ = ""},
            {.session = 2, .function = HRNPQ_GETISNULL, .param = "[0,0]", .resultInt = 1},
            {.session = 2, .function = HRNPQ_GETVALUE, .param = "[0,1]", .resultZ = "false"},
            {.session = 2, .function = HRNPQ_CLEAR},
            {.session = 2, .function = HRNPQ_GETRESULT, .resultNull = true},

            // Timeout waiting for sync
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_GE_10(2, "5/5", false, "5/3"),
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 250),
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 0),

            // Checkpoint target not reached
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_GE_10(2, "5/5", true, "5/5"),
            HRNPQ_MACRO_CHECKPOINT(2),
            HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", false, "5/4"),

            // Wait for standby to sync
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_GE_10(2, "5/5", false, "5/3"),
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 0),
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/4", "5/3", true, 0),
            HRNPQ_MACRO_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", true, "5/5", "5/4", true, 0),
            HRNPQ_MACRO_CHECKPOINT(2),
            HRNPQ_MACRO_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", true, "X/X"),

            // Close standby
            HRNPQ_MACRO_CLOSE(2),

            // Stop backup
            HRNPQ_MACRO_STOP_BACKUP_GE_10(1, "5/6", "000000050000000500000006", true),

            // Close primary
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(db, dbGet(false, true, true), "get primary and standby");

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, false, false).lsn, "5/5", "start backup");

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), 1000), ArchiveTimeoutError,
            "unable to query replay lsn on the standby using 'pg_catalog.pg_last_wal_replay_lsn()'\n"
            "HINT: Is this a standby?");

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), 200), ArchiveTimeoutError,
            "timeout before standby replayed to 5/5 - only reached 5/3");

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), 1000), ArchiveTimeoutError,
            "the checkpoint lsn 5/4 is less than the target lsn 5/5 even though the replay lsn is 5/5");

        TEST_RESULT_VOID(dbReplayWait(db.standby, STRDEF("5/5"), 1000), "sync standby");

        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        TEST_RESULT_STR_Z(dbBackupStop(db.primary).tablespaceMap, "TABLESPACE_MAP_DATA", "stop backup");

        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
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
        strLstAddZ(argList, "--pg1-user=bob");
        harnessCfgLoad(cfgCmdBackup, argList);

        harnessPqScriptSet((HarnessPq [])
        {
            {.function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432 user='bob'\"]"},
            {.function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRNPQ_FINISH},
            {.function = NULL}
        });

        TEST_ERROR(dbGet(true, true, false), DbConnectError, "unable to find primary cluster - cannot proceed");
        harnessLogResult(
            "P00   WARN: unable to check pg-1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432 user='bob'':"
                " error");

        // Only cluster is a standby
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432 user='bob'"),
            HRNPQ_MACRO_SET_SEARCH_PATH(1),
            HRNPQ_MACRO_SET_CLIENT_ENCODING(1),
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
            HRNPQ_MACRO_OPEN(1, "dbname='postgres' port=5432 user='bob'"),
            HRNPQ_MACRO_SET_SEARCH_PATH(1),
            HRNPQ_MACRO_SET_CLIENT_ENCODING(1),
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
            HRNPQ_MACRO_OPEN_LE_91(1, "dbname='postgres' port=5432 user='bob'", PG_VERSION_84, "/pgdata", NULL, NULL),
            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_DONE()
        });

        TEST_ASSIGN(result, dbGet(true, true, false), "get primary only");

        TEST_RESULT_INT(result.primaryId, 1, "    check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "    check primary");
        TEST_RESULT_INT(result.standbyId, 0, "    check standby id");
        TEST_RESULT_BOOL(result.standby == NULL, true, "    check standby");
        TEST_RESULT_INT(dbPgVersion(result.primary), PG_VERSION_84, "    version set");
        TEST_RESULT_STR_Z(dbPgDataPath(result.primary), "/pgdata", "    path set");

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
            HRNPQ_MACRO_OPEN_LE_91(1, "dbname='postgres' port=5432", PG_VERSION_84, "/pgdata", NULL, NULL),
            HRNPQ_MACRO_OPEN_LE_91(8, "dbname='postgres' port=5433", PG_VERSION_84, "/pgdata", NULL, NULL),

            HRNPQ_MACRO_CLOSE(1),
            HRNPQ_MACRO_CLOSE(8),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(true, true, false), DbConnectError, "more than one primary cluster found");

        // Two standbys found but no primary
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/pgdata", true, NULL, NULL),

            HRNPQ_MACRO_CLOSE(8),
            HRNPQ_MACRO_CLOSE(1),

            HRNPQ_MACRO_DONE()
        });

        TEST_ERROR(dbGet(false, true, false), DbConnectError, "unable to find primary cluster - cannot proceed");

        // Two standbys and primary not required
        // -------------------------------------------------------------------------------------------------------------------------
        harnessPqScriptSet((HarnessPq [])
        {
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),
            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5433", PG_VERSION_92, "/pgdata", true, NULL, NULL),

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
            HRNPQ_MACRO_OPEN_GE_92(1, "dbname='postgres' port=5432", PG_VERSION_92, "/pgdata", true, NULL, NULL),

            // pg-4 error
            {.session = 4, .function = HRNPQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 4, .function = HRNPQ_STATUS, .resultInt = CONNECTION_BAD},
            {.session = 4, .function = HRNPQ_ERRORMESSAGE, .resultZ = "error"},
            {.session = 4, .function = HRNPQ_FINISH},

            HRNPQ_MACRO_OPEN_GE_92(8, "dbname='postgres' port=5434", PG_VERSION_92, "/pgdata", false, NULL, NULL),

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
        TEST_RESULT_STR_Z(dbArchiveMode(result.primary), "on", "    dbArchiveMode");
        TEST_RESULT_STR_Z(dbArchiveCommand(result.primary), PROJECT_BIN, "    dbArchiveCommand");
        TEST_RESULT_STR_Z(dbWalSwitch(result.primary), "000000010000000200000003", "    wal switch");
        TEST_RESULT_INT(result.standbyId, 1, "    check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "    check standby");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");
        TEST_RESULT_VOID(dbFree(result.standby), "free standby");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
