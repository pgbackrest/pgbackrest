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
                " (select setting from pg_catalog.pg_settings where name = 'data_directory')::text"));

        // Strip the minor version off since we don't need it.  In the future it might be a good idea to warn users when they are
        // running an old minor version.
        this->pgVersion = varUIntForce(varLstGet(row, 0)) / 100 * 100;

        // Store the data directory that PostgreSQL is running in.  This can be compared to the configured pgBackRest directory when
        // validating the configuration.
        MEM_CONTEXT_BEGIN(this->memContext)
        {
            this->pgDataPath = strDup(varStr(varLstGet(row, 1)));
        }
        MEM_CONTEXT_END();

        if (this->pgVersion >= PG_VERSION_APPLICATION_NAME)
            dbExec(this, strNewFmt("set application_name = '%s'", strPtr(this->applicationName)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
Render as string for logging
***********************************************************************************************************************************/
String *
dbToLog(const Db *this)
{
    return strNewFmt(
        "{client: %s, remoteClient: %s}", this->client == NULL ? "null" : strPtr(pgClientToLog(this->client)),
        this->remoteClient == NULL ? "null" : strPtr(protocolClientToLog(this->remoteClient)));
}
