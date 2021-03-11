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
        const unsigned int paramFixed = 6;                          // Fixed params before the repo param array
        const unsigned int paramRepo = 3;                           // Parameters in each index of the repo array

        // Check that the correct number of repo parameters were passed
        CHECK(varLstSize(paramList) - paramFixed == cfgOptionGroupIdxTotal(cfgOptGrpRepo) * paramRepo);

        // Build the repo data array
        ArchivePushFileRepoData *repoData = memNew(cfgOptionGroupIdxTotal(cfgOptGrpRepo) * sizeof(ArchivePushFileRepoData));

        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            repoData[repoIdx].archiveId = varStr(varLstGet(paramList, paramFixed + (repoIdx * paramRepo)));
            repoData[repoIdx].cipherType = (CipherType)varUIntForce(
                varLstGet(paramList, paramFixed + (repoIdx * paramRepo) + 1));
            repoData[repoIdx].cipherPass = varStr(varLstGet(paramList, paramFixed + (repoIdx * paramRepo) + 2));
        }

        // Push the file
        ArchivePushFileResult fileResult = archivePushFile(
            varStr(varLstGet(paramList, 0)), varUIntForce(varLstGet(paramList, 1)), varUInt64(varLstGet(paramList, 2)),
            varStr(varLstGet(paramList, 3)), (CompressType)varUIntForce(varLstGet(paramList, 4)),
            varIntForce(varLstGet(paramList, 5)), repoData);

        // Return result
        VariantList *result = varLstNew();
        varLstAdd(result, varNewVarLst(varLstNewStrLst(fileResult.warnList)));

        protocolServerResponse(server, varNewVarLst(result));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
