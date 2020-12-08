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
verifyProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
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
            VerifyResult result = verifyFile(
                varStr(varLstGet(paramList, 0)),                                                    // Full filename
                varStr(varLstGet(paramList, 1)),                                                    // Checksum
                varUInt64(varLstGet(paramList, 2)),                                                 // File size
                varStr(varLstGet(paramList, 3)));                                                   // Cipher pass

            protocolServerResponse(server, varNewInt(result));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
