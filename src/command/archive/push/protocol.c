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
archivePushProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Get the repo storage in case it is remote and encryption settings need to be pulled down
    storageRepo();

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_ARCHIVE_PUSH_STR))
        {
            CHECK(varLstSize(paramList) - 6 == cfgOptionGroupIdxTotal(cfgOptGrpRepo) * 3);

            ArchivePushFileRepoData *repoData = memNew(cfgOptionGroupIdxTotal(cfgOptGrpRepo) * sizeof(ArchivePushFileRepoData));

            for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
            {
                repoData[repoIdx].archiveId = varStr(varLstGet(paramList, 6 + (repoIdx * 3)));
                repoData[repoIdx].cipherType = (CipherType)varUIntForce(varLstGet(paramList, 6 + (repoIdx * 3) + 1));
                repoData[repoIdx].cipherPass = varStr(varLstGet(paramList, 6 + (repoIdx * 3) + 2));
            }

            protocolServerResponse(
                server,
                VARSTR(
                    archivePushFile(
                        varStr(varLstGet(paramList, 0)), varUIntForce(varLstGet(paramList, 1)), varUInt64(varLstGet(paramList, 2)),
                        varStr(varLstGet(paramList, 3)), (CompressType)varUIntForce(varLstGet(paramList, 4)),
                        varIntForce(varLstGet(paramList, 5)), repoData)));
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
