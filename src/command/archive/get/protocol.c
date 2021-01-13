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

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_ARCHIVE_GET_STR,                     PROTOCOL_COMMAND_ARCHIVE_GET);

/**********************************************************************************************************************************/
bool
archiveGetProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_ARCHIVE_GET_STR))
        {
            const String *walSegment = varStr(varLstGet(paramList, 0));

            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepo();

            protocolServerResponse(
                server,
                VARINT(
                    archiveGetFile(
                        storageSpoolWrite(), walSegment, strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment)), true,
                        cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStrNull(cfgOptRepoCipherPass))));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
