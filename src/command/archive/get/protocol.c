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
#include "storage/write.intern.h"

/**********************************************************************************************************************************/
void
archiveGetFileProtocol(PackRead *const param, ProtocolServer *const server)
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
        // const String *request = varStr(varLstGet(paramList, 0));
        //
        // const unsigned int paramFixed = 1;                          // Fixed params before the actual list
        // const unsigned int paramActual = 5;                         // Parameters in each index of the actual list
        //
        // // Check that the correct number of list parameters were passed
        // CHECK((varLstSize(paramList) - paramFixed) % paramActual == 0);
        //
        // // Build the actual list
        // List *actualList = lstNewP(sizeof(ArchiveGetFile));
        // unsigned int actualListSize = (varLstSize(paramList) - paramFixed) / paramActual;
        //
        // for (unsigned int actualIdx = 0; actualIdx < actualListSize; actualIdx++)
        // {
        //     ASSERT(param != NULL);
        //
        //     const String *walSegment = pckReadStrP(param);
        //
        //     protocolServerResponseVar(
        //         server,
        //         VARINT(
        //             archiveGetFile(
        //                 storageSpoolWrite(), walSegment, strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s", strZ(walSegment)), true,
        //                 cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStrNull(cfgOptRepoCipherPass))));
        // }
        //
        // // Return result
        // ArchiveGetFileResult fileResult = archiveGetFile(
        //     storageSpoolWrite(), request, actualList,
        //     strNewFmt(STORAGE_SPOOL_ARCHIVE_IN "/%s." STORAGE_FILE_TEMP_EXT, strZ(request)));
        //
        // VariantList *result = varLstNew();
        // varLstAdd(result, varNewUInt(fileResult.actualIdx));
        // varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));
        //
        // protocolServerResponse(server, varNewVarLst(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
