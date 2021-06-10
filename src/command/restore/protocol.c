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
void
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
        const String *const repoFileReference = pckReadStrP(param);
        const CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        const String *const pgFile = pckReadStrP(param);
        const String *const pgFileChecksum = pckReadStrP(param);
        const bool pgFileZero = pckReadBoolP(param);
        const uint64_t pgFileSize = pckReadU64P(param);
        const time_t pgFileModified = pckReadTimeP(param);
        const mode_t pgFileMode = pckReadModeP(param);
        const String *const pgFileUser = pckReadStrP(param);
        const String *const pgFileGroup = pckReadStrP(param);
        const time_t copyTimeBegin = pckReadTimeP(param);
        const bool delta = pckReadBoolP(param);
        const bool deltaForce = pckReadBoolP(param);
        const String *const cipherPass = pckReadStrP(param);

        const bool result = restoreFile(
            repoFile, repoIdx, repoFileReference, repoFileCompressType, pgFile, pgFileChecksum, pgFileZero, pgFileSize,
            pgFileModified, pgFileMode, pgFileUser, pgFileGroup, copyTimeBegin, delta, deltaForce, cipherPass);

        // Result result
        protocolServerDataPut(server, pckWriteBoolP(protocolPackNew(), result));
        protocolServerDataEndPut(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
