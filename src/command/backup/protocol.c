/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/file.h"
#include "command/backup/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_BACKUP_FILE_STR,                     PROTOCOL_COMMAND_BACKUP_FILE);

/**********************************************************************************************************************************/
bool
backupProtocol(const String *command, PackRead *param, ProtocolServer *server)
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
        if (strEq(command, PROTOCOL_COMMAND_BACKUP_FILE_STR))
        {
            // Backup the file
            BackupFileResult result = backupFile(
                pckReadStrP(param), pckReadBoolP(param), pckReadU64P(param), pckReadBoolP(param), pckReadStrP(param),
                pckReadBoolP(param), pckReadU64P(param), pckReadStrP(param), pckReadBoolP(param), (CompressType)pckReadU32P(param),
                pckReadI32P(param), pckReadStrP(param), pckReadBoolP(param), (CipherType)pckReadU32P(param), pckReadStrP(param));

            // Return backup result
            VariantList *resultList = varLstNew();
            varLstAdd(resultList, varNewUInt(result.backupCopyResult));
            varLstAdd(resultList, varNewUInt64(result.copySize));
            varLstAdd(resultList, varNewUInt64(result.repoSize));
            varLstAdd(resultList, varNewStr(result.copyChecksum));
            varLstAdd(resultList, result.pageChecksumResult != NULL ? varNewKv(result.pageChecksumResult) : NULL);

            protocolServerResponse(server, varNewVarLst(resultList));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
