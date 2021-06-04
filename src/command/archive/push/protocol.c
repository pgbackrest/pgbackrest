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
        // Read parameters
        const String *const walSource = pckReadStrP(param);
        const bool headerCheck = pckReadBoolP(param);
        const unsigned int pgVersion = pckReadU32P(param);
        const uint64_t pgSystemId = pckReadU64P(param);
        const String *const archiveFile = pckReadStrP(param);
        const CompressType compressType = pckReadU32P(param);
        const int compressLevel = pckReadI32P(param);
        const StringList *const priorErrorList = pckReadStrLstP(param);

        // Read repo data
        List *repoList = lstNewP(sizeof(ArchivePushFileRepoData));

        pckReadArrayBeginP(param);

        while (!pckReadNullP(param))
        {
            pckReadObjBeginP(param);

            ArchivePushFileRepoData repo = {.repoIdx = pckReadU32P(param)};
            repo.archiveId = pckReadStrP(param);
            repo.cipherType = pckReadU64P(param);
            repo.cipherPass = pckReadStrP(param);
            pckReadObjEndP(param);

            lstAdd(repoList, &repo);
        }

        pckReadArrayEndP(param);

        // Push file
        const ArchivePushFileResult fileResult = archivePushFile(
            walSource, headerCheck, pgVersion, pgSystemId, archiveFile, compressType, compressLevel, repoList, priorErrorList);

        // Return result
        VariantList *result = varLstNew();
        varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));

        protocolServerResult(server, pckWriteStrP(protocolPack(), jsonFromVar(varNewVarLst(result))));
        protocolServerResponse(server);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
