/***********************************************************************************************************************************
Db Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "config/config.h"
#include "db/protocol.h"
#include "postgres/client.h"
#include "postgres/interface.h"

/**********************************************************************************************************************************/
FN_EXTERN void *
dbOpenProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param == NULL);
    ASSERT(server != NULL);

    PgClient *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = pgClientNew(
            cfgOptionStrNull(cfgOptPgSocketPath), cfgOptionUInt(cfgOptPgPort), cfgOptionStr(cfgOptPgDatabase),
            cfgOptionStrNull(cfgOptPgUser), cfgOptionUInt64(cfgOptDbTimeout));
        pgClientOpen(result);

        protocolServerDataPut(server, NULL);

        pgClientMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PG_CLIENT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
dbQueryProtocol(PackRead *const param, ProtocolServer *const server, void *const pgClient)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
        FUNCTION_LOG_PARAM(PG_CLIENT, pgClient);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);
    ASSERT(pgClient != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const PgClientQueryResult resultType = (PgClientQueryResult)pckReadStrIdP(param);
        const String *const query = pckReadStrP(param);

        protocolServerDataPut(server, pckWritePackP(protocolPackNew(), pgClientQuery(pgClient, query, resultType)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, true);
}
