/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_H
#define STORAGE_AZURE_STORAGE_H

#include "common/io/http/url.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_AZURE_TYPE                                          STRID5("azure", 0x5957410)

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageAzureKeyTypeShared,
    storageAzureKeyTypeSas,
    storageAzureKeyTypeAuto,
} StorageAzureKeyType;

/***********************************************************************************************************************************
URI style
***********************************************************************************************************************************/
typedef enum
{
    storageAzureUriStyleHost,
    storageAzureUriStylePath,
} StorageAzureUriStyle;

/***********************************************************************************************************************************
Immutability policy mode
***********************************************************************************************************************************/
typedef enum
{
    storageAzureImmutabilityPolicyModeUnlocked,                     // Retention can be shortened or removed
    storageAzureImmutabilityPolicyModeLocked,                       // Retention cannot be shortened or removed
} StorageAzureImmutabilityPolicyMode;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageAzureNew(
    const String *path, bool write, time_t targetTime, StoragePathExpressionCallback pathExpressionFunction,
    const String *container, const String *account, StorageAzureKeyType keyType, const String *key, size_t blockSize,
    const KeyValue *tag, const String *endpoint, StorageAzureUriStyle uriStyle, unsigned int port, TimeMSec timeout,
    HttpProtocolType protocolType, bool verifyPeer, const String *caFile, const String *caPath, TimeMSec tmpRetention,
    StorageAzureImmutabilityPolicyMode tmpRetentionMode, unsigned int prefetch, uint64_t readOver);

#endif
