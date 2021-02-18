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
    storageGcsKeyTypeNone,
    storageGcsKeyTypeService,
    storageGcsKeyTypeToken,
} StorageGcsKeyType;

#define STORAGE_GCS_KEY_TYPE_NONE                                   "none"
#define STORAGE_GCS_KEY_TYPE_SERVICE                                "service"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_GCS_CHUNKSIZE_MIN                                   ((size_t)256 * 1024)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageGcsNew(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    StorageGcsKeyType keyType, const String *key, size_t blockSize,  const String *endpoint, unsigned int port, TimeMSec timeout,
    bool verifyPeer, const String *caFile, const String *caPath);

#endif
