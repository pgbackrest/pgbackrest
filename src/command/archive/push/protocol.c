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

/**********************************************************************************************************************************/
void
archivePushFileProtocol(PackRead *const param, ProtocolServer *const server)
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
        // // Build the repo data list
        // List *repoList = lstNewP(sizeof(ArchivePushFileRepoData));
        // unsigned int repoListSize = varUIntForce(varLstGet(paramList, 8));
        // unsigned int paramIdx = 9;
        //
        // for (unsigned int repoListIdx = 0; repoListIdx < repoListSize; repoListIdx++)
        // {
        //     const String *walSource = pckReadStrP(param);
        //     const String *archiveId = pckReadStrP(param);
        //     unsigned int pgVersion = pckReadU32P(param);
        //     uint64_t pgSystemId = pckReadU64P(param);
        //     const String *archiveFile = pckReadStrP(param);
        //     CipherType cipherType = (CipherType)pckReadU32P(param);
        //     const String *cipherPass = pckReadStrP(param);
        //     CompressType compressType = (CompressType)pckReadU32P(param);
        //     int compressLevel = pckReadI32P(param);
        //
        //     protocolServerResponseVar(
        //         server,
        //         VARSTR(
        //             archivePushFile(
        //                 walSource, archiveId, pgVersion, pgSystemId, archiveFile, cipherType, cipherPass, compressType,
        //                 compressLevel)));
        // }
        //
        // // Push the file
        // ArchivePushFileResult fileResult = archivePushFile(
        //     varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)), varUIntForce(varLstGet(paramList, 2)),
        //     varUInt64(varLstGet(paramList, 3)), varStr(varLstGet(paramList, 4)),
        //     (CompressType)varUIntForce(varLstGet(paramList, 5)), varIntForce(varLstGet(paramList, 6)), repoList,
        //     strLstNewVarLst(varVarLst(varLstGet(paramList, 7))));
        //
        // // Return result
        // VariantList *result = varLstNew();
        // varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));
        //
        // protocolServerResponse(server, varNewVarLst(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
