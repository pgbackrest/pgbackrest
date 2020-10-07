/***********************************************************************************************************************************
Azure Storage
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_H
#define STORAGE_AZURE_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_AZURE_TYPE                                          "azure"
    STRING_DECLARE(STORAGE_AZURE_TYPE_STR);

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageAzureKeyTypeShared,
    storageAzureKeyTypeSas,
} StorageAzureKeyType;

#define STORAGE_AZURE_KEY_TYPE_SHARED                               "shared"
#define STORAGE_AZURE_KEY_TYPE_SAS                                  "sas"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_AZURE_BLOCKSIZE_MIN                                 ((size_t)4 * 1024 * 1024)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageAzureNew(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *container,
    const String *account, StorageAzureKeyType keyType, const String *key, size_t blockSize, const String *host,
    const String *endpoint, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

#endif
