/***********************************************************************************************************************************
Archive Get Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/get/file.h"
#include "command/archive/get/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"
#include "storage/write.intern.h"

/**********************************************************************************************************************************/
void
archiveGetFileProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get request
        const String *const request = pckReadStrP(param);

        // Build the actual list
        List *actualList = lstNewP(sizeof(ArchiveGetFile));

        while (!pckReadNullP(param))
        {
            ArchiveGetFile actual = {.file = pckReadStrP(param)};
            actual.repoIdx = pckReadU32P(param);
            actual.archiveId = pckReadStrP(param);
            actual.cipherType = pckReadU64P(param);
            actual.cipherPassArchive = pckReadStrP(param);

            lstAdd(actualList, &actual);
        }

        // Return result
        ArchiveGetFileResult fileResult = archiveGetFile(
            storageSpoolWrite(), request, actualList,
            strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s." STORAGE_FILE_TEMP_EXT, strZ(request)));

        VariantList *result = varLstNew();
        varLstAdd(result, varNewUInt(fileResult.actualIdx));
        varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));

        protocolServerResult(server, pckWriteStrP(protocolPack(), jsonFromVar(varNewVarLst(result))));
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
