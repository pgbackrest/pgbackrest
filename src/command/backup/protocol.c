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

/**********************************************************************************************************************************/
void
backupFileProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Backup the file
        BackupFileResult result = backupFile(
            varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)), varUInt64(varLstGet(paramList, 2)),
            varBool(varLstGet(paramList, 3)), varStr(varLstGet(paramList, 4)), varBool(varLstGet(paramList, 5)),
            varUInt64(varLstGet(paramList, 6)), varStr(varLstGet(paramList, 7)), varBool(varLstGet(paramList, 8)),
            (CompressType)varUIntForce(varLstGet(paramList, 9)), varIntForce(varLstGet(paramList, 10)),
            varStr(varLstGet(paramList, 11)), varBool(varLstGet(paramList, 12)), varUInt64(varLstGet(paramList, 13)),
            varStr(varLstGet(paramList, 14)));

        // Return backup result
        VariantList *resultList = varLstNew();
        varLstAdd(resultList, varNewUInt(result.backupCopyResult));
        varLstAdd(resultList, varNewUInt64(result.copySize));
        varLstAdd(resultList, varNewUInt64(result.repoSize));
        varLstAdd(resultList, varNewStr(result.copyChecksum));
        varLstAdd(resultList, result.pageChecksumResult != NULL ? varNewKv(result.pageChecksumResult) : NULL);

        protocolServerResponse(server, varNewVarLst(resultList));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
