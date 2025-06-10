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
#include "storage/write.h"

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerResult *
archiveGetFileProtocol(PackRead *const param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);

    ProtocolServerResult *const result = protocolServerResultNewP();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get request
        const String *const request = pckReadStrP(param);

        // Build the actual list
        List *const actualList = lstNewP(sizeof(ArchiveGetFile));

        while (!pckReadNullP(param))
        {
            ArchiveGetFile actual = {.file = pckReadStrP(param)};
            actual.repoIdx = pckReadU32P(param);
            actual.archiveId = pckReadStrP(param);
            actual.cipherType = pckReadU64P(param);
            actual.cipherPassArchive = pckReadStrP(param);

            lstAdd(actualList, &actual);
        }

        // Get file
        const ArchiveGetFileResult fileResult = archiveGetFile(
            storageSpoolWrite(), request, actualList,
            strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s." STORAGE_FILE_TEMP_EXT, strZ(request)));

        // Return result
        PackWrite *const data = protocolServerResultData(result);
        pckWriteU32P(data, fileResult.actualIdx);
        pckWriteStrLstP(data, fileResult.warnList);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER_RESULT, result);
}
