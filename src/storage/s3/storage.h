/***********************************************************************************************************************************
S3 Storage
***********************************************************************************************************************************/
#ifndef STORAGE_S3_STORAGE_H
#define STORAGE_S3_STORAGE_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_S3_TYPE                                             STRID5("s3", 0x3d30)

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageS3KeyTypeShared,
    storageS3KeyTypeAuto,
} StorageS3KeyType;

#define STORAGE_S3_KEY_TYPE_SHARED                                  "shared"
#define STORAGE_S3_KEY_TYPE_AUTO                                    "auto"

/***********************************************************************************************************************************
URI style
***********************************************************************************************************************************/
typedef enum
{
    storageS3UriStyleHost,
    storageS3UriStylePath,
} StorageS3UriStyle;

#define STORAGE_S3_URI_STYLE_HOST                                   "host"
#define STORAGE_S3_URI_STYLE_PATH                                   "path"

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_S3_PARTSIZE_MIN                                     ((size_t)5 * 1024 * 1024)

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, StorageS3UriStyle uriStyle, const String *region, StorageS3KeyType keyType, const String *accessKey,
    const String *secretAccessKey, const String *securityToken, const String *credRole, size_t partSize, const String *host,
    unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

#endif
