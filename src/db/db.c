/***********************************************************************************************************************************
Database Client
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/wait.h"
#include "config/config.h"
#include "config/protocol.h"
#include "db/db.h"
#include "db/protocol.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PG_BACKUP_ADVISORY_LOCK                                     "12340078987004321"
#define DB_PING_SEC                                                 30

// Maximum length of application_name as reported by PostgreSQL, i.e. NAMEDATALEN - 1
#define DB_APPLICATION_NAME_MAX                                     63

// Application name the WAL receiver falls back to when cluster_name is empty
STRING_STATIC(DB_WAL_RECEIVER_NAME_STR,                             "walreceiver");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Db
{
    DbPub pub;                                                      // Publicly accessible variables
    PgClient *client;                                               // Local PostgreSQL client
    ProtocolClient *remoteClient;                                   // Protocol client for remote db queries
    ProtocolClientSession *session;                                 // Protocol session for requests
    const Storage *storage;                                         // PostgreSQL storage
    const String *applicationName;                                  // Used to identify this connection in PostgreSQL
    time_t pingTimeLast;                                            // Last time cluster was pinged
};

/***********************************************************************************************************************************
Close protocol connection. No need to close a locally created PgClient since it has its own destructor.
***********************************************************************************************************************************/
static void
dbFreeResource(THIS_VOID)
{
    THIS(Db);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    protocolClientSessionFree(this->session);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN Db *
dbNew(PgClient *const client, ProtocolClient *const remoteClient, const Storage *const storage, const String *const applicationName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, client);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, remoteClient);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, applicationName);
    FUNCTION_LOG_END();

    ASSERT((client != NULL && remoteClient == NULL) || (client == NULL && remoteClient != NULL));
    ASSERT(storage != NULL);
    ASSERT(applicationName != NULL);

    OBJ_NEW_BEGIN(Db, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (Db)
        {
            .pub =
            {
                .memContext = memContextCurrent(),
            },
            .remoteClient = remoteClient,
            .session = remoteClient != NULL ? protocolClientSessionNewP(remoteClient, PROTOCOL_COMMAND_DB) : NULL,
            .storage = storage,
            .applicationName = strDup(applicationName),
        };

        this->client = pgClientMove(client, this->pub.memContext);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(DB, this);
}

/***********************************************************************************************************************************
Execute a query
***********************************************************************************************************************************/
static Pack *
dbQuery(Db *const this, const PgClientQueryResult resultType, const String *const query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING_ID, resultType);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(resultType != 0);
    ASSERT(query != NULL);

    Pack *result;

    // Query remotely
    if (this->remoteClient != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackWrite *const param = protocolPackNew();

            pckWriteStrIdP(param, resultType);
            pckWriteStrP(param, query);

            PackRead *const read = protocolClientSessionRequestP(this->session, .param = param);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = pckReadPackP(read);
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();
    }
    // Else locally
    else
        result = pgClientQuery(this->client, query, resultType);

    FUNCTION_LOG_RETURN(PACK, result);
}

