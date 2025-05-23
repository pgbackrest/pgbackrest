/***********************************************************************************************************************************
Test Database
***********************************************************************************************************************************/
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/type/json.h"
#include "storage/remote/protocol.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessLog.h"
#include "common/harnessPack.h"
#include "common/harnessPostgres.h"
#include "common/harnessPq.h"

/***********************************************************************************************************************************
Macro to check that replay is making progress -- this does not seem useful enough to be included in the pq harness header
***********************************************************************************************************************************/
#define                                                                                                                            \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS(                                                                                  \
        sessionParam, walNameParam, lsnNameParam, targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam,          \
        replayProgressParam, sleepParam)                                                                                           \
    {.session = sessionParam,                                                                                                      \
        .function = HRN_PQ_SENDQUERY,                                                                                              \
        .param =                                                                                                                   \
            "[\"select replayLsn::text,\\n"                                                                                        \
            "       (replayLsn > '" targetLsnParam "')::bool as targetReached,\\n"                                                 \
            "       (replayLsn > '" replayLastLsnParam "')::bool as replayProgress\\n"                                             \
            "  from pg_catalog.pg_last_" walNameParam "_replay_" lsnNameParam "() as replayLsn\"]",                                \
        .resultInt = 1, .sleep = sleepParam},                                                                                      \
    {.session = sessionParam, .function = HRN_PQ_CONSUMEINPUT},                                                                    \
    {.session = sessionParam, .function = HRN_PQ_ISBUSY},                                                                          \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT},                                                                       \
    {.session = sessionParam, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},                                      \
    {.session = sessionParam, .function = HRN_PQ_NTUPLES, .resultInt = 1},                                                         \
    {.session = sessionParam, .function = HRN_PQ_NFIELDS, .resultInt = 3},                                                         \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_BOOL},                            \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = replayLsnParam},                           \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = cvtBoolToConstZ(targetReachedParam)},      \
    {.session = sessionParam, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = cvtBoolToConstZ(replayProgressParam)},     \
    {.session = sessionParam, .function = HRN_PQ_CLEAR},                                                                           \
    {.session = sessionParam, .function = HRN_PQ_GETRESULT, .resultNull = true}

