/***********************************************************************************************************************************
Database Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "db/db.h"
#include "db/protocol.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define PG_BACKUP_ADVISORY_LOCK                                     "12340078987004321"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Db
{
    MemContext *memContext;
    PgClient *client;                                               // Local PostgreSQL client
    ProtocolClient *remoteClient;                                   // Protocol client for remote db queries
    unsigned int remoteIdx;                                         // Index provided by the remote on open for subsequent calls
    const String *applicationName;                                  // Used to identify this connection in PostgreSQL

    unsigned int pgVersion;                                         // Version as reported by the database
    const String *pgDataPath;                                       // Data directory reported by the database
    const String *archiveMode;                                      // The archive_mode reported by the database
    const String *archiveCommand;                                   // The archive_command reported by the database
};

OBJECT_DEFINE_MOVE(DB);
OBJECT_DEFINE_FREE(DB);

/***********************************************************************************************************************************
Close protocol connection.  No need to close a locally created PgClient since it has its own destructor.
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(DB, LOG, logLevelTrace)
{
    ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_CLOSE_STR);
    protocolCommandParamAdd(command, VARUINT(this->remoteIdx));

    protocolClientExecute(this->remoteClient, command, false);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
Db *
dbNew(PgClient *client, ProtocolClient *remoteClient, const String *applicationName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, client);
        FUNCTION_LOG_PARAM(PROTOCOL_CLIENT, remoteClient);
        FUNCTION_LOG_PARAM(STRING, applicationName);
    FUNCTION_LOG_END();

    ASSERT((client != NULL && remoteClient == NULL) || (client == NULL && remoteClient != NULL));
    ASSERT(applicationName != NULL);

    Db *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Db")
    {
        this = memNew(sizeof(Db));
        this->memContext = memContextCurrent();

        this->client = pgClientMove(client, this->memContext);
        this->remoteClient = remoteClient;
        this->applicationName = strDup(applicationName);
    }
    MEM_CONTEXT_NEW_END();

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
        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_QUERY_STR);
        protocolCommandParamAdd(command, VARUINT(this->remoteIdx));
        protocolCommandParamAdd(command, VARSTR(query));

        result = varVarLst(protocolClientExecute(this->remoteClient, command, true));
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

    CHECK(dbQuery(this, command) == NULL);

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

    CHECK(varLstSize(result) == 1);
    CHECK(varLstSize(varVarLst(varLstGet(result, 0))) == 1);

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

    CHECK(varLstSize(result) == 1);

    FUNCTION_LOG_RETURN(VARIANT_LIST, varVarLst(varLstGet(result, 0)));
}

/***********************************************************************************************************************************
Open the db connection
***********************************************************************************************************************************/
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
            ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_DB_OPEN_STR);
            this->remoteIdx = varUIntForce(protocolClientExecute(this->remoteClient, command, true));

            // Set a callback to notify the remote when a connection is closed
            memContextCallbackSet(this->memContext, dbFreeResource, this);
        }
        else
            pgClientOpen(this->client);

        // Set search_path to prevent overrides of the functions we expect to call.  All queries should also be schema-qualified,
        // but this is an extra level protection.
        dbExec(this, STRDEF("set search_path = 'pg_catalog'"));

        // Query the version and data_directory
        VariantList *row = dbQueryRow(
            this,
            STRDEF(
                "select (select setting from pg_catalog.pg_settings where name = 'server_version_num')::int4,"
                " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_mode')::text,"
                " (select setting from pg_catalog.pg_settings where name = 'archive_command')::text"));

        // Strip the minor version off since we don't need it.  In the future it might be a good idea to warn users when they are
        // running an old minor version.
        this->pgVersion = varUIntForce(varLstGet(row, 0)) / 100 * 100;

        // Store the data directory that PostgreSQL is running in, the archive mode, and archive command. These can be compared to
        // the configured pgBackRest directory, and archive settings checked for validity, when validating the configuration.
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->pgDataPath = strDup(varStr(varLstGet(row, 1)));
            this->archiveMode = strDup(varStr(varLstGet(row, 2)));
            this->archiveCommand = strDup(varStr(varLstGet(row, 3)));
        }
        MEM_CONTEXT_END();

        if (this->pgVersion >= PG_VERSION_APPLICATION_NAME)
            dbExec(this, strNewFmt("set application_name = '%s'", strPtr(this->applicationName)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Start a backup and return starting lsn and wal segment name
***********************************************************************************************************************************/
#define FUNCTION_LOG_DB_BACKUP_START_RESULT_TYPE                                                                                   \
    DbBackupStartResult
#define FUNCTION_LOG_DB_BACKUP_START_RESULT_FORMAT(value, buffer, bufferSize)                                                      \
    objToLog(&value, "DbBackupStartResult", buffer, bufferSize)

typedef struct DbBackupStartResult
{
    String *lsn;
    String *walSegmentName;
} DbBackupStartResult;

// Helper to build start start backup query
static String *
dbBackupStartQuery(unsigned int pgVersion, const String *backupLabel, bool startFast)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, pgVersion);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(BOOL, startFast);
    FUNCTION_TEST_END();

    // Build query to return start lsn and WAL segment name
    String *result = strNewFmt(
        "select lsn::text as lsn,\n"
        "       pg_catalog.pg_%s_file_name(lsn)::text as wal_segment_name\n"
        "  from pg_catalog.pg_start_backup('" PROJECT_NAME " backup %s started at ' | current_timestamp",
    strPtr(pgWalName(pgVersion)), strPtr(backupLabel));

    // Start backup after an immediate checkpoint
    if (startFast)
    {
        strCatFmt(result, ", true");
    }
    // Else start backup at the next scheduled checkpoint
    else if (pgVersion >= PG_VERSION_84)
        strCatFmt(result, ", false");

    // Disable archive checking in pg_stop_backup() since we already do this
    if (pgVersion >= PG_VERSION_96)
        strCatFmt(result, ", false");

    // Complete query
    strCatFmt(result, ") as lsn");

    // !!! my ($strTimestampDbStart, $strArchiveStart, $strLsnStart, $iWalSegmentSize) = $self->executeSqlRow(
    //     "select to_char(current_timestamp, 'YYYY-MM-DD HH24:MI:SS.US TZ'), pg_" . $self->walId() . "file_name(lsn), lsn::text," .
    //         ($self->{strDbVersion} < PG_VERSION_84 ? PG_WAL_SIZE_83 :
    //             " (select setting::int8 from pg_settings where name = 'wal_segment_size')" .
    //             # In Pre-11 versions the wal_segment_sise was expressed in terms of blocks rather than total size
    //             ($self->{strDbVersion} < PG_VERSION_11 ?
    //                 " * (select setting::int8 from pg_settings where name = 'wal_block_size')" : '')) .
    //         " from pg_start_backup('${strLabel}'" .
    //         ($bStartFast ? ', true' : $self->{strDbVersion} >= PG_VERSION_84 ? ', false' : '') .
    //         ($self->{strDbVersion} >= PG_VERSION_96 ? ', false' : '') . ') as lsn');

    FUNCTION_TEST_RETURN(result);
}

