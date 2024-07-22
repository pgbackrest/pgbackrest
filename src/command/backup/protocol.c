/***********************************************************************************************************************************
Backup Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/backup/file.h"
#include "command/backup/protocol.h"
#include "common/crypto/hash.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/json.h"
#include "config/config.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN ProtocolServerResult *
backupFileProtocol(PackRead *const param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, param);
    FUNCTION_LOG_END();

    ASSERT(param != NULL);

    ProtocolServerResult *const result = protocolServerResultNewP();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Backup options that apply to all files
        const String *const repoFile = pckReadStrP(param);
        const uint64_t bundleId = pckReadU64P(param);
        const bool bundleRaw = bundleId != 0 ? pckReadBoolP(param) : false;
        const unsigned int blockIncrReference = (unsigned int)pckReadU64P(param);
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const int repoFileCompressLevel = pckReadI32P(param);
        const CipherType cipherType = (CipherType)pckReadU64P(param);
        const String *const cipherPass = pckReadStrP(param);
        const PgPageSize pageSize = pckReadU32P(param);
        const String *const pgVersionForce = pckReadStrP(param);

        // Build the file list
        List *const fileList = lstNewP(sizeof(BackupFile));

        while (!pckReadNullP(param))
        {
            BackupFile file = {.pgFile = pckReadStrP(param)};
            file.pgFileDelta = pckReadBoolP(param);
            file.pgFileIgnoreMissing = pckReadBoolP(param);
            file.pgFileSize = pckReadU64P(param);
            file.pgFileSizeOriginal = pckReadU64P(param);
            file.pgFileCopyExactSize = pckReadBoolP(param);
            file.pgFileChecksum = pckReadBinP(param);
            file.pgFileChecksumPage = pckReadBoolP(param);
            file.pgFilePageHeaderCheck = pckReadBoolP(param);
            file.blockIncrSize = (size_t)pckReadU64P(param);

            if (file.blockIncrSize > 0)
            {
                file.blockIncrChecksumSize = (size_t)pckReadU64P(param);
                file.blockIncrSuperSize = pckReadU64P(param);
                file.blockIncrMapPriorFile = pckReadStrP(param);

                if (file.blockIncrMapPriorFile != NULL)
                {
                    file.blockIncrMapPriorOffset = pckReadU64P(param);
                    file.blockIncrMapPriorSize = pckReadU64P(param);
                }
            }

            file.manifestFile = pckReadStrP(param);
            file.repoFileChecksum = pckReadBinP(param);
            file.repoFileSize = pckReadU64P(param);
            file.manifestFileResume = pckReadBoolP(param);
            file.manifestFileHasReference = pckReadBoolP(param);

            lstAdd(fileList, &file);
        }

        // Backup file
        const List *const resultList = backupFile(
            repoFile, bundleId, bundleRaw, blockIncrReference, repoFileCompressType, repoFileCompressLevel, cipherType, cipherPass,
            pgVersionForce, pageSize, fileList);

        // Return result
        PackWrite *const data = protocolServerResultData(result);

        for (unsigned int resultIdx = 0; resultIdx < lstSize(resultList); resultIdx++)
        {
            const BackupFileResult *const fileResult = lstGet(resultList, resultIdx);

            ASSERT(
                fileResult->backupCopyResult == backupCopyResultSkip || fileResult->copySize != 0 ||
                bufEq(fileResult->copyChecksum, HASH_TYPE_SHA1_ZERO_BUF));

            pckWriteStrP(data, fileResult->manifestFile);
            pckWriteU32P(data, fileResult->backupCopyResult);
            pckWriteBoolP(data, fileResult->repoInvalid);
            pckWriteU64P(data, fileResult->copySize);
            pckWriteU64P(data, fileResult->bundleOffset);
            pckWriteU64P(data, fileResult->blockIncrMapSize);
            pckWriteU64P(data, fileResult->repoSize);
            pckWriteBinP(data, fileResult->copyChecksum);
            pckWriteBinP(data, fileResult->repoChecksum);
            pckWritePackP(data, fileResult->pageChecksumResult);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(PROTOCOL_SERVER_RESULT, result);
}
