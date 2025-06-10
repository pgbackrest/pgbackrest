/***********************************************************************************************************************************
GCS Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/gcs/helper.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN Storage *
storageGcsHelper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_GCS_TYPE);

    Storage *const result = storageGcsNew(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write, storageRepoTargetTime(), pathExpressionCallback,
        cfgOptionIdxStr(cfgOptRepoGcsBucket, repoIdx), (StorageGcsKeyType)cfgOptionIdxStrId(cfgOptRepoGcsKeyType, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoGcsKey, repoIdx), (size_t)cfgOptionIdxUInt64(cfgOptRepoStorageUploadChunkSize, repoIdx),
        cfgOptionIdxKvNull(cfgOptRepoStorageTag, repoIdx), cfgOptionIdxStr(cfgOptRepoGcsEndpoint, repoIdx), ioTimeoutMs(),
        cfgOptionIdxBool(cfgOptRepoStorageVerifyTls, repoIdx), cfgOptionIdxStrNull(cfgOptRepoStorageCaFile, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoStorageCaPath, repoIdx), cfgOptionIdxStrNull(cfgOptRepoGcsUserProject, repoIdx));

    FUNCTION_LOG_RETURN(STORAGE, result);
}
