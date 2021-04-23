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
archivePushFileProtocol(const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(paramList != NULL);
    ASSERT(server != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the repo data list
        List *repoList = lstNewP(sizeof(ArchivePushFileRepoData));
        unsigned int repoListSize = varUIntForce(varLstGet(paramList, 8));
        unsigned int paramIdx = 9;

        for (unsigned int repoListIdx = 0; repoListIdx < repoListSize; repoListIdx++)
        {
            lstAdd(
                repoList,
                &(ArchivePushFileRepoData)
                {
                    .repoIdx = varUIntForce(varLstGet(paramList, paramIdx)),
                    .archiveId = varStr(varLstGet(paramList, paramIdx + 1)),
                    .cipherType = (CipherType)varUIntForce(varLstGet(paramList, paramIdx + 2)),
                    .cipherPass = varStr(varLstGet(paramList, paramIdx + 3)),
                });

            paramIdx += 4;
        }

        // Push the file
        ArchivePushFileResult fileResult = archivePushFile(
            varStr(varLstGet(paramList, 0)), varBool(varLstGet(paramList, 1)), varUIntForce(varLstGet(paramList, 2)),
            varUInt64(varLstGet(paramList, 3)), varStr(varLstGet(paramList, 4)),
            (CompressType)varUIntForce(varLstGet(paramList, 5)), varIntForce(varLstGet(paramList, 6)), repoList,
            strLstNewVarLst(varVarLst(varLstGet(paramList, 7))));

        // Return result
        VariantList *result = varLstNew();
        varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));

        protocolServerResponse(server, varNewVarLst(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
