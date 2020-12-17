/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/push/file.h"
#include "command/archive/push/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_ARCHIVE_PUSH_STR,                     PROTOCOL_COMMAND_ARCHIVE_PUSH);

/**********************************************************************************************************************************/
bool
archivePushProtocol(const String *command, PackRead *param, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_ARCHIVE_PUSH_STR))
        {
            protocolServerResponse(
                server,
                VARSTR(
                    archivePushFile(
                        pckReadStrP(param), pckReadStrP(param), pckReadU32P(param), pckReadU64P(param), pckReadStrP(param),
                        (CipherType)pckReadU32P(param), pckReadStrP(param), (CompressType)pckReadU32P(param), pckReadI32P(param))));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
