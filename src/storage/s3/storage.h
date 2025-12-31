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
    storageS3KeyTypeShared = STRID5("shared", 0x85905130),
    storageS3KeyTypeAuto = STRID5("auto", 0x7d2a10),
    storageS3KeyTypeWebId = STRID5("web-id", 0x89d88b70),
    storageS3KeyTypePodId = STRID5("pod-id", 0x89d91f00),
} StorageS3KeyType;

/***********************************************************************************************************************************
URI style
***********************************************************************************************************************************/
typedef enum
{
    storageS3UriStyleHost = STRID5("host", 0xa4de80),
    storageS3UriStylePath = STRID5("path", 0x450300),
} StorageS3UriStyle;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Storage *storageS3New(
    const String *path, bool write, time_t targetTime, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, StorageS3UriStyle uriStyle, const String *region, StorageS3KeyType keyType, const String *accessKey,
    const String *secretAccessKey, const String *securityToken, const String *kmsKeyId, const String *sseCustomerKey,
    const String *credRole, const String *webIdTokenFile, const String *credUrl, size_t partSize, const KeyValue *tag,
    const String *host, unsigned int port, TimeMSec timeout, HttpProtocolType protocolType, bool verifyPeer, const String *caFile,
    const String *caPath, bool requesterPays);

#endif
