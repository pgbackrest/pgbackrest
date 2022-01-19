/***********************************************************************************************************************************
Database Client
***********************************************************************************************************************************/
#include "build.auto.h"

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

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Db
{
    DbPub pub;                                                      // Publicly accessible variables
    PgClient *client;                                               // Local PostgreSQL client
    ProtocolClient *remoteClient;                                   // Protocol client for remote db queries
    unsigned int remoteIdx;                                         // Index provided by the remote on open for subsequent calls
    const Storage *storage;                                         // PostgreSQL storage
    const String *applicationName;                                  // Used to identify this connection in PostgreSQL
    time_t pingTimeLast;                                            // Last time cluster was pinged
};

/***********************************************************************************************************************************
Close protocol connection.  No need to close a locally created PgClient since it has its own destructor.
***********************************************************************************************************************************/
static void
dbFreeResource(THIS_VOID)
{
    THIS(Db);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_CLOSE);
    pckWriteU32P(protocolCommandParam(command), this->remoteIdx);

    protocolClientExecute(this->remoteClient, command, false);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
Db *
dbNew(PgClient *client, ProtocolClient *remoteClient, const Storage *const storage, const String *applicationName)
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

    Db *this = NULL;

    OBJ_NEW_BEGIN(Db)
    {
        this = OBJ_NEW_ALLOC();

        *this = (Db)
        {
            .pub =
            {
                .memContext = memContextCurrent(),
            },
            .remoteClient = remoteClient,
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
static VariantList *
dbQuery(Db *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    VariantList *result = NULL;

    // Query remotely
    if (this->remoteClient != NULL)
    {
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_QUERY);
        PackWrite *const param = protocolCommandParam(command);

        pckWriteU32P(param, this->remoteIdx);
        pckWriteStrP(param, query);

        result = varVarLst(jsonToVar(pckReadStrP(protocolClientExecute(this->remoteClient, command, true))));
    }
    // Else locally
    else
        result = pgClientQuery(this->client, query);

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}

/***********************************************************************************************************************************
Execute a command that expects no output
***********************************************************************************************************************************/
static void
dbExec(Db *this, const String *command)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, command);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    CHECK(AssertError, dbQuery(this, command) == NULL, "exec returned data");

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Execute a query that returns a single row and column
***********************************************************************************************************************************/
static Variant *
dbQueryColumn(Db *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    VariantList *result = dbQuery(this, query);

    CHECK(AssertError, varLstSize(result) == 1, "query must return one column");
    CHECK(AssertError, varLstSize(varVarLst(varLstGet(result, 0))) == 1, "query must return one row");

    FUNCTION_LOG_RETURN(VARIANT, varLstGet(varVarLst(varLstGet(result, 0)), 0));
}

