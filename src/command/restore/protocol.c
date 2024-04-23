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

/**********************************************************************************************************************************/
FN_EXTERN void
restoreFileProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Restore file
        const String *const repoFile = pckReadStrP(param);
        const unsigned int repoIdx = pckReadU32P(param);
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const time_t copyTimeBegin = pckReadTimeP(param);
        const bool delta = pckReadBoolP(param);
        const bool deltaForce = pckReadBoolP(param);
        const bool bundleRaw = pckReadBoolP(param);
        const String *const cipherPass = pckReadStrP(param);
        const StringList *const referenceList = pckReadStrLstP(param);

        // Build the file list
        List *const fileList = lstNewP(sizeof(RestoreFile));

        while (!pckReadNullP(param))
        {
            RestoreFile file = {.name = pckReadStrP(param)};
            file.checksum = pckReadBinP(param);
            file.size = pckReadU64P(param);
            file.timeModified = pckReadTimeP(param);
            file.mode = pckReadModeP(param);
            file.zero = pckReadBoolP(param);
            file.user = pckReadStrP(param);
            file.group = pckReadStrP(param);

            if (pckReadBoolP(param))
            {
                file.offset = pckReadU64P(param);
                file.limit = varNewUInt64(pckReadU64P(param));
            }

            // Block incremental
            file.blockIncrMapSize = pckReadU64P(param);

            if (file.blockIncrMapSize != 0)
            {
                file.blockIncrSize = (size_t)pckReadU64P(param);
                file.blockIncrChecksumSize = (size_t)pckReadU64P(param);
            }

            file.manifestFile = pckReadStrP(param);

            lstAdd(fileList, &file);
        }

        // Restore files
        const List *const result = restoreFile(
            repoFile, repoIdx, repoFileCompressType, copyTimeBegin, delta, deltaForce, bundleRaw, cipherPass, referenceList,
            fileList);

        // Return result
        PackWrite *const resultPack = protocolPackNew();

        for (unsigned int resultIdx = 0; resultIdx < lstSize(result); resultIdx++)
        {
            const RestoreFileResult *const fileResult = lstGet(result, resultIdx);

            pckWriteStrP(resultPack, fileResult->manifestFile);
            pckWriteU32P(resultPack, fileResult->result);
            pckWriteU64P(resultPack, fileResult->blockIncrDeltaSize);
        }

        protocolServerDataPut(server, resultPack);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