#define                                                                                                                            \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(                                                                            \
        sessionParam, targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam, replayProgressParam, sleepParam)     \
    HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS(                                                                                  \
        sessionParam, "wal", "lsn", targetLsnParam, targetReachedParam, replayLsnParam, replayLastLsnParam, replayProgressParam,   \
        sleepParam)

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // PQfinish() is strictly checked
    hrnPqScriptStrictSet(true);

    // *****************************************************************************************************************************
    if (testBegin("Db and dbProtocol()"))
    {
        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                // Set options
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
                hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
                hrnCfgArgKeyRawZ(argList, cfgOptPgDatabase, 1, "testdb");
                hrnCfgArgRawStrId(argList, cfgOptRemoteType, protocolStorageTypePg);
                hrnCfgArgRawZ(argList, cfgOptProcess, "0");
                hrnCfgArgRawZ(argList, cfgOptDbTimeout, "777");
                HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleRemote);

                // Set script
                HRN_PQ_SCRIPT_SET(
                    HRN_PQ_SCRIPT_OPEN(1, "dbname='testdb' port=5432"),
                    HRN_PQ_SCRIPT_SET_SEARCH_PATH(1),
                    HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(1),
                    HRN_PQ_SCRIPT_VALIDATE_QUERY(1, PG_VERSION_95, TEST_PATH "/pg", NULL, NULL),
                    HRN_PQ_SCRIPT_SET_APPLICATION_NAME(1),
                    HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false),
                    HRN_PQ_SCRIPT_CLOSE(1),

                    HRN_PQ_SCRIPT_OPEN(1, "dbname='testdb' port=5432"),
                    HRN_PQ_SCRIPT_SET_SEARCH_PATH(1),
                    HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(1),
                    HRN_PQ_SCRIPT_VALIDATE_QUERY(1, PG_VERSION_95, TEST_PATH "/pg", NULL, NULL),
                    HRN_PQ_SCRIPT_SET_APPLICATION_NAME(1),
                    HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false),
                    HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(1, "2/3"),
                    HRN_PQ_SCRIPT_WAL_SWITCH(1, "xlog", "000000030000000200000003"),
                    HRN_PQ_SCRIPT_CLOSE(1));

                // Create server
                ProtocolServer *server = NULL;

                TEST_ASSIGN(
                    server,
                    protocolServerNew(STRDEF("db test server"), STRDEF("test"), HRN_FORK_CHILD_READ(), HRN_FORK_CHILD_WRITE()),
                    "create server");

                static const ProtocolServerHandler commandHandler[] =
                {
                    PROTOCOL_SERVER_HANDLER_DB_LIST
                    PROTOCOL_SERVER_HANDLER_OPTION_LIST
                    PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST
                };

                TEST_RESULT_VOID(protocolServerProcess(server, NULL, LSTDEF(commandHandler)), "run process loop");
                TEST_RESULT_VOID(protocolServerFree(server), "free server");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN()
            {
                // Set options
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
                hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg");
                hrnCfgArgKeyRawZ(argList, cfgOptPgDatabase, 1, "testdb");
                hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
                HRN_CFG_LOAD(cfgCmdBackup, argList);

                // Create control file
                HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95);

                // Create client
                ProtocolClient *client = NULL;
                Db *db = NULL;

                TEST_ASSIGN(
                    client,
                    protocolClientNew(STRDEF("db test client"), STRDEF("test"), HRN_FORK_PARENT_READ(0), HRN_FORK_PARENT_WRITE(0)),
                    "create client");

                TRY_BEGIN()
                {
                    // -------------------------------------------------------------------------------------------------------------
                    TEST_TITLE("open and free database");

                    TEST_ASSIGN(db, dbNew(NULL, client, storagePgIdx(0), STRDEF(PROJECT_NAME " [" CFGCMD_BACKUP "]")), "create db");

                    TRY_BEGIN()
                    {
                        TEST_RESULT_VOID(dbOpen(db), "open db");
                        TEST_RESULT_VOID(dbFree(db), "free db");
                        db = NULL;
                    }
                    CATCH_ANY()
                    {
                        // Free on error
                        dbFree(db);
                    }
                    TRY_END();

                    // -------------------------------------------------------------------------------------------------------------
                    TEST_TITLE("remote commands");

                    TEST_ASSIGN(db, dbNew(NULL, client, storagePgIdx(0), STRDEF(PROJECT_NAME " [" CFGCMD_BACKUP "]")), "create db");

                    TRY_BEGIN()
                    {
                        TEST_RESULT_VOID(dbOpen(db), "open db");
                        TEST_RESULT_STR_Z(dbWalSwitch(db), "000000030000000200000003", "wal switch");
                        TEST_RESULT_UINT(dbDbTimeout(db), 777000, "check timeout");
                        TEST_RESULT_VOID(memContextCallbackClear(db->pub.memContext), "clear context so close is not called");
                    }
                    FINALLY()
                    {
                        // Clear the context callback so the server frees the db on exit
                        memContextCallbackClear(db->pub.memContext);
                    }
                    TRY_END();
                }
                FINALLY()
                {
                    // Free on error
                    protocolClientFree(client);
                }
                TRY_END();
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("dbBackupStart(), dbBackupStop(), dbTime(), dbList(), dbTablespaceList(), and dbReplayWait()"))
    {
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgDatabase, 1, "backupdb");
        hrnCfgArgRawZ(argList, cfgOptDbTimeout, "888");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create control file
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95, .checkpoint = pgLsnFromStr(STRDEF("1/1")));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error when unable to select any pg_settings");

        HRN_PQ_SCRIPT_SET(
            // Connect to primary
            HRN_PQ_SCRIPT_OPEN(1, "dbname='backupdb' port=5432"),
            HRN_PQ_SCRIPT_SET_SEARCH_PATH(1),
            HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(1),

            // Return NULL for a row in pg_settings
            {.session = 1, .function = HRN_PQ_SENDQUERY,
             .param =
                 "[\"select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"
                 " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"
                 " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"
                 " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text,"
                 " (select setting from pg_catalog.pg_settings where name = 'checkpoint_timeout')::int4\"]",
             .resultInt = 1},
            {.session = 1, .function = HRN_PQ_CONSUMEINPUT},
            {.session = 1, .function = HRN_PQ_ISBUSY},
            {.session = 1, .function = HRN_PQ_GETRESULT},
            {.session = 1, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 1, .function = HRN_PQ_NTUPLES, .resultInt = 1},
            {.session = 1, .function = HRN_PQ_NFIELDS, .resultInt = 5},
            {.session = 1, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_INT4},
            {.session = 1, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_TEXT},
            {.session = 1, .function = HRN_PQ_FTYPE, .param = "[2]", .resultInt = HRN_PQ_TYPE_TEXT},
            {.session = 1, .function = HRN_PQ_FTYPE, .param = "[3]", .resultInt = HRN_PQ_TYPE_TEXT},
            {.session = 1, .function = HRN_PQ_FTYPE, .param = "[4]", .resultInt = HRN_PQ_TYPE_INT4},
            {.session = 1, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = "0"},
            {.session = 1, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = "value"},
            {.session = 1, .function = HRN_PQ_GETVALUE, .param = "[0,2]", .resultZ = "value"},
            {.session = 1, .function = HRN_PQ_GETVALUE, .param = "[0,3]", .resultZ = ""},
            {.session = 1, .function = HRN_PQ_GETISNULL, .param = "[0,3]", .resultInt = 1},
            {.session = 1, .function = HRN_PQ_GETVALUE, .param = "[0,4]", .resultZ = "300"},
            {.session = 1, .function = HRN_PQ_CLEAR},
            {.session = 1, .function = HRN_PQ_GETRESULT, .resultNull = true},

            // Close primary
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), DbConnectError,
            "unable to find primary cluster - cannot proceed\n"
            "HINT: are all available clusters in recovery?");

        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg1: [DbQueryError] unable to select some rows from pg_settings\n"
            "            HINT: is the backup running as the postgres user?\n"
            "            HINT: is the pg_read_all_settings role assigned for PostgreSQL >= 10?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start/stop backup");

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95, .checkpoint = pgLsnFromStr(STRDEF("2/3")));

        // Connect to primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='backupdb' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL));

        DbGetResult db = {0};
        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");

        // Get start time
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_TIME_QUERY(1, 1000));
        TEST_RESULT_UINT(dbTimeMSec(db.primary), 1000, "check time");

        // Start backup errors on advisory lock
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_ADVISORY_LOCK(1, false));

        TEST_ERROR(
            dbBackupStart(db.primary, false, false, false), LockAcquireError,
            "unable to acquire pgBackRest advisory lock\n"
            "HINT: is another pgBackRest backup already running on this cluster?");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_ADVISORY_LOCK(1, true),
            HRN_PQ_SCRIPT_IS_IN_BACKUP(1, false),
            HRN_PQ_SCRIPT_START_BACKUP_LE_95(1, false, "2/3", "000000010000000200000003"));

        DbBackupStartResult backupStartResult = {0};
        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, true, false), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "2/3", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000010000000200000003", "check wal segment name");

        // Get database list
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_DATABASE_LIST_1(1, "test1"));
        TEST_RESULT_STR_Z(hrnPackToStr(dbList(db.primary)), "1:array:[1:u32:16384, 2:str:test1, 3:u32:13777]", "check db list");

        // Get tablespace list
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_TABLESPACE_LIST_0(1));
        TEST_RESULT_STR_Z(hrnPackToStr(dbTablespaceList(db.primary)), "", "check tablespace list");

        // Stop backup
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(1, "2/4", "000000010000000200000004"));

        DbBackupStopResult backupStopResult = {.lsn = NULL};
        TEST_ASSIGN(backupStopResult, dbBackupStop(db.primary), "stop backup");
        TEST_RESULT_STR_Z(backupStopResult.lsn, "2/4", "check lsn");
        TEST_RESULT_STR_Z(backupStopResult.walSegmentName, "000000010000000200000004", "check wal segment name");
        TEST_RESULT_STR_Z(backupStopResult.backupLabel, NULL, "check backup label is not set");
        TEST_RESULT_STR_Z(backupStopResult.tablespaceMap, NULL, "check tablespace map is not set");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start/stop backup where backup is in progress");

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95, .checkpoint = pgLsnFromStr(STRDEF("2/5")));

        // Connect to primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='backupdb' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");

        // Start backup when backup is in progress
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_ADVISORY_LOCK(1, true),
            HRN_PQ_SCRIPT_IS_IN_BACKUP(1, true),

            // Stop old backup
            HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(1, "1/1", "000000010000000100000001"),

            // Start backup
            HRN_PQ_SCRIPT_START_BACKUP_LE_95(1, true, "2/5", "000000010000000200000005"));

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, true, true, false).lsn, "2/5", "start backup");

        TEST_RESULT_LOG(
            "P00   WARN: the cluster is already in backup mode but no pgBackRest backup process is running."
            " pg_stop_backup() will be called so a new backup can be started.");

        // Stop backup
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_STOP_BACKUP_LE_95(1, "2/6", "000000010000000200000006"));
        TEST_RESULT_STR_Z(dbBackupStop(db.primary).lsn, "2/6", "stop backup");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.6 start/stop backup");

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_96, .checkpoint = pgLsnFromStr(STRDEF("3/3")));

        // Connect to primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='backupdb' port=5432", PG_VERSION_96, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");

        // Start backup with timeline error
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_LE_96(1, "000000020000000300000002"),
            HRN_PQ_SCRIPT_START_BACKUP_96(1, false, "3/3", "000000020000000300000003"));

        TEST_ERROR(
            dbBackupStart(db.primary, false, true, true), DbMismatchError, "WAL timeline 2 does not match pg_control timeline 1");

        // Start backup with checkpoint error
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_LE_96(1, "000000010000000400000003"),
            HRN_PQ_SCRIPT_START_BACKUP_96(1, false, "4/4", "000000010000000400000004"));

        TEST_ERROR(
            dbBackupStart(db.primary, false, true, true), DbMismatchError,
            "current checkpoint '3/3' is less than backup start '4/4'");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_LE_96(1, "000000010000000300000002"),
            HRN_PQ_SCRIPT_START_BACKUP_96(1, false, "3/3", "000000010000000300000003"));

        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, true, true), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "3/3", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000010000000300000003", "check wal segment name");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentCheck, "000000010000000300000002", "check wal segment check");

        // Stop backup
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_STOP_BACKUP_96(1, "3/4", "000000010000000300000004", false));

        TEST_ASSIGN(backupStopResult, dbBackupStop(db.primary), "stop backup");
        TEST_RESULT_STR_Z(backupStopResult.lsn, "3/4", "check lsn");
        TEST_RESULT_STR_Z(backupStopResult.walSegmentName, "000000010000000300000004", "check wal segment name");
        TEST_RESULT_STR_Z(backupStopResult.backupLabel, "BACKUP_LABEL_DATA", "check backup label");
        TEST_RESULT_STR_Z(backupStopResult.tablespaceMap, NULL, "check tablespace map is not set");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 9.5 start backup from standby");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 2, TEST_PATH "/pg2");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 2, "5433");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create control file
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95, .timeline = 5, .checkpoint = pgLsnFromStr(STRDEF("5/4")));
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_95, .timeline = 5, .checkpoint = pgLsnFromStr(STRDEF("5/4")));

        // Connect to primary and standby
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_93(2, "dbname='postgres' port=5433", PG_VERSION_95, TEST_PATH "/pg2", true, NULL, NULL));

        TEST_ASSIGN(db, dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_Y), "get primary and standby");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_ADVISORY_LOCK(1, true),
            HRN_PQ_SCRIPT_START_BACKUP_LE_95(1, false, "5/4", "000000050000000500000004"));

        TEST_RESULT_STR_Z(dbBackupStart(db.primary, false, false, false).lsn, "5/4", "start backup");

        // Wait for standby to sync
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_REPLAY_WAIT_LE_95(2, "5/4"));

        TEST_RESULT_VOID(dbReplayWait(db.standby, STRDEF("5/4"), dbPgControl(db.primary).timeline, 1000), "sync standby");

        // Ping
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, true));
        TEST_ERROR(dbPing(db.primary, false), AssertError, "primary has switched to recovery");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));
        TEST_RESULT_VOID(dbPing(db.primary, false), "ping primary cluster");
        TEST_RESULT_VOID(dbPing(db.primary, false), "ping primary cluster (noop)");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false));
        TEST_RESULT_VOID(dbPing(db.primary, true), "ping primary cluster (force)");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, false));
        TEST_ERROR(
            dbPing(db.standby, false), DbMismatchError,
            "standby is no longer in recovery\n"
            "HINT: was the standby promoted during the backup?");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));
        TEST_RESULT_VOID(dbPing(db.standby, false), "ping standby cluster");
        TEST_RESULT_VOID(dbPing(db.standby, false), "ping standby cluster (noop)");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));
        TEST_RESULT_VOID(dbPing(db.standby, true), "ping standby cluster (force)");

        db.standby->pingTimeLast -= DB_PING_SEC * 2;
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_IS_STANDBY_QUERY(2, true));
        TEST_RESULT_VOID(dbPing(db.standby, false), "ping standby cluster");

        // Close primary and standby
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(2));
        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 10 start/stop backup from standby");

        // Update control file
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_10, .timeline = 5, .checkpoint = pgLsnFromStr(STRDEF("5/5")));
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_10, .timeline = 5, .checkpoint = pgLsnFromStr(STRDEF("5/5")));

        // Connect to primary and standby
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_10, TEST_PATH "/pg1", false, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(2, "dbname='postgres' port=5433", PG_VERSION_10, TEST_PATH "/pg2", true, NULL, NULL));

        TEST_ASSIGN(db, dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_Y), "get primary and standby");

        TEST_RESULT_UINT(dbPgControl(db.primary).timeline, 5, "check primary timeline");
        TEST_RESULT_UINT(dbPgControl(db.standby).timeline, 5, "check standby timeline");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_GE_10(1, "000000050000000500000005"),
            HRN_PQ_SCRIPT_START_BACKUP_GE_10(1, false, "5/5", "000000050000000500000005"),

            // Switch WAL segment so it can be checked
            HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(1, "5/5"),
            HRN_PQ_SCRIPT_WAL_SWITCH(1, "wal", "000000050000000500000005"));

        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, false, true), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "5/5", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000050000000500000005", "check wal segment name");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentCheck, "000000050000000500000005", "check wal segment check");

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("4/4"), 5, 1000), DbMismatchError, "standby checkpoint '5/5' is ahead of target '4/4'");

        // Standby returns NULL lsn
        HRN_PQ_SCRIPT_SET(
            {.session = 2, .function = HRN_PQ_SENDQUERY,
             .param =
                 "[\"select replayLsn::text,\\n"
                 "       (replayLsn > '5/5')::bool as targetReached\\n"
                 "  from pg_catalog.pg_last_wal_replay_lsn() as replayLsn\"]",
             .resultInt = 1},
            {.session = 2, .function = HRN_PQ_CONSUMEINPUT},
            {.session = 2, .function = HRN_PQ_ISBUSY},
            {.session = 2, .function = HRN_PQ_GETRESULT},
            {.session = 2, .function = HRN_PQ_RESULTSTATUS, .resultInt = PGRES_TUPLES_OK},
            {.session = 2, .function = HRN_PQ_NTUPLES, .resultInt = 1},
            {.session = 2, .function = HRN_PQ_NFIELDS, .resultInt = 2},
            {.session = 2, .function = HRN_PQ_FTYPE, .param = "[0]", .resultInt = HRN_PQ_TYPE_TEXT},
            {.session = 2, .function = HRN_PQ_FTYPE, .param = "[1]", .resultInt = HRN_PQ_TYPE_BOOL},
            {.session = 2, .function = HRN_PQ_GETVALUE, .param = "[0,0]", .resultZ = ""},
            {.session = 2, .function = HRN_PQ_GETISNULL, .param = "[0,0]", .resultInt = 1},
            {.session = 2, .function = HRN_PQ_GETVALUE, .param = "[0,1]", .resultZ = "false"},
            {.session = 2, .function = HRN_PQ_CLEAR},
            {.session = 2, .function = HRN_PQ_GETRESULT, .resultNull = true});

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), dbPgControl(db.primary).timeline, 1000), ArchiveTimeoutError,
            "unable to query replay lsn on the standby using 'pg_catalog.pg_last_wal_replay_lsn()'\n"
            "HINT: Is this a standby?");

        // Timeout waiting for sync
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(2, "5/5", false, "5/3"),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 100),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 100));

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), dbPgControl(db.primary).timeline, 200), ArchiveTimeoutError,
            "timeout before standby replayed to 5/5 - only reached 5/3\n"
            "HINT: is replication running and current on the standby?\n"
            "HINT: disable the 'backup-standby' option to backup directly from the primary.");

        // Checkpoint target timeout waiting for sync
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(2, "5/5", true, "5/5"),
            HRN_PQ_SCRIPT_CHECKPOINT(2),
            HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", false, "5/4", 100),
            HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", false, "5/4", 100),
            HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", false, "5/4", 100));

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), dbPgControl(db.primary).timeline, 200), ArchiveTimeoutError,
            "timeout before standby checkpoint lsn reached 5/5 - only reached 5/4");

        // Wait for standby to sync
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(2, "5/5", false, "5/3"),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 0),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/4", "5/3", true, 0),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", true, "5/5", "5/4", true, 0),
            HRN_PQ_SCRIPT_CHECKPOINT(2),
            HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", true, "X/X", 0));

        TEST_RESULT_VOID(dbReplayWait(db.standby, STRDEF("5/5"), dbPgControl(db.primary).timeline, 1000), "sync standby");

        // Update timeline to demonstrate that it is reloaded in dbReplayWait()
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_10, .timeline = 6, .checkpoint = pgLsnFromStr(STRDEF("5/5")));

        // Fail on timeline mismatch
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_GE_10(2, "5/5", false, "5/3"),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/3", "5/3", false, 0),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", false, "5/4", "5/3", true, 0),
            HRN_PQ_SCRIPT_REPLAY_TARGET_REACHED_PROGRESS_GE_10(2, "5/5", true, "5/5", "5/4", true, 0),
            HRN_PQ_SCRIPT_CHECKPOINT(2),
            HRN_PQ_SCRIPT_CHECKPOINT_TARGET_REACHED_GE_10(2, "5/5", true, "X/X", 0));

        TEST_ERROR(
            dbReplayWait(db.standby, STRDEF("5/5"), 77, 1000), DbMismatchError, "standby is on timeline 6 but expected 77");

        // Close standby
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(2));
        TEST_RESULT_VOID(dbFree(db.standby), "free standby");

        // Stop backup
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_STOP_BACKUP_GE_10(1, "5/6", "000000050000000500000006", true));
        TEST_RESULT_STR_Z(dbBackupStop(db.primary).tablespaceMap, "TABLESPACE_MAP_DATA", "stop backup");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 14 - checkpoint timeout warning");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        // With start-fast being disabled, set db-timeout smaller than checkpoint_timeout to raise a warning
        hrnCfgArgRawZ(argList, cfgOptDbTimeout, "299");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Connect to primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_14, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_GE_10(1, "000000050000000500000004"),
            HRN_PQ_SCRIPT_START_BACKUP_GE_10(1, false, "5/5", "000000050000000500000005"));

        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, false, true), "start backup");

        TEST_RESULT_LOG(
            "P00   WARN: start-fast is disabled and db-timeout (299s) is smaller than the PostgreSQL checkpoint_timeout (300s) -"
            " timeout may occur before the backup starts");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("PostgreSQL 15 - non-exclusive flag dropped");

        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_15, .timeline = 6, .checkpoint = pgLsnFromStr(STRDEF("6/6")));

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Connect to primary
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_15, TEST_PATH "/pg1", false, NULL, NULL));

        TEST_ASSIGN(db, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary");

        // Start backup
        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_CURRENT_WAL_GE_10(1, "000000060000000600000005"),
            HRN_PQ_SCRIPT_START_BACKUP_GE_15(1, false, "6/6", "000000060000000600000006"));

        TEST_ASSIGN(backupStartResult, dbBackupStart(db.primary, false, false, true), "start backup");
        TEST_RESULT_STR_Z(backupStartResult.lsn, "6/6", "check lsn");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentName, "000000060000000600000006", "check wal segment name");
        TEST_RESULT_STR_Z(backupStartResult.walSegmentCheck, "000000060000000600000005", "check wal segment check");

        // Stop backup
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_STOP_BACKUP_GE_15(1, "6/7", "000000060000000600000006", false));

        backupStopResult = (DbBackupStopResult){.lsn = NULL};
        TEST_ASSIGN(backupStopResult, dbBackupStop(db.primary), "stop backup");
        TEST_RESULT_STR_Z(backupStopResult.lsn, "6/7", "check lsn");
        TEST_RESULT_STR_Z(backupStopResult.walSegmentName, "000000060000000600000006", "check wal segment name");
        TEST_RESULT_STR_Z(backupStopResult.backupLabel, "BACKUP_LABEL_DATA", "check backup label");
        TEST_RESULT_STR_Z(backupStopResult.tablespaceMap, NULL, "check tablespace map is not set");

        // Close primary
        HRN_PQ_SCRIPT_SET(HRN_PQ_SCRIPT_CLOSE(1));
        TEST_RESULT_VOID(dbFree(db.primary), "free primary");
    }

    // *****************************************************************************************************************************
    if (testBegin("dbGet()"))
    {
        DbGetResult result = {0};

        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgUser, 1, "bob");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create control file
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(0), PG_VERSION_95);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error connecting to primary");

        HRN_PQ_SCRIPT_SET(
            {.function = HRN_PQ_CONNECTDB, .param = "[\"dbname='postgres' port=5432 user='bob'\"]"},
            {.function = HRN_PQ_STATUS, .resultInt = CONNECTION_BAD},
            {.function = HRN_PQ_ERRORMESSAGE, .resultZ = "error"},
            {.function = HRN_PQ_FINISH});

        TEST_ERROR(
            dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), DbConnectError,
            "unable to find primary cluster - cannot proceed\n"
            "HINT: are all available clusters in recovery?");
        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg1: [DbConnectError] unable to connect to 'dbname='postgres' port=5432 user='bob'':"
            " error");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("only available cluster is a standby");

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN(1, "dbname='postgres' port=5432 user='bob'"),
            HRN_PQ_SCRIPT_SET_SEARCH_PATH(1),
            HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(1),
            HRN_PQ_SCRIPT_VALIDATE_QUERY(1, PG_VERSION_95, TEST_PATH "/pg", NULL, NULL),
            HRN_PQ_SCRIPT_SET_APPLICATION_NAME(1),
            HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, true),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), DbConnectError,
            "unable to find primary cluster - cannot proceed\n"
            "HINT: are all available clusters in recovery?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("standby cluster required but not found");

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN(1, "dbname='postgres' port=5432 user='bob'"),
            HRN_PQ_SCRIPT_SET_SEARCH_PATH(1),
            HRN_PQ_SCRIPT_SET_CLIENT_ENCODING(1),
            HRN_PQ_SCRIPT_VALIDATE_QUERY(1, PG_VERSION_95, TEST_PATH "/pg", NULL, NULL),
            HRN_PQ_SCRIPT_SET_APPLICATION_NAME(1),
            HRN_PQ_SCRIPT_IS_STANDBY_QUERY(1, false),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            dbGet(false, false, CFGOPTVAL_BACKUP_STANDBY_Y), DbConnectError, "unable to find standby cluster - cannot proceed");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary cluster found");

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(
                1, "dbname='postgres' port=5432 user='bob'", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ASSIGN(result, dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary only");

        TEST_RESULT_INT(result.primaryIdx, 0, "check primary id");
        TEST_RESULT_BOOL(result.primary != NULL, true, "check primary");
        TEST_RESULT_INT(result.standbyIdx, 0, "check standby id");
        TEST_RESULT_BOOL(result.standby == NULL, true, "check standby");
        TEST_RESULT_INT(dbPgVersion(result.primary), PG_VERSION_95, "version set");
        TEST_RESULT_STR_Z(dbPgDataPath(result.primary), TEST_PATH "/pg1", "path set");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("more than one primary found");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5433");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        // Create control file
        HRN_PG_CONTROL_PUT(storagePgIdxWrite(1), PG_VERSION_95);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_93(1, "dbname='postgres' port=5432", PG_VERSION_95, TEST_PATH "/pg1", false, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_93(8, "dbname='postgres' port=5433", PG_VERSION_95, TEST_PATH "/pg8", false, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(1),
            HRN_PQ_SCRIPT_CLOSE(8));

        TEST_ERROR(dbGet(true, true, CFGOPTVAL_BACKUP_STANDBY_N), DbConnectError, "more than one primary cluster found");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two standbys found but no primary");

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_96, TEST_PATH "/pg1", true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_96, TEST_PATH "/pg8", true, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ERROR(
            dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_N), DbConnectError,
            "unable to find primary cluster - cannot proceed\n"
            "HINT: are all available clusters in recovery?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("two standbys and primary not required");

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_10, TEST_PATH "/pg1", true, NULL, NULL),
            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5433", PG_VERSION_10, TEST_PATH "/pg8", true, NULL, NULL),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ASSIGN(result, dbGet(false, false, CFGOPTVAL_BACKUP_STANDBY_N), "get standbys");

        TEST_RESULT_INT(result.primaryIdx, 0, "check primary id");
        TEST_RESULT_BOOL(result.primary == NULL, true, "check primary");
        TEST_RESULT_INT(result.standbyIdx, 0, "check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "check standby");

        TEST_RESULT_VOID(dbFree(result.standby), "free standby");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary and standby found");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 4, TEST_PATH "/pg4");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 4, "5433");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHost, 5, "localhost");
        hrnCfgArgKeyRawZ(argList, cfgOptPgHostUser, 5, TEST_USER);
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 5, TEST_PATH "/pg5");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 8, TEST_PATH "/pg8");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 8, "5434");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_12, TEST_PATH "/pg1", true, NULL, NULL),

            // pg4 error
            {.session = 4, .function = HRN_PQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 4, .function = HRN_PQ_STATUS, .resultInt = CONNECTION_BAD},
            {.session = 4, .function = HRN_PQ_ERRORMESSAGE, .resultZ = "error"},
            {.session = 4, .function = HRN_PQ_FINISH},

            HRN_PQ_SCRIPT_OPEN_GE_96(8, "dbname='postgres' port=5434", PG_VERSION_12, TEST_PATH "/pg8", false, NULL, NULL),

            HRN_PQ_SCRIPT_CREATE_RESTORE_POINT(8, "2/3"),
            HRN_PQ_SCRIPT_WAL_SWITCH(8, "wal", "000000010000000200000003"),

            HRN_PQ_SCRIPT_CLOSE(8),
            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ASSIGN(result, dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_N), "get primary and standby");

        hrnLogReplaceAdd("(could not connect to server|connection to server on socket).*$", NULL, "PG ERROR", false);
        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg4: [DbConnectError] unable to connect to 'dbname='postgres' port=5433': error\n"
            "P00   WARN: unable to check pg5: [DbConnectError] raised from remote-0 ssh protocol on 'localhost':"
            " unable to connect to 'dbname='postgres' port=5432': [PG ERROR]");

        TEST_RESULT_INT(result.primaryIdx, 3, "check primary idx");
        TEST_RESULT_BOOL(result.primary != NULL, true, "check primary");
        TEST_RESULT_STR_Z(dbArchiveMode(result.primary), "on", "dbArchiveMode");
        TEST_RESULT_STR_Z(dbArchiveCommand(result.primary), PROJECT_BIN, "dbArchiveCommand");
        TEST_RESULT_STR_Z(dbWalSwitch(result.primary), "000000010000000200000003", "wal switch");
        TEST_RESULT_INT(result.standbyIdx, 0, "check standby id");
        TEST_RESULT_BOOL(result.standby != NULL, true, "check standby");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");
        TEST_RESULT_VOID(dbFree(result.standby), "free standby");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("primary found and standby skipped");

        argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test1");
        hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, TEST_PATH "/pg1");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 4, TEST_PATH "/pg4");
        hrnCfgArgKeyRawZ(argList, cfgOptPgPort, 4, "5433");
        HRN_CFG_LOAD(cfgCmdBackup, argList);

        HRN_PQ_SCRIPT_SET(
            HRN_PQ_SCRIPT_OPEN_GE_96(1, "dbname='postgres' port=5432", PG_VERSION_12, TEST_PATH "/pg1", false, NULL, NULL),

            // pg4 error
            {.session = 4, .function = HRN_PQ_CONNECTDB, .param = "[\"dbname='postgres' port=5433\"]"},
            {.session = 4, .function = HRN_PQ_STATUS, .resultInt = CONNECTION_BAD},
            {.session = 4, .function = HRN_PQ_ERRORMESSAGE, .resultZ = "error"},
            {.session = 4, .function = HRN_PQ_FINISH},

            HRN_PQ_SCRIPT_CLOSE(1));

        TEST_ASSIGN(result, dbGet(false, true, CFGOPTVAL_BACKUP_STANDBY_PREFER), "get primary and try standby");

        TEST_RESULT_LOG(
            "P00   WARN: unable to check pg4: [DbConnectError] unable to connect to 'dbname='postgres' port=5433': error");

        TEST_RESULT_INT(result.primaryIdx, 0, "check primary idx");
        TEST_RESULT_BOOL(result.primary != NULL, true, "check primary");
        TEST_RESULT_BOOL(result.standby == NULL, true, "check standby was not found");

        TEST_RESULT_VOID(dbFree(result.primary), "free primary");
        TEST_RESULT_VOID(dbFree(result.standby), "free standby");
    }

    protocolFree();

    FUNCTION_HARNESS_RETURN_VOID();
}
