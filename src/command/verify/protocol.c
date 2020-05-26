/***********************************************************************************************************************************
Restore Protocol Handler
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

    // Get the repo storage in case it is remote and encryption settings need to be pulled down
    storageRepo();

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Process any commands received that are for this handler
        if (strEq(command, PROTOCOL_COMMAND_VERIFY_FILE_STR))
        {
            protocolServerResponse(
                server,
                VARBOOL(
                    verifyFile(varStr(varLstGet(paramList, 0)))));
                        // CSHANG Not sure yet what this will look like
                        // varStr(varLstGet(paramList, 0)), varStr(varLstGet(paramList, 1)),
                        // (CompressType)varUIntForce(varLstGet(paramList, 2)), varStr(varLstGet(paramList, 3)),
                        // varStr(varLstGet(paramList, 4)), varBoolForce(varLstGet(paramList, 5)), varUInt64(varLstGet(paramList, 6)),
                        // (time_t)varInt64Force(varLstGet(paramList, 7)), cvtZToUIntBase(strPtr(varStr(varLstGet(paramList, 8))), 8),
                        // varStr(varLstGet(paramList, 9)), varStr(varLstGet(paramList, 10)),
                        // (time_t)varInt64Force(varLstGet(paramList, 11)), varBoolForce(varLstGet(paramList, 12)),
                        // varBoolForce(varLstGet(paramList, 13)), varStr(varLstGet(paramList, 14)))));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
