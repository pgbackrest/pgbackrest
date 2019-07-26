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
Render as string for logging
***********************************************************************************************************************************/
String *
dbToLog(const Db *this)
{
    return strNewFmt("{client: %s}", strPtr(pgClientToLog(this->client)));
}