/***********************************************************************************************************************************
Execute a query that returns a single row
***********************************************************************************************************************************/
static VariantList *
dbQueryRow(Db *this, const String *query)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, query);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(query != NULL);

    VariantList *result = dbQuery(this, query);

    CHECK(AssertError, varLstSize(result) == 1, "query must return one row");

    FUNCTION_LOG_RETURN(VARIANT_LIST, varVarLst(varLstGet(result, 0)));
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

    bool result = false;

    if (dbPgVersion(this) >= PG_VERSION_HOT_STANDBY)
        result = varBool(dbQueryColumn(this, STRDEF("select pg_catalog.pg_is_in_recovery()")));

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
void
dbOpen(Db *this)
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
            ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_OPEN);
            this->remoteIdx = pckReadU32P(protocolClientExecute(this->remoteClient, command, true));

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

        // Set search_path to prevent overrides of the functions we expect to call.  All queries should also be schema-qualified,
        // but this is an extra level protection.
        dbExec(this, STRDEF("set search_path = 'pg_catalog'"));

        // Set client encoding to UTF8.  This is the only encoding (other than ASCII) that we can safely work with.
        dbExec(this, STRDEF("set client_encoding = 'UTF8'"));

        // Query the version and data_directory
        VariantList *row = dbQueryRow(
            this,
            STRDEF(
                "select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"
                " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'checkpoint_timeout')::int4"));

        // Check that none of the return values are null, which indicates the user cannot select some rows in pg_settings
        for (unsigned int columnIdx = 0; columnIdx < varLstSize(row); columnIdx++)
        {
            if (varLstGet(row, columnIdx) == NULL)
            {
                THROW(
                    DbQueryError,
                    "unable to select some rows from pg_settings\n"
                        "HINT: is the backup running as the postgres user?\n"
                        "HINT: is the pg_read_all_settings role assigned for " PG_NAME " >= " PG_VERSION_10_STR "?");
            }
        }

        // Strip the minor version off since we don't need it.  In the future it might be a good idea to warn users when they are
        // running an old minor version.
        this->pub.pgVersion = varUIntForce(varLstGet(row, 0)) / 100 * 100;

        // Store the data directory that PostgreSQL is running in, the archive mode, and archive command. These can be compared to
        // the configured pgBackRest directory, and archive settings checked for validity, when validating the configuration.
        // Also store the checkpoint timeout to warn in case a backup is requested without using the start-fast option.
        MEM_CONTEXT_BEGIN(this->pub.memContext)
        {
            this->pub.pgDataPath = strDup(varStr(varLstGet(row, 1)));
            this->pub.archiveMode = strDup(varStr(varLstGet(row, 2)));
            this->pub.archiveCommand = strDup(varStr(varLstGet(row, 3)));
            this->pub.checkpointTimeout = (TimeMSec)(varUIntForce(varLstGet(row, 4))) * MSEC_PER_SEC;
        }
        MEM_CONTEXT_END();

        // Set application name to help identify the backup session
        dbExec(this, strNewFmt("set application_name = '%s'", strZ(this->applicationName)));

        // There is no need to have parallelism enabled in a backup session. In particular, 9.6 marks pg_stop_backup() as
        // parallel-safe but an error will be thrown if pg_stop_backup() is run in a worker.
        if (dbPgVersion(this) >= PG_VERSION_PARALLEL_QUERY)
            dbExec(this, STRDEF("set max_parallel_workers_per_gather = 0"));

        // Is the cluster a standby?
        this->pub.standby = dbIsInRecovery(this);

        // Get control file
        this->pub.pgControl = pgControlFromFile(this->storage);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to build start backup query
static String *
dbBackupStartQuery(unsigned int pgVersion, bool startFast)
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
        "  from pg_catalog.pg_start_backup('" PROJECT_NAME " backup started at ' || current_timestamp",
        strZ(pgWalName(pgVersion)));

    // Start backup after immediate checkpoint
    if (startFast)
    {
        strCatFmt(result, ", " TRUE_Z);
    }
    // Else start backup at the next scheduled checkpoint
    else
        strCatFmt(result, ", " FALSE_Z);

    // Use non-exclusive backup mode when available
    if (pgVersion >= PG_VERSION_96)
        strCatFmt(result, ", " FALSE_Z);

    // Complete query
    strCatFmt(result, ") as lsn");

    FUNCTION_TEST_RETURN(result);
}

