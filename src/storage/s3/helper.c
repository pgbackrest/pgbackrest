/***********************************************************************************************************************************
S3 Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

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

    Storage *const result = storageS3New(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write, pathExpressionCallback, cfgOptionIdxStr(cfgOptRepoS3Bucket, repoIdx),
        endPoint, (StorageS3UriStyle)cfgOptionIdxStrId(cfgOptRepoS3UriStyle, repoIdx), cfgOptionIdxStr(cfgOptRepoS3Region, repoIdx),
        (StorageS3KeyType)cfgOptionIdxStrId(cfgOptRepoS3KeyType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoS3Key, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoS3KeySecret, repoIdx), cfgOptionIdxStrNull(cfgOptRepoS3Token, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoS3Role, repoIdx), STORAGE_S3_PARTSIZE_MIN, host, port, ioTimeoutMs(),
        cfgOptionIdxBool(cfgOptRepoStorageVerifyTls, repoIdx), cfgOptionIdxStrNull(cfgOptRepoStorageCaFile, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoStorageCaPath, repoIdx));

    FUNCTION_LOG_RETURN(STORAGE, result);
}
