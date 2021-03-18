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
STRING_EXTERN(PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE_STR,               PROTOCOL_COMMAND_ARCHIVE_PUSH_FILE);

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
        unsigned int repoListSize = varUIntForce(varLstGet(paramList, 6));
        unsigned int paramIdx = 7;

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
            varStr(varLstGet(paramList, 0)), varUIntForce(varLstGet(paramList, 1)), varUInt64(varLstGet(paramList, 2)),
            varStr(varLstGet(paramList, 3)), (CompressType)varUIntForce(varLstGet(paramList, 4)),
            varIntForce(varLstGet(paramList, 5)), repoList);

        // Return result
        VariantList *result = varLstNew();
        varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));

        protocolServerResponse(server, varNewVarLst(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
