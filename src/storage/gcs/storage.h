/***********************************************************************************************************************************
GCS Storage
***********************************************************************************************************************************/
#ifndef STORAGE_GCS_STORAGE_H
#define STORAGE_GCS_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_GCS_TYPE                                            "gcs"
    STRING_DECLARE(STORAGE_GCS_TYPE_STR);

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageGcsKeyTypeShared,
    storageGcsKeyTypeSas,
} StorageGcsKeyType;

#define STORAGE_GCS_KEY_TYPE_SHARED                                 "shared"
#define STORAGE_GCS_KEY_TYPE_SAS                                    "sas"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_GCS_BLOCKSIZE_MIN                                   ((size_t)4 * 1024 * 1024)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageGcsNew(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *container,
    const String *account, StorageGcsKeyType keyType, const String *key, size_t blockSize, const String *host,
    const String *endpoint, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

#endif