DbBackupStartResult
dbBackupStart(Db *this, const String *backupLabel, bool startFast)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
        FUNCTION_LOG_PARAM(BOOL, startFast);
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

        // -------------------------------------------------------------------------------------------------------------------------
        // !!! If stop-auto is enabled check for a running backup.  This feature is not supported for PostgreSQL >= 9.6 since backups are
        // # run in non-exclusive mode.
        // if (cfgOption(CFGOPT_STOP_AUTO) && $self->{strDbVersion} < PG_VERSION_96)
        // {
        //     # Running backups can only be detected in PostgreSQL >= 9.3
        //     if ($self->{strDbVersion} >= PG_VERSION_93)
        //     {
        //         # If a backup is currently in progress emit a warning and then stop it
        //         if ($self->executeSqlOne('select pg_is_in_backup()'))
        //         {
        //             &log(WARN, 'the cluster is already in backup mode but no ' . PROJECT_NAME . ' backup process is running.' .
        //                        ' pg_stop_backup() will be called so a new backup can be started.');
        //             $self->backupStop();
        //         }
        //     }

        // Start backup
        VariantList *row = dbQueryRow(this, dbBackupStartQuery(dbPgVersion(this), backupLabel, startFast));

        // Return results
        memContextSwitch(MEM_CONTEXT_OLD());
        result.lsn = strDup(varStr(varLstGet(row, 0)));
        result.walSegmentName = strDup(varStr(varLstGet(row, 1)));
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(DB_BACKUP_START_RESULT, result);
}

/***********************************************************************************************************************************
Is this instance a standby?
***********************************************************************************************************************************/
bool
dbIsStandby(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    if (this->pgVersion >= PG_VERSION_HOT_STANDBY)
    {
        result = varBool(dbQueryColumn(this, STRDEF("select pg_catalog.pg_is_in_recovery()")));
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Switch the WAL segment and return the segment that should have been archived
***********************************************************************************************************************************/
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
        if (this->pgVersion >= PG_VERSION_RESTORE_POINT)
            dbQueryColumn(this, STRDEF("select pg_catalog.pg_create_restore_point('" PROJECT_NAME " Archive Check')::text"));

        // Request a WAL segment switch
        const char *walName = strPtr(pgWalName(this->pgVersion));
        const String *walFileName = varStr(
            dbQueryColumn(this, strNewFmt("select pg_catalog.pg_%sfile_name(pg_catalog.pg_switch_%s())::text", walName, walName)));

        // Copy WAL segment name to the calling context
        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(walFileName);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Get pg data path loaded from the data_directory GUC
***********************************************************************************************************************************/
const String *
dbPgDataPath(const Db *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->pgDataPath);
}

/***********************************************************************************************************************************
Get pg version loaded from the server_version_num GUC
***********************************************************************************************************************************/
unsigned int
dbPgVersion(const Db *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->pgVersion);
}

/***********************************************************************************************************************************
Get pg version loaded from the server_version_num GUC
***********************************************************************************************************************************/
const String *
dbArchiveMode(const Db *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->archiveMode);
}

/***********************************************************************************************************************************
Get pg version loaded from the server_version_num GUC
***********************************************************************************************************************************/
const String *
dbArchiveCommand(const Db *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->archiveCommand);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
dbToLog(const Db *this)
{
    return strNewFmt(
        "{client: %s, remoteClient: %s}", this->client == NULL ? "null" : strPtr(pgClientToLog(this->client)),
        this->remoteClient == NULL ? "null" : strPtr(protocolClientToLog(this->remoteClient)));
}
