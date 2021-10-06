/***********************************************************************************************************************************
Azure Storage Helper
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "config/config.h"
#include "storage/azure/helper.h"

/**********************************************************************************************************************************/
Storage *
storageAzureHelper(const unsigned int repoIdx, const bool write, StoragePathExpressionCallback pathExpressionCallback)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM_P(VOID, pathExpressionCallback);
    FUNCTION_LOG_END();

    ASSERT(cfgOptionIdxStrId(cfgOptRepoType, repoIdx) == STORAGE_AZURE_TYPE);

    const String *endpoint = cfgOptionIdxStr(cfgOptRepoAzureEndpoint, repoIdx);
    const String *const host = cfgOptionIdxStrNull(cfgOptRepoStorageHost, repoIdx);
    StorageAzureUriStyle uriStyle = (StorageAzureUriStyle)cfgOptionIdxStrId(cfgOptRepoAzureUriStyle, repoIdx);

    // If the host is set then set it as the endpoint. The host option is used to set path-style URIs when working with Azurite.
    // This was ill-advised, so the uri-style option was added to allow the user to select the URI style used by the server.
    // Preserve the old behavior when uri-style is defaulted.
    if (host != NULL)
    {
        endpoint = host;

        if (cfgOptionIdxSource(cfgOptRepoAzureUriStyle, repoIdx) == cfgSourceDefault)
            uriStyle = storageAzureUriStylePath;
    }

    Storage *const result = storageAzureNew(
        cfgOptionIdxStr(cfgOptRepoPath, repoIdx), write, pathExpressionCallback,
        cfgOptionIdxStr(cfgOptRepoAzureContainer, repoIdx), cfgOptionIdxStr(cfgOptRepoAzureAccount, repoIdx),
        (StorageAzureKeyType)cfgOptionIdxStrId(cfgOptRepoAzureKeyType, repoIdx),
        cfgOptionIdxStr(cfgOptRepoAzureKey, repoIdx), STORAGE_AZURE_BLOCKSIZE_MIN, endpoint, uriStyle,
        cfgOptionIdxUInt(cfgOptRepoStoragePort, repoIdx), ioTimeoutMs(), cfgOptionIdxBool(cfgOptRepoStorageVerifyTls, repoIdx),
        cfgOptionIdxStrNull(cfgOptRepoStorageCaFile, repoIdx), cfgOptionIdxStrNull(cfgOptRepoStorageCaPath, repoIdx));

    FUNCTION_LOG_RETURN(STORAGE, result);
}
