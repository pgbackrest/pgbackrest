/***********************************************************************************************************************************
Database Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "db/db.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Db
{
    MemContext *memContext;
    PgClient *client;

    unsigned int pgVersion;                                         // Version as reported by the database
    const String *pgDataPath;                                       // Data directory reported by the database
};

OBJECT_DEFINE_FREE(DB);

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
Db *
dbNew(PgClient *client)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PG_CLIENT, client);
    FUNCTION_LOG_END();

    ASSERT(client != NULL);

    Db *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Db")
    {
        this = memNew(sizeof(Db));
        this->memContext = memContextCurrent();

        this->client = pgClientMove(client, this->memContext);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(DB, this);
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

    CHECK(pgClientQuery(this->client, command) == NULL);

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

    VariantList *result = pgClientQuery(this->client, query);

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

    VariantList *result = pgClientQuery(this->client, query);

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
        pgClientOpen(this->client);

        // Set search_path to prevent overrides of the functions we expect to call.  All queries should also be schema-qualified, but
        // this is an extra protection.
        dbExec(this, STRDEF("set search_path = 'pg_catalog'"));

        // Query the version and data_directory
        VariantList *row = dbQueryRow(
            this,
            STRDEF(
                "select (select setting from pg_settings where name = 'server_version_num')::int4,"
                " (select setting from pg_settings where name = 'data_directory')::text"));

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
            dbExec(this, strNewFmt("set application_name = '%s'", "dude"));
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

    FUNCTION_LOG_RETURN(BOOL, varBool(dbQueryColumn(this, STRDEF("select pg_catalog.pg_is_in_recovery()"))));
}

/***********************************************************************************************************************************
Close the db connection
***********************************************************************************************************************************/
void
dbClose(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    pgClientClose(this->client);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Move the db object to a new context
***********************************************************************************************************************************/
Db *
dbMove(Db *this, MemContext *parentNew)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(DB, this);
        FUNCTION_TEST_PARAM(MEM_CONTEXT, parentNew);
    FUNCTION_TEST_END();

    ASSERT(parentNew != NULL);

    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
dbToLog(const Db *this)
{
    return strNewFmt("{client: %s}", strPtr(pgClientToLog(this->client)));
}
