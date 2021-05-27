/***********************************************************************************************************************************
Db Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/list.h"
#include "config/config.h"
#include "db/protocol.h"
#include "postgres/client.h"
#include "postgres/interface.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct
{
    List *pgClientList;                                             // List of db objects
} dbProtocolLocal;

/**********************************************************************************************************************************/
void
dbOpenProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the db list does not exist then create it in the prior context (which should be persistent)
        if (dbProtocolLocal.pgClientList == NULL)
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                dbProtocolLocal.pgClientList = lstNewP(sizeof(PgClient *));
            }
            MEM_CONTEXT_PRIOR_END();
        }

        // Add db to the list
        unsigned int dbIdx = lstSize(dbProtocolLocal.pgClientList);

        // Return db index which should be included in subsequent calls
        protocolServerResponseVar(server, VARUINT(dbIdx));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
dbQueryProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PgClient *const pgClient = *(PgClient **)lstGet(dbProtocolLocal.pgClientList, pckReadU32P(param));
        const String *const query = pckReadStrP(param);

        protocolServerResponseVar(server, varNewVarLst(pgClientQuery(pgClient, query)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
dbCloseProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        pgClientClose(*(PgClient **)lstGet(dbProtocolLocal.pgClientList, pckReadU32P(param)));
        protocolServerResponseVar(server, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
