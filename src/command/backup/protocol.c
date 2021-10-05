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
        // Backup file
        const String *const pgFile = pckReadStrP(param);
        const bool pgFileIgnoreMissing = pckReadBoolP(param);
        const uint64_t pgFileSize = pckReadU64P(param);
        const bool pgFileCopyExactSize = pckReadBoolP(param);
        const String *const pgFileChecksum = pckReadStrP(param);
        const bool pgFileChecksumPage = pckReadBoolP(param);
        const uint64_t pgFileChecksumPageLsnLimit = pckReadU64P(param);
        const String *const repoFile = pckReadStrP(param);
        const bool repoFileHasReference = pckReadBoolP(param);
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const int repoFileCompressLevel = pckReadI32P(param);
        const String *const backupLabel = pckReadStrP(param);
        const bool delta = pckReadBoolP(param);
        const CipherType cipherType = (CipherType)pckReadU64P(param);
        const String *const cipherPass = pckReadStrP(param);

        const BackupFileResult result = backupFile(
            pgFile, pgFileIgnoreMissing, pgFileSize, pgFileCopyExactSize, pgFileChecksum, pgFileChecksumPage,
            pgFileChecksumPageLsnLimit, repoFile, repoFileHasReference, repoFileCompressType, repoFileCompressLevel,
            backupLabel, delta, cipherType, cipherPass);

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
