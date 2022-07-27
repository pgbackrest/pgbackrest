/***********************************************************************************************************************************
Sftp Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/url.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/sftp/helper.h"
#include "storage/sftp/storage.h"

/**********************************************************************************************************************************/
Storage *
storageSftpHelper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_SFTP_TYPE);

    Storage *result = NULL;

    /*
    StorageSftpNewParam param;
    param.write = write;
    param.pathExpressionFunction = pathExpressionCallback;

    result = storageSftpNew(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptSftpHost, repoIdx),
        cfgOptionIdxInt(cfgOptSftpPort, repoIdx), cfgOptionUInt64(cfgOptIoTimeout), cfgOptionUInt64(cfgOptIoTimeout), param);
        */

    FUNCTION_LOG_RETURN(STORAGE, result);
}
