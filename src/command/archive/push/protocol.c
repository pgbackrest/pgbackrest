/***********************************************************************************************************************************
Archive Push Protocol Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/push/file.h"
#include "command/archive/push/protocol.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_ARCHIVE_PUSH_STR,                     PROTOCOL_COMMAND_ARCHIVE_PUSH);

/**********************************************************************************************************************************/
bool
archivePushProtocol(const String *command, PackRead *param, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(PACK_READ, param);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);
    ASSERT(param != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_ARCHIVE_PUSH_STR))
        {
            const String *walSource = pckReadStrP(param);
            const String *archiveId = pckReadStrP(param);
            unsigned int pgVersion = pckReadU32P(param);
            uint64_t pgSystemId = pckReadU64P(param);
            const String *archiveFile = pckReadStrP(param);
            CipherType cipherType = (CipherType)pckReadU32P(param);
            const String *cipherPass = pckReadStrP(param);
            CompressType compressType = (CompressType)pckReadU32P(param);
            int compressLevel = pckReadI32P(param);

            protocolServerResponse(
                server,
                VARSTR(
                    archivePushFile(
                        walSource, archiveId, pgVersion, pgSystemId, archiveFile, cipherType, cipherPass, compressType,
                        compressLevel)));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
