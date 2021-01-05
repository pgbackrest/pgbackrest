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

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_RESTORE_FILE_STR,                    PROTOCOL_COMMAND_RESTORE_FILE);

/**********************************************************************************************************************************/
bool
restoreProtocol(const String *command, PackRead *param, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_RESTORE_FILE_STR))
        {
            ASSERT(param != NULL);

            const String *repoFile = pckReadStrP(param);
            const String *repoFileReference = pckReadStrP(param);
            CompressType repoFileCompressType = (CompressType)pckReadU32P(param);
            const String *pgFile = pckReadStrP(param);
            const String *pgFileChecksum = pckReadStrP(param);
            bool pgFileZero = pckReadBoolP(param);
            uint64_t pgFileSize = pckReadU64P(param);
            time_t pgFileModified = pckReadTimeP(param);
            mode_t pgFileMode = pckReadU32P(param);
            const String *pgFileUser = pckReadStrP(param);
            const String *pgFileGroup = pckReadStrP(param);
            time_t copyTimeBegin = pckReadTimeP(param);
            bool delta = pckReadBoolP(param);
            bool deltaForce = pckReadBoolP(param);
            const String *cipherPass = pckReadStrP(param);

            protocolServerResponseVar(
                server,
                VARBOOL(
                    restoreFile(
                        repoFile, repoFileReference, repoFileCompressType, pgFile, pgFileChecksum, pgFileZero, pgFileSize,
                        pgFileModified, pgFileMode, pgFileUser, pgFileGroup, copyTimeBegin, delta, deltaForce, cipherPass)));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
