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
#include "common/type/json.h"
#include "config/config.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
void
backupFileProtocol(PackRead *const param, ProtocolServer *const server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Backup options that apply to all files
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const int repoFileCompressLevel = pckReadI32P(param);
        const String *const backupLabel = pckReadStrP(param);
        const bool delta = pckReadBoolP(param);
        const CipherType cipherType = (CipherType)pckReadU64P(param);
        const String *const cipherPass = pckReadStrP(param);

        // Build the file list
        List *fileList = lstNewP(sizeof(BackupFile));

        while (!pckReadNullP(param))
        {
            BackupFile file = {.pgFile = pckReadStrP(param)};
            file.pgFileIgnoreMissing = pckReadBoolP(param);
            file.pgFileSize = pckReadU64P(param);
            file.pgFileCopyExactSize = pckReadBoolP(param);
            file.pgFileChecksum = pckReadStrP(param);
            file.pgFileChecksumPage = pckReadBoolP(param);
            file.pgFileChecksumPageLsnLimit = pckReadU64P(param);
            file.repoFile = pckReadStrP(param);
            file.repoFileHasReference = pckReadBoolP(param);

            lstAdd(fileList, &file);
        }

        // Backup file
        const BackupFileResult result = backupFile(
            repoFileCompressType, repoFileCompressLevel, backupLabel, delta, cipherType, cipherPass, fileList);

        // Return result
        PackWrite *const resultPack = protocolPackNew();
        pckWriteU32P(resultPack, result.backupCopyResult);
        pckWriteU64P(resultPack, result.copySize);
        pckWriteU64P(resultPack, result.repoSize);
        pckWriteStrP(resultPack, result.copyChecksum);
        pckWritePackP(resultPack, result.pageChecksumResult);

        protocolServerDataPut(server, resultPack);
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
