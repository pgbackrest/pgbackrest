/***********************************************************************************************************************************
S3 Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/s3/helper.h"

/**********************************************************************************************************************************/
Storage *
storageS3Helper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_S3_TYPE);

    // Set the default port
    unsigned int port = cfgOptionIdxUInt(cfgOptRepoStoragePort, repoIdx);

    // Extract port from the endpoint and host if it is present
    const String *const endPoint = cfgOptionIdxHostPort(cfgOptRepoS3Endpoint, repoIdx, &port);
    const String *const host = cfgOptionIdxHostPort(cfgOptRepoStorageHost, repoIdx, &port);

    // If the port option was set explicitly then use it in preference to appended ports
    if (cfgOptionIdxSource(cfgOptRepoStoragePort, repoIdx) != cfgSourceDefault)
        port = cfgOptionIdxUInt(cfgOptRepoStoragePort, repoIdx);

    // Get role and token
    const StorageS3KeyType keyType = (StorageS3KeyType)cfgOptionIdxStrId(cfgOptRepoS3KeyType, repoIdx);
    const String *role = cfgOptionIdxStrNull(cfgOptRepoS3Role, repoIdx);
    const String *webIdToken = NULL;

    // If service authentication then load the role and token from environment variables documented here:
    // https://docs.aws.amazon.com/eks/latest/userguide/iam-roles-for-service-accounts-technical-overview.html
    if (keyType == storageS3KeyTypeService)
    {
        #define S3_ENV_AWS_ROLE_ARN                             "AWS_ROLE_ARN"
        #define S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE              "AWS_WEB_IDENTITY_TOKEN_FILE"

        const char *const roleZ = getenv(S3_ENV_AWS_ROLE_ARN);
        const char *const webIdTokenFileZ = getenv(S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE);

        if (roleZ == NULL || webIdTokenFileZ == NULL)
        {
            THROW_FMT(
                OptionInvalidError,
                "option '%s' is '" CFGOPTVAL_REPO_S3_KEY_TYPE_SERVICE_Z "' but '" S3_ENV_AWS_ROLE_ARN "' and '"
                    S3_ENV_AWS_WEB_IDENTITY_TOKEN_FILE "' are not set",
                cfgOptionIdxName(cfgOptRepoS3KeyType, repoIdx));
        }

        role = strNewZ(roleZ);
        webIdToken = strNewBuf(storageGetP(storageNewReadP(storagePosixNewP(FSLASH_STR), STR(webIdTokenFileZ))));
    }

    Storage *const result = storageS3New(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write, pathExpressionCallback,
        cfgOptionIdxStr(cfgOptRepoS3Bucket, repoIdx), endPoint, (StorageS3UriStyle)cfgOptionIdxStrId(cfgOptRepoS3UriStyle, repoIdx),
        cfgOptionIdxStr(cfgOptRepoS3Region, repoIdx), keyType, cfgOptionIdxStrNull(cfgOptRepoS3Key, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoS3KeySecret, repoIdx), cfgOptionIdxStrNull(cfgOptRepoS3Token, repoIdx), role,
        webIdToken, STORAGE_S3_PARTSIZE_MIN, host, port, ioTimeoutMs(), cfgOptionIdxBool(cfgOptRepoStorageVerifyTls, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoStorageCaFile, repoIdx), cfgOptionIdxStrNull(cfgOptRepoStorageCaPath, repoIdx));

    FUNCTION_LOG_RETURN(STORAGE, result);
}
