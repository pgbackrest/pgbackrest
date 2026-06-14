/***********************************************************************************************************************************
GCS Storage
***********************************************************************************************************************************/
#ifndef STORAGE_GCS_STORAGE_H
#define STORAGE_GCS_STORAGE_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_GCS_TYPE                                            STRID5("gcs", 0x4c670)

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageGcsKeyTypeService,
    storageGcsKeyTypeToken,
    storageGcsKeyTypeAuto,
} StorageGcsKeyType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageGcsNew(
    const String *path, bool write, time_t targetTime, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    StorageGcsKeyType keyType, const String *key, size_t blockSize, const KeyValue *tag, const String *endpoint, TimeMSec timeout,
    bool verifyPeer, const String *caFile, const String *caPath, const String *userProject);

#endif
