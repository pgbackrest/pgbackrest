/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_H
#define STORAGE_AZURE_STORAGE_H

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
    storageAzureKeyTypeShared = STRID5("shared", 0x85905130),
    storageAzureKeyTypeSas = STRID5("sas", 0x4c330),
} StorageAzureKeyType;

/***********************************************************************************************************************************
URI style
***********************************************************************************************************************************/
typedef enum
{
    storageAzureUriStyleHost = STRID5("host", 0xa4de80),
    storageAzureUriStylePath = STRID5("path", 0x450300),
} StorageAzureUriStyle;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageAzureNew(
    const String *path, bool write, time_t targetTime, StoragePathExpressionCallback pathExpressionFunction,
    const String *container, const String *account, StorageAzureKeyType keyType, const String *key, size_t blockSize,
    const KeyValue *tag, const String *endpoint, StorageAzureUriStyle uriStyle, unsigned int port, TimeMSec timeout,
    bool verifyPeer, const String *caFile, const String *caPath);

#endif
