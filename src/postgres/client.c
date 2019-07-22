/***********************************************************************************************************************************
Postgres Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/type/string.h"
#include "postgres/client.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct PgClient
{
    MemContext *memContext;
    const String *host;
    unsigned int port;
    const String *database;
    const String *user;
};

OBJECT_DEFINE_FREE(PG_CLIENT);

/***********************************************************************************************************************************
Close protocol connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(PG_CLIENT, LOG, logLevelTrace)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // pgClientWriteCommand(this, protocolCommandNew(PROTOCOL_COMMAND_EXIT_STR));
    }
    MEM_CONTEXT_TEMP_END();
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Create object
***********************************************************************************************************************************/
PgClient *
pgClientNew(const String *host, const unsigned int port, const String *database, const String *user)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(STRING, database);
        FUNCTION_LOG_PARAM(STRING, user);
    FUNCTION_LOG_END();

    ASSERT(port >= 1 && port <= 65535);
    ASSERT(database != NULL);

    PgClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("PgClient")
    {
        this = memNew(sizeof(PgClient));
        this->memContext = memContextCurrent();

        this->host = strDup(host);
        this->port = port;
        this->database = strDup(database);
        this->user = strDup(user);

        // postgresql:///mydb?host=localhost&port=5433

        // Set a callback to shutdown the connection
       memContextCallbackSet(this->memContext, pgClientFreeResource, this);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, this);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
pgClientToLog(const PgClient *this)
{
    return strNewFmt(
        "{host: %s, port: %u, database: %s, user: %s}", strPtr(strToLog(this->host)), this->port, strPtr(strToLog(this->database)),
        strPtr(strToLog(this->user)));
}
