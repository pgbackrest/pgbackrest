/***********************************************************************************************************************************
SFTP Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/sftp/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN Storage *
storageSftpHelper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_SFTP_TYPE);

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageSftpNewP(
            cfgOptionIdxStr(cfgOptRepoPath, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHost, repoIdx),
            cfgOptionIdxUInt(cfgOptRepoSftpHostPort, repoIdx), cfgOptionIdxStr(cfgOptRepoSftpHostUser, repoIdx),
            cfgOptionUInt64(cfgOptIoTimeout), cfgOptionIdxStr(cfgOptRepoSftpPrivateKeyFile, repoIdx),
            cfgOptionIdxStrId(cfgOptRepoSftpHostKeyHashType, repoIdx), .write = write,
            .pathExpressionFunction = pathExpressionCallback, .modeFile = STORAGE_MODE_FILE_DEFAULT,
            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = cfgOptionIdxStrNull(cfgOptRepoSftpPublicKeyFile, repoIdx),
            .keyPassphrase = cfgOptionIdxStrNull(cfgOptRepoSftpPrivateKeyPassphrase, repoIdx),
            .hostFingerprint = cfgOptionIdxStrNull(cfgOptRepoSftpHostFingerprint, repoIdx)));
}

#endif // HAVE_LIBSSH2
