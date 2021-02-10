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
restoreProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
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
        if (strEq(command, PROTOCOL_COMMAND_RESTORE_FILE_STR))
        {
// CSHANG remove
if (varLstGet(paramList, 0) == NULL) THROW(RuntimeError, "repoFile is NULL");
if (varLstGet(paramList, 2) == NULL) THROW(RuntimeError, "repoFileReference is NULL");
if (varLstGet(paramList, 4) == NULL) THROW(RuntimeError, "pgFile is NULL");
if (varLstGet(paramList, 5) == NULL) THROW(RuntimeError, "pgFileChecksum is NULL");
if (varLstGet(paramList, 10) == NULL) THROW(RuntimeError, "pgFileUser is NULL");
if (varLstGet(paramList, 11) == NULL) THROW(RuntimeError, "pgFileGroup is NULL");
if (varLstGet(paramList, 15) == NULL) THROW(RuntimeError, "cipherSubPass is NULL");

            protocolServerResponse(
                server,
                VARBOOL(
                    restoreFile(
                        varStr(varLstGet(paramList, 0)), varUInt(varLstGet(paramList, 1)), varStr(varLstGet(paramList, 2)),
                        (CompressType)varUIntForce(varLstGet(paramList, 3)), varStr(varLstGet(paramList, 4)),
                        varStr(varLstGet(paramList, 5)), varBoolForce(varLstGet(paramList, 6)), varUInt64(varLstGet(paramList, 7)),
                        (time_t)varInt64Force(varLstGet(paramList, 8)),
                        (mode_t)cvtZToUIntBase(strZ(varStr(varLstGet(paramList, 9))), 8),
                        varStr(varLstGet(paramList, 10)), varStr(varLstGet(paramList, 11)),
                        (time_t)varInt64Force(varLstGet(paramList, 12)), varBoolForce(varLstGet(paramList, 13)),
                        varBoolForce(varLstGet(paramList, 14)), varStr(varLstGet(paramList, 15)))));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
