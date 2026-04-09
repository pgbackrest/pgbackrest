/***********************************************************************************************************************************
S3 Storage
***********************************************************************************************************************************/
#ifndef STORAGE_S3_STORAGE_H
#define STORAGE_S3_STORAGE_H

#include "common/io/http/url.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
#define STORAGE_S3_TYPE                                             STRID6("s3", 0x7d31)

/***********************************************************************************************************************************
Key type
***********************************************************************************************************************************/
typedef enum
{
    storageS3KeyTypeShared,
    storageS3KeyTypeAuto,
    storageS3KeyTypeWebId,
    storageS3KeyTypePodId,
} StorageS3KeyType;

/***********************************************************************************************************************************
URI style
***********************************************************************************************************************************/
typedef enum
{
    storageS3UriStyleHost,
    storageS3UriStylePath,
} StorageS3UriStyle;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageS3New(
    const String *path, bool write, time_t targetTime, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, const String *region, StorageS3KeyType keyType, StorageS3UriStyle uriStyle, const String *accessKey,
    const String *secretAccessKey, const String *securityToken, const String *kmsKeyId, const String *sseCustomerKey,
    const String *credRole, const String *tokenFile, const String *credUrl, size_t partSize, const KeyValue *tag,
    const String *host, unsigned int port, TimeMSec timeout, HttpProtocolType protocolType, bool verifyPeer, const String *caFile,
    const String *caPath, bool requesterPays);

#endif
