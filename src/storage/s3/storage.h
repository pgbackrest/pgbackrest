/***********************************************************************************************************************************
S3 Storage
***********************************************************************************************************************************/
#ifndef STORAGE_S3_STORAGE_H
#define STORAGE_S3_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_S3_TYPE                                             "s3"
    STRING_DECLARE(STORAGE_S3_TYPE_STR);

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
#define STORAGE_S3_TIMEOUT_DEFAULT                                  60000
#define STORAGE_S3_PARTSIZE_MIN                                     ((size_t)5 * 1024 * 1024)
#define STORAGE_S3_DELETE_MAX                                       1000

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Storage *storageS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, StorageS3UriStyle uriStyle, const String *region, const String *accessKey,
    const String *secretAccessKey, const String *securityToken, size_t partSize, unsigned int deleteMax, const String *host,
    unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath);

#endif
