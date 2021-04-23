/***********************************************************************************************************************************
Verify Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/verify/file.h"
#include "command/verify/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
void
verifyFileProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        VerifyResult result = verifyFile(
            varStr(varLstGet(paramList, 0)),                        // Full filename
            varStr(varLstGet(paramList, 1)),                        // Checksum
            varUInt64(varLstGet(paramList, 2)),                     // File size
            varStr(varLstGet(paramList, 3)));                       // Cipher pass

        protocolServerResponse(server, VARUINT(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
