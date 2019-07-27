/***********************************************************************************************************************************
Database Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "db/db.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Db
{
    MemContext *memContext;
    PgClient *client;
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
Execute a query that returns a single row and column
***********************************************************************************************************************************/
Variant *
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
Is this instance a standby?
***********************************************************************************************************************************/
bool
dbIsStandby(Db *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, varBool(dbQueryColumn(this, STRDEF("select pg_is_in_recovery()"))));
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
