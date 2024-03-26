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
FN_EXTERN ProtocolServerOpenResult
dbOpenProtocol(PackRead *const param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(param == NULL);

    ProtocolServerOpenResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        PgClient *const pgClient = pgClientNew(
            cfgOptionStrNull(cfgOptPgSocketPath), cfgOptionUInt(cfgOptPgPort), cfgOptionStr(cfgOptPgDatabase),
            cfgOptionStrNull(cfgOptPgUser), cfgOptionUInt64(cfgOptDbTimeout));
        pgClientOpen(pgClient);

        result.sessionData = pgClientMove(pgClient, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerProcessSessionResult
dbQueryProtocol(PackRead *const param, void *const pgClient)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PG_CLIENT, pgClient);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(param != NULL);
    ASSERT(pgClient != NULL);

    ProtocolServerProcessSessionResult result = {.data = protocolPackNew()};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const PgClientQueryResult resultType = (PgClientQueryResult)pckReadStrIdP(param);
        const String *const query = pckReadStrP(param);

        pckWritePackP(result.data, pgClientQuery(pgClient, query, resultType));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}