/***********************************************************************************************************************************
Execute a command that expects no output
***********************************************************************************************************************************/
static void
dbExec(Db *const this, const String *const command)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    CHECK(AssertError, dbQuery(this, pgClientQueryResultNone, command) == NULL, "exec returned data");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Execute a query that returns a single row and column
***********************************************************************************************************************************/
static PackRead *
dbQueryColumn(Db *const this, const String *const query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    Pack *const pack = dbQuery(this, pgClientQueryResultColumn, query);
    PackRead *const result = pckReadNew(pack);
    pckMove(pack, objMemContext(result));

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/***********************************************************************************************************************************
Execute a query that returns a single row
***********************************************************************************************************************************/
static PackRead *
dbQueryRow(Db *const this, const String *const query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    Pack *const pack = dbQuery(this, pgClientQueryResultRow, query);
    PackRead *const result = pckReadNew(pack);
    pckMove(pack, objMemContext(result));

    FUNCTION_LOG_RETURN(PACK_READ, result);
}

/***********************************************************************************************************************************
Is the cluster in recovery?
***********************************************************************************************************************************/
static bool
dbIsInRecovery(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = pckReadBoolP(dbQueryColumn(this, STRDEF("select pg_catalog.pg_is_in_recovery()")));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbOpen(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Open the connection
        if (this->remoteClient != NULL)
        {
            protocolClientSessionOpenP(this->session);

            // Set a callback to notify the remote when a connection is closed
            memContextCallbackSet(this->pub.memContext, dbFreeResource, this);

            // Get db-timeout from the remote since it might be different than the local value
            this->pub.dbTimeout = varUInt64Force(
                varLstGet(configOptionRemote(this->remoteClient, varLstAdd(varLstNew(), varNewStrZ(CFGOPT_DB_TIMEOUT))), 0));
        }
        else
        {
            pgClientOpen(this->client);
            this->pub.dbTimeout = cfgOptionUInt64(cfgOptDbTimeout);
        }

        // Set search_path to prevent overrides of the functions we expect to call. All queries should also be schema-qualified,
        // but this is an extra level protection.
        dbExec(this, STRDEF("set search_path = 'pg_catalog'"));

        // Set client encoding to UTF8. This is the only encoding (other than ASCII) that we can safely work with.
        dbExec(this, STRDEF("set client_encoding = 'UTF8'"));

        // Query the version and data_directory. Be sure the update the total in the null check below when adding/removing columns.
        const Pack *const row = dbQuery(
            this, pgClientQueryResultRow,
            STRDEF(
                "select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"
                " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'checkpoint_timeout')::int4"));

        // Check that none of the return values are null, which indicates the user cannot select some rows in pg_settings
        PackRead *read = pckReadNew(row);

        for (unsigned int columnIdx = 0; columnIdx < 5; columnIdx++)
        {
            if (pckReadNullP(read, .id = columnIdx + 1))
            {
                THROW(
                    DbQueryError,
                    "unable to select some rows from pg_settings\n"
                    "HINT: is the backup running as the postgres user?\n"
                    "HINT: is the pg_read_all_settings role assigned for " PG_NAME " >= " PG_VERSION_10_Z "?");
            }
        }

        // Restart the read to get the data
        read = pckReadNew(row);

        // Strip the minor version off since we don't need it. In the future it might be a good idea to warn users when they are
        // running an old minor version.
        this->pub.pgVersion = (unsigned int)pckReadI32P(read) / 100 * 100;

        // Store the data directory that PostgreSQL is running in, the archive mode, and archive command. These can be compared to
        // the configured pgBackRest directory, and archive settings checked for validity, when validating the configuration.
        // Also store the checkpoint timeout to warn in case a backup is requested without using the start-fast option.
        MEM_CONTEXT_BEGIN(this->pub.memContext)
        {
            this->pub.pgDataPath = pckReadStrP(read);
            this->pub.archiveMode = pckReadStrP(read);
            this->pub.archiveCommand = pckReadStrP(read);
            this->pub.checkpointTimeout = (TimeMSec)pckReadI32P(read) * MSEC_PER_SEC;
        }
        MEM_CONTEXT_END();

        // Set application name to help identify the backup session
        dbExec(this, strNewFmt("set application_name = '%s'", strZ(this->applicationName)));

        // There is no need to have parallelism enabled in a backup session. In particular, 9.6 marks pg_stop_backup() as
        // parallel-safe but an error will be thrown if pg_stop_backup() is run in a worker.
        dbExec(this, STRDEF("set max_parallel_workers_per_gather = 0"));

        // Is the cluster a standby?
        this->pub.standby = dbIsInRecovery(this);

        // Get control file
        this->pub.pgControl = pgControlFromFile(this->storage, cfgOptionStrNull(cfgOptPgVersionForce));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to build start backup query
static String *
dbBackupStartQuery(const unsigned int pgVersion, const bool startFast)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(BOOL, startFast);
    FUNCTION_TEST_END();

    // Build query to return start lsn and WAL segment name
    String *result = strCatFmt(
        strNew(),
        "select lsn::text as lsn,\n"
        "       pg_catalog.pg_%sfile_name(lsn)::text as wal_segment_name\n"
        "  from pg_catalog.pg_%s('" PROJECT_NAME " backup started at ' || current_timestamp::text",
        strZ(pgWalName(pgVersion)), pgVersion >= PG_VERSION_15 ? "backup_start" : "start_backup");

    // Start backup after immediate checkpoint
    if (startFast)
    {
        strCatZ(result, ", " TRUE_Z);
    }
    // Else start backup at the next scheduled checkpoint
    else
        strCatZ(result, ", " FALSE_Z);

    // Use non-exclusive backup mode when available
    if (pgVersion <= PG_VERSION_14)
        strCatZ(result, ", " FALSE_Z);

    // Complete query
    strCatZ(result, ") as lsn");

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN DbBackupStartResult
dbBackupStart(Db *const this, const bool startFast, const bool archiveCheck)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(BOOL, startFast);
        FUNCTION_LOG_PARAM(BOOL, archiveCheck);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);

    DbBackupStartResult result = {.lsn = NULL};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // When the start-fast option is disabled and db-timeout is smaller than checkpoint_timeout, the command may timeout
        // before the backup actually starts
        if (!startFast && dbDbTimeout(this) <= dbCheckpointTimeout(this))
        {
            LOG_WARN_FMT(
                CFGOPT_START_FAST " is disabled and " CFGOPT_DB_TIMEOUT " (%" PRIu64 "s) is smaller than the " PG_NAME
                " checkpoint_timeout (%" PRIu64 "s) - timeout may occur before the backup starts",
                dbDbTimeout(this) / MSEC_PER_SEC, dbCheckpointTimeout(this) / MSEC_PER_SEC);
        }

        // If archive check then get the current WAL segment
        const String *walSegmentCheck = NULL;

        if (archiveCheck)
        {
            walSegmentCheck = pckReadStrP(
                dbQueryColumn(
                    this,
                    strNewFmt(
                        "select pg_catalog.pg_%sfile_name(pg_catalog.pg_current_%s_insert_%s())::text",
                        strZ(pgWalName(dbPgVersion(this))), strZ(pgWalName(dbPgVersion(this))),
                        strZ(pgLsnName(dbPgVersion(this))))));
        }

        // Start backup
        PackRead *const read = dbQueryRow(this, dbBackupStartQuery(dbPgVersion(this), startFast));

        // Make sure the backup start checkpoint was written to pg_control. This helps ensure that we have a consistent view of the
        // storage with PostgreSQL.
        const PgControl pgControl = pgControlFromFile(this->storage, cfgOptionStrNull(cfgOptPgVersionForce));
        const String *const lsnStart = pckReadStrP(read);

        if (pgControl.checkpoint < pgLsnFromStr(lsnStart))
        {
            THROW_FMT(
                DbMismatchError, "current checkpoint '%s' is less than backup start '%s'", strZ(pgLsnToStr(pgControl.checkpoint)),
                strZ(lsnStart));
        }

        // If archive check then make sure WAL segment was switched on start backup. The WAL segment might not have been switched if
        // a switch occurred before the backup and nothing was written since then. In this case, force a switch so we can verify
        // that archiving is functional before starting the backup.
        const String *const walSegmentName = pckReadStrP(read);

        if (archiveCheck && strEq(walSegmentCheck, walSegmentName))
            dbWalSwitch(this);

        // Check that the WAL timeline matches what is in pg_control
        if (pgTimelineFromWalSegment(walSegmentName) != dbPgControl(this).timeline)
        {
            THROW_FMT(
                DbMismatchError, "WAL timeline %u does not match " PG_FILE_PGCONTROL " timeline %u",
                pgTimelineFromWalSegment(walSegmentName), dbPgControl(this).timeline);
        }

        // Return results
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.lsn = strDup(lsnStart);
            result.walSegmentName = strDup(walSegmentName);
            result.walSegmentCheck = strDup(walSegmentCheck);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
// Helper to build stop backup query
static String *
dbBackupStopQuery(const unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    // Build query to return start lsn and WAL segment name
    String *const result = strCatFmt(
        strNew(),
        "select lsn::text as lsn,\n"
        "       pg_catalog.pg_%sfile_name(lsn)::text as wal_segment_name,\n"
        "       labelfile::text as backuplabel_file,\n"
        "       spcmapfile::text as tablespacemap_file\n"
        "  from pg_catalog.pg_%s(",
        strZ(pgWalName(pgVersion)), pgVersion >= PG_VERSION_15 ? "backup_stop" : "stop_backup");

    // Use non-exclusive backup mode when available
    if (pgVersion <= PG_VERSION_14)
        strCatZ(result, FALSE_Z);

    // Disable archive checking since we do this elsewhere
    if (pgVersion >= PG_VERSION_10)
    {
        if (pgVersion <= PG_VERSION_14)
            strCatZ(result, ", ");

        strCatZ(result, FALSE_Z);
    }

    // Complete query
    strCatZ(result, ")");

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN DbBackupStopResult
dbBackupStop(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(this != NULL);

    DbBackupStopResult result = {.lsn = NULL};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Stop backup
        PackRead *const read = dbQueryRow(this, dbBackupStopQuery(dbPgVersion(this)));

        // Return results
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.lsn = pckReadStrP(read);
            result.walSegmentName = pckReadStrP(read);
            result.backupLabel = pckReadStrP(read);

            // Return the tablespace map if it is not empty
            String *const tablespaceMap = pckReadStrP(read);
            String *const tablespaceMapTrim = strTrim(strDup(tablespaceMap));

            if (!strEmpty(tablespaceMapTrim))
                result.tablespaceMap = tablespaceMap;
            else
                strFree(tablespaceMap);

            strFree(tablespaceMapTrim);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
dbList(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        PACK,
        dbQuery(
            this, pgClientQueryResultAny,
            STRDEF(
                "select oid::oid, datname::text, (select oid::oid from pg_catalog.pg_database where datname = 'template0')"
                " as datlastsysoid from pg_catalog.pg_database")));
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbReplayWait(Db *const this, const String *const targetLsn, const uint32_t targetTimeline, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, targetLsn);
        FUNCTION_LOG_PARAM(UINT, targetTimeline);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(targetLsn != NULL);
    ASSERT(targetTimeline != 0);
    ASSERT(timeout > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Standby checkpoint before the backup started must be <= the target LSN. If not, it indicates that the standby was ahead
        // of the primary and cannot be following it.
        if (dbPgControl(this).checkpoint > pgLsnFromStr(targetLsn))
        {
            THROW_FMT(
                DbMismatchError, "standby checkpoint '%s' is ahead of target '%s'", strZ(pgLsnToStr(dbPgControl(this).checkpoint)),
                strZ(targetLsn));
        }

        // Loop until lsn has been reached or timeout
        Wait *wait = waitNew(timeout);
        bool targetReached = false;
        const char *const lsnName = strZ(pgLsnName(dbPgVersion(this)));
        const String *const replayLsnFunction = strNewFmt(
            "pg_catalog.pg_last_%s_replay_%s()", strZ(pgWalName(dbPgVersion(this))), lsnName);
        const String *replayLsn = NULL;

        do
        {
            // Build the query
            String *const query = strCatFmt(
                strNew(),
                "select replayLsn::text,\n"
                "       (replayLsn > '%s')::bool as targetReached",
                strZ(targetLsn));

            if (replayLsn != NULL)
            {
                strCatFmt(
                    query,
                    ",\n"
                    "       (replayLsn > '%s')::bool as replayProgress",
                    strZ(replayLsn));
            }

            strCatFmt(
                query,
                "\n"
                "  from %s as replayLsn",
                strZ(replayLsnFunction));

            // Execute the query and get replayLsn
            PackRead *const read = dbQueryRow(this, query);
            replayLsn = pckReadStrP(read);

            // Error when replayLsn is null which indicates that this is not a standby. This should have been sorted out before we
            // connected but it's possible that the standby was promoted in the meantime.
            if (replayLsn == NULL)
            {
                THROW_FMT(
                    ArchiveTimeoutError,
                    "unable to query replay lsn on the standby using '%s'\n"
                    "HINT: Is this a standby?",
                    strZ(replayLsnFunction));
            }

            targetReached = pckReadBoolP(read);

            // If the target has not been reached but progress is being made then reset the timer
            if (!targetReached && pckReadBoolP(read, .defaultValue = true))
                wait = waitNew(timeout);

            protocolKeepAlive();
        }
        while (!targetReached && waitMore(wait));

        // Error if a timeout occurred before the target lsn was reached
        if (!targetReached)
        {
            THROW_FMT(
                ArchiveTimeoutError,
                "timeout before standby replayed to %s - only reached %s\n"
                "HINT: is replication running and current on the standby?\n"
                "HINT: disable the 'backup-standby' option to backup directly from the primary.",
                strZ(targetLsn), strZ(replayLsn));
        }

        // Perform a checkpoint
        dbExec(this, STRDEF("checkpoint"));

        // Loop until the checkpoint has reached the target lsn (or timeout)
        wait = waitNew(timeout);
        targetReached = false;
        const String *checkpointLsn = NULL;

        do
        {
            // Build the query
            const String *const query = strNewFmt(
                "select (checkpoint_%s >= '%s')::bool as targetReached,\n"
                "       checkpoint_%s::text as checkpointLsn\n"
                "  from pg_catalog.pg_control_checkpoint()",
                lsnName, strZ(targetLsn), lsnName);

            // Execute the query and get checkpointLsn
            PackRead *const read = dbQueryRow(this, query);
            targetReached = pckReadBoolP(read);
            checkpointLsn = pckReadStrP(read);

            protocolKeepAlive();
        }
        while (!targetReached && waitMore(wait));

        // Error if a timeout occurred before the target lsn was reached
        if (!targetReached)
        {
            THROW_FMT(
                ArchiveTimeoutError, "timeout before standby checkpoint lsn reached %s - only reached %s", strZ(targetLsn),
                strZ(checkpointLsn));
        }

        // Reload pg_control in case timeline was updated by the checkpoint
        this->pub.pgControl = pgControlFromFile(this->storage, cfgOptionStrNull(cfgOptPgVersionForce));

        // Check that the timeline matches the primary
        if (dbPgControl(this).timeline != targetTimeline)
            THROW_FMT(DbMismatchError, "standby is on timeline %u but expected %u", dbPgControl(this).timeline, targetTimeline);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN StringList *
dbSyncStandbyList(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StringList *const result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PackRead *const read = pckReadNew(
            dbQuery(
                this, pgClientQueryResultAny,
                STRDEF("select application_name::text, sync_state::text from pg_catalog.pg_stat_replication")));

        while (!pckReadNullP(read))
        {
            pckReadArrayBeginP(read);

            const String *const appName = pckReadStrP(read);
            const String *const syncState = pckReadStrP(read);

            pckReadArrayEndP(read);

            // Only privileged users can see sync_state so without it standbys cannot be classified
            if (syncState == NULL)
            {
                THROW(
                    DbQueryError,
                    "unable to read sync_state from pg_stat_replication\n"
                    "HINT: does the user have the pg_read_all_stats role or superuser?");
            }

            if (strEqZ(syncState, "sync") || strEqZ(syncState, "quorum"))
                strLstAdd(result, appName);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
dbWalReceiverAppName(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Before PostgreSQL 12 primary_conninfo is in recovery.conf rather than a setting so it cannot be read
        if (dbPgVersion(this) < PG_VERSION_12)
            THROW(DbQueryError, "unable to read primary_conninfo on " PG_NAME " < " PG_VERSION_12_Z);

        PackRead *const read = pckReadNew(
            dbQuery(
                this, pgClientQueryResultAny,
                STRDEF(
                    "select (select setting from pg_catalog.pg_settings where name = 'primary_conninfo')::text,"
                    " pg_catalog.current_setting('cluster_name')::text"
                    " from pg_catalog.pg_stat_wal_receiver")));

        // A standby replaying from the archive or reconnecting has no WAL receiver so its replication state is unknown
        if (pckReadNullP(read))
            THROW(DbQueryError, "WAL receiver is not running");

        pckReadArrayBeginP(read);
        const String *const connInfo = pckReadStrP(read);
        const String *const clusterName = pckReadStrP(read);
        pckReadArrayEndP(read);

        // Only privileged users can see primary_conninfo
        if (connInfo == NULL)
        {
            THROW(
                DbQueryError,
                "unable to read primary_conninfo from pg_settings\n"
                "HINT: does the user have the pg_read_all_settings role or superuser?");
        }

        // The WAL receiver connects with primary_conninfo. When application_name is not set it falls back to cluster_name or
        // walreceiver when cluster_name is empty, which overrides a -c application_name in options. libpq does not send an empty
        // application_name, so when it is set to '' a -c application_name in options wins and the standby may be misclassified.
        const String *appName = pgClientConnInfoValue(connInfo, "application_name");

        if (appName == NULL)
            appName = strEmpty(clusterName) ? DB_WAL_RECEIVER_NAME_STR : clusterName;

        // Normalize the name the same way the primary does before it is reported in pg_stat_replication. Characters that are not
        // printable ASCII are replaced with ? or, as of PostgreSQL 16, hex escapes. Since the result is ASCII it can be clipped to
        // NAMEDATALEN - 1 bytes (assuming the default NAMEDATALEN) without splitting a multibyte character.
        for (const char *appNameChar = strZ(appName); *appNameChar != '\0'; appNameChar++)
        {
            const unsigned char byte = (unsigned char)*appNameChar;

            if (byte >= 32 && byte <= 126)
                strCatChr(result, (char)byte);
            else if (dbPgVersion(this) >= PG_VERSION_16)
                strCatFmt(result, "\\x%02x", byte);
            else
                strCatChr(result, '?');
        }

        if (strSize(result) > DB_APPLICATION_NAME_MAX)
            strTruncIdx(result, DB_APPLICATION_NAME_MAX);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbPing(Db *const this, const bool force)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(BOOL, force);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Ping if forced or interval has elapsed
        const time_t timeNow = time(NULL);

        if (force || timeNow - this->pingTimeLast > DB_PING_SEC)
        {
            // Make sure recovery state has not changed
            if (dbIsInRecovery(this) != dbIsStandby(this))
            {
                // If this is the standby then it must have been promoted
                if (dbIsStandby(this))
                {
                    THROW(
                        DbMismatchError,
                        "standby is no longer in recovery\n"
                        "HINT: was the standby promoted during the backup?");
                }
                // Else if a primary then something has gone seriously wrong
                else
                    THROW(AssertError, "primary has switched to recovery");
            }

            this->pingTimeLast = timeNow;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN Pack *
dbTablespaceList(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        PACK,
        dbQuery(this, pgClientQueryResultAny, STRDEF("select oid::oid, spcname::text from pg_catalog.pg_tablespace")));
}

/**********************************************************************************************************************************/
FN_EXTERN TimeMSec
dbTimeMSec(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    TimeMSec result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = (TimeMSec)pckReadI64P(
            dbQueryColumn(this, STRDEF("select (extract(epoch from clock_timestamp()) * 1000)::bigint")));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(TIME_MSEC, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
dbWalSwitch(Db *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create a restore point to ensure current WAL will be archived
        dbQueryColumn(this, STRDEF("select pg_catalog.pg_create_restore_point('" PROJECT_NAME " Archive Check')::text"));

        // Request a WAL segment switch
        const char *const walName = strZ(pgWalName(dbPgVersion(this)));
        const String *const walFileName = pckReadStrP(
            dbQueryColumn(this, strNewFmt("select pg_catalog.pg_%sfile_name(pg_catalog.pg_switch_%s())::text", walName, walName)));

        // Copy WAL segment name to the prior context
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(walFileName);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
dbToLog(const Db *const this, StringStatic *const debugLog)
{
    strStcCat(debugLog, "{client: ");
    strStcResultSizeInc(
        debugLog, FUNCTION_LOG_OBJECT_FORMAT(this->client, pgClientToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));

    strStcCat(debugLog, ", remoteClient: ");
    strStcResultSizeInc(
        debugLog,
        FUNCTION_LOG_OBJECT_FORMAT(
            this->remoteClient, protocolClientToLog, strStcRemains(debugLog), strStcRemainsSize(debugLog)));
    strStcCatChr(debugLog, '}');
}
