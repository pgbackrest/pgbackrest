/***********************************************************************************************************************************
Restore Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/file.h"
#include "command/restore/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_RESTORE_FILE_STR,                    PROTOCOL_COMMAND_RESTORE_FILE);

/**********************************************************************************************************************************/
bool
restoreProtocol(const String *command, PackRead *param, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);
    ASSERT(param != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_RESTORE_FILE_STR))
        {
            protocolServerResponse(
                server,
                VARBOOL(
                    restoreFile(
                        pckReadStrP(param), pckReadStrP(param), (CompressType)pckReadU32P(param), pckReadStrP(param),
                        pckReadStrP(param), pckReadBoolP(param), pckReadU64P(param), pckReadTimeP(param), pckReadU32P(param),
                        pckReadStrP(param), pckReadStrP(param), pckReadTimeP(param), pckReadBoolP(param), pckReadBoolP(param),
                        pckReadStrP(param))));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
