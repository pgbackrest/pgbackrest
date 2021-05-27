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
        // !!! BROKEN
        // const String *repoFile = pckReadStrP(param);
        // const String *repoFileReference = pckReadStrP(param);
        // CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
        // const String *pgFile = pckReadStrP(param);
        // const String *pgFileChecksum = pckReadStrP(param);
        // bool pgFileZero = pckReadBoolP(param);
        // uint64_t pgFileSize = pckReadU64P(param);
        // time_t pgFileModified = pckReadTimeP(param);
        // mode_t pgFileMode = pckReadU32P(param);
        // const String *pgFileUser = pckReadStrP(param);
        // const String *pgFileGroup = pckReadStrP(param);
        // time_t copyTimeBegin = pckReadTimeP(param);
        // bool delta = pckReadBoolP(param);
        // bool deltaForce = pckReadBoolP(param);
        // const String *cipherPass = pckReadStrP(param);
        //
        // protocolServerResponseVar(
        //     server,
        //     VARBOOL(
        //         restoreFile(
        //             repoFile, repoFileReference, repoFileCompressType, pgFile, pgFileChecksum, pgFileZero, pgFileSize,
        //             pgFileModified, pgFileMode, pgFileUser, pgFileGroup, copyTimeBegin, delta, deltaForce, cipherPass)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
