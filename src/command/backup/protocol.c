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
        // Backup the file
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

        // Return backup result
        VariantList *resultList = varLstNew();
        varLstAdd(resultList, varNewUInt(result.backupCopyResult));
        varLstAdd(resultList, varNewUInt64(result.copySize));
        varLstAdd(resultList, varNewUInt64(result.repoSize));
        varLstAdd(resultList, varNewStr(result.copyChecksum));
        varLstAdd(resultList, result.pageChecksumResult != NULL ? varNewKv(result.pageChecksumResult) : NULL);

        protocolServerResult(server, pckWriteStrP(protocolPack(), jsonFromVar(varNewVarLst(resultList))));
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
