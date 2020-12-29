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

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_VERIFY_FILE_STR,                    PROTOCOL_COMMAND_VERIFY_FILE);

/**********************************************************************************************************************************/
bool
verifyProtocol(const String *command, PackRead *param, ProtocolServer *server)
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
        // Process any commands received that are for this handler
        if (strEq(command, PROTOCOL_COMMAND_VERIFY_FILE_STR))
        {
            ASSERT(param != NULL);

            const String *filePathName = pckReadStrP(param);
            const String *fileChecksum = pckReadStrP(param);
            uint64_t fileSize = pckReadU64P(param);
            const String *cipherPass = pckReadStrP(param);

            VerifyResult result = verifyFile(filePathName, fileChecksum, fileSize, cipherPass);

            protocolServerResponse(server, varNewInt(result));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
