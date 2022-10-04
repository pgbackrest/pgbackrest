/***********************************************************************************************************************************
Sftp Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageSftpNewParam param;
        param.write = write;
        param.pathExpressionFunction = pathExpressionCallback;
        param.user = strDup(cfgOptionStrNull(cfgOptRepoSftpAccount));
        param.password = strDup(cfgOptionStrNull(cfgOptRepoSftpPassword));
        param.keyPub = strDup(cfgOptionStrNull(cfgOptRepoSftpPublicKeyfile));
        param.keyPriv = strDup(cfgOptionStrNull(cfgOptRepoSftpPrivateKeyfile));
        param.keyPassphrase = strDup(cfgOptionStrNull(cfgOptRepoSftpKeyfilePassphrase));
        param.modeFile = STORAGE_MODE_FILE_DEFAULT;
        param.modePath = STORAGE_MODE_PATH_DEFAULT;

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = storageSftpNew(
                        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
                        cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionUInt64(cfgOptIoTimeout),
                        cfgOptionUInt64(cfgOptIoTimeout), param);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE, result);
}

#endif // HAVE_LIBSSH2