DbBackupStartResult
dbBackupStart(Db *const this, const bool startFast, const bool stopAuto, const bool archiveCheck)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(BOOL, startFast);
        FUNCTION_LOG_PARAM(BOOL, stopAuto);
        FUNCTION_LOG_PARAM(BOOL, archiveCheck);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    DbBackupStartResult result = {.lsn = NULL};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Acquire the backup advisory lock to make sure that backups are not running from multiple backup servers against the same
        // database cluster.  This lock helps make the stop-auto option safe.
        if (!varBool(dbQueryColumn(this, STRDEF("select pg_catalog.pg_try_advisory_lock(" PG_BACKUP_ADVISORY_LOCK ")::bool"))))
        {
            THROW(
                LockAcquireError,
                "unable to acquire " PROJECT_NAME " advisory lock\n"
                "HINT: is another " PROJECT_NAME " backup already running on this cluster?");
        }

        // If stop-auto is enabled check for a running backup
        if (stopAuto)
        {
            CHECK(AssertError, dbPgVersion(this) >= PG_VERSION_93, "feature not supported in PostgreSQL < " PG_VERSION_93_STR);

            // Feature is not needed for PostgreSQL >= 9.6 since backups are run in non-exclusive mode
            if (dbPgVersion(this) < PG_VERSION_96)
            {
                if (varBool(dbQueryColumn(this, STRDEF("select pg_catalog.pg_is_in_backup()::bool"))))
                {
                    LOG_WARN(
                        "the cluster is already in backup mode but no " PROJECT_NAME " backup process is running."
                        " pg_stop_backup() will be called so a new backup can be started.");

                    dbBackupStop(this);
                }
            }
        }

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
            walSegmentCheck = varStr(
                dbQueryColumn(
                    this,
                    strNewFmt(
                        "select pg_catalog.pg_%sfile_name(pg_catalog.pg_current_%s_insert_%s())::text",
                        strZ(pgWalName(dbPgVersion(this))), strZ(pgWalName(dbPgVersion(this))),
                        strZ(pgLsnName(dbPgVersion(this))))));
        }

        // Start backup
        VariantList *row = dbQueryRow(this, dbBackupStartQuery(dbPgVersion(this), startFast));

        // Make sure the backup start checkpoint was written to pg_control. This helps ensure that we have a consistent view of the
        // storage with PostgreSQL.
        const PgControl pgControl = pgControlFromFile(this->storage);
        const String *const lsnStart = varStr(varLstGet(row, 0));

        if (pgControl.checkpoint < pgLsnFromStr(lsnStart))
        {
            THROW_FMT(
                DbMismatchError, "current checkpoint '%s' is less than backup start '%s'", strZ(pgLsnToStr(pgControl.checkpoint)),
                strZ(lsnStart));
        }

        // If archive check then make sure WAL segment was switched on start backup
        const String *const walSegmentName = varStr(varLstGet(row, 1));

        if (archiveCheck && strEq(walSegmentCheck, walSegmentName))
        {
            // If the version supports restore points then force a WAL switch
            if (dbPgVersion(this) >= PG_VERSION_RESTORE_POINT)
            {
                dbWalSwitch(this);
            }
            // Else disable the check. All WAL will still be checked at the end of the backup.
            else
                walSegmentCheck = NULL;
        }

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
dbBackupStopQuery(unsigned int pgVersion)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
    FUNCTION_TEST_END();

    // Build query to return start lsn and WAL segment name
    String *result = strCatFmt(
        strNew(),
        "select lsn::text as lsn,\n"
        "       pg_catalog.pg_%sfile_name(lsn)::text as wal_segment_name",
        strZ(pgWalName(pgVersion)));

    // For PostgreSQL >= 9.6 the backup label and tablespace map are returned from pg_stop_backup
    if (pgVersion >= PG_VERSION_96)
    {
        strCatZ(
            result,
            ",\n"
            "       labelfile::text as backuplabel_file,\n"
            "       spcmapfile::text as tablespacemap_file");
    }

    // Build stop backup function
    strCatZ(
        result,
        "\n"
        "  from pg_catalog.pg_stop_backup(");

    // Use non-exclusive backup mode when available
    if (pgVersion >= PG_VERSION_96)
        strCatFmt(result, FALSE_Z);

    // Disable archive checking in pg_stop_backup() since we do this elsewhere
    if (pgVersion >= PG_VERSION_10)
        strCatFmt(result, ", " FALSE_Z);

    // Complete query
    strCatFmt(result, ")");

    if (pgVersion < PG_VERSION_96)
        strCatFmt(result, " as lsn");

    FUNCTION_TEST_RETURN(result);
}

DbBackupStopResult
dbBackupStop(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    DbBackupStopResult result = {.lsn = NULL};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Stop backup
        VariantList *row = dbQueryRow(this, dbBackupStopQuery(dbPgVersion(this)));

        // Check if the tablespace map is empty
        bool tablespaceMapEmpty =
            dbPgVersion(this) >= PG_VERSION_96 ? strSize(strTrim(strDup(varStr(varLstGet(row, 3))))) == 0 : false;

        // Return results
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.lsn = strDup(varStr(varLstGet(row, 0)));
            result.walSegmentName = strDup(varStr(varLstGet(row, 1)));

            if (dbPgVersion(this) >= PG_VERSION_96)
            {
                result.backupLabel = strDup(varStr(varLstGet(row, 2)));

                if (!tablespaceMapEmpty)
                    result.tablespaceMap = strDup(varStr(varLstGet(row, 3)));
            }
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
VariantList *
dbList(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        VARIANT_LIST, dbQuery(this, STRDEF("select oid::oid, datname::text, datlastsysoid::oid from pg_catalog.pg_database")));
}

