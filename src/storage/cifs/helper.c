/***********************************************************************************************************************************
CIFS Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/cifs/helper.h"

/**********************************************************************************************************************************/
Storage *
storageCifsHelper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_CIFS_TYPE);

    Storage *const result = storageCifsNew(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, write,
        pathExpressionCallback);

    FUNCTION_LOG_RETURN(STORAGE, result);
}