/**********************************************************************************************************************************/
void
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
        // Check that the timeline matches the primary
        if (dbPgControl(this).timeline != targetTimeline)
            THROW_FMT(DbMismatchError, "standby is on timeline %u but expected %u", dbPgControl(this).timeline, targetTimeline);

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
        const char *lsnName = strZ(pgLsnName(dbPgVersion(this)));
        const String *replayLsnFunction = strNewFmt(
            "pg_catalog.pg_last_%s_replay_%s()", strZ(pgWalName(dbPgVersion(this))), lsnName);
        const String *replayLsn = NULL;

        do
        {
            // Build the query
            String *query = strCatFmt(
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
            VariantList *row = dbQueryRow(this, query);
            replayLsn = varStr(varLstGet(row, 0));

            // Error when replayLsn is null which indicates that this is not a standby.  This should have been sorted out before we
            // connected but it's possible that the standby was promoted in the meantime.
            if (replayLsn == NULL)
            {
                THROW_FMT(
                    ArchiveTimeoutError,
                    "unable to query replay lsn on the standby using '%s'\n"
                    "HINT: Is this a standby?",
                    strZ(replayLsnFunction));
            }

            targetReached = varBool(varLstGet(row, 1));

            // If the target has not been reached but progress is being made then reset the timer
            if (!targetReached && varLstSize(row) > 2 && varBool(varLstGet(row, 2)))
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

        // On PostgreSQL >= 9.6 the checkpoint location can be verified so loop until lsn has been reached or timeout
        if (dbPgVersion(this) >= PG_VERSION_96)
        {
            wait = waitNew(timeout);
            targetReached = false;
            const String *checkpointLsn = NULL;

            do
            {
                // Build the query
                const String *query = strNewFmt(
                    "select (checkpoint_%s > '%s')::bool as targetReached,\n"
                    "       checkpoint_%s::text as checkpointLsn\n"
                    "  from pg_catalog.pg_control_checkpoint()",
                    lsnName, strZ(targetLsn), lsnName);

                // Execute the query and get checkpointLsn
                VariantList *row = dbQueryRow(this, query);
                targetReached = varBool(varLstGet(row, 0));
                checkpointLsn = varStr(varLstGet(row, 1));

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
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
dbPing(Db *const this, const bool force)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(BOOL, force);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Ping if forced or interval has elapsed
        time_t timeNow = time(NULL);

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
VariantList *
dbTablespaceList(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(VARIANT_LIST, dbQuery(this, STRDEF("select oid::oid, spcname::text from pg_catalog.pg_tablespace")));
}

/**********************************************************************************************************************************/
TimeMSec
dbTimeMSec(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        TIME_MSEC, varUInt64Force(dbQueryColumn(this, STRDEF("select (extract(epoch from clock_timestamp()) * 1000)::bigint"))));
}

/**********************************************************************************************************************************/
String *
dbWalSwitch(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create a restore point to ensure current WAL will be archived.  For versions < 9.1 activity will need to be generated by
        // the user if there have been no writes since the last WAL switch.
        if (dbPgVersion(this) >= PG_VERSION_RESTORE_POINT)
            dbQueryColumn(this, STRDEF("select pg_catalog.pg_create_restore_point('" PROJECT_NAME " Archive Check')::text"));

        // Request a WAL segment switch
        const char *walName = strZ(pgWalName(dbPgVersion(this)));
        const String *walFileName = varStr(
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
String *
dbToLog(const Db *this)
{
    return strNewFmt(
        "{client: %s, remoteClient: %s}", this->client == NULL ? NULL_Z : strZ(pgClientToLog(this->client)),
        this->remoteClient == NULL ? NULL_Z : strZ(protocolClientToLog(this->remoteClient)));
}
