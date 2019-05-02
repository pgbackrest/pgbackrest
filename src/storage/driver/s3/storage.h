/***********************************************************************************************************************************
S3 Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_STORAGE_H
#define STORAGE_DRIVER_S3_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant
***********************************************************************************************************************************/
#define STORAGE_DRIVER_S3_TYPE                                      "s3"
    STRING_DECLARE(STORAGE_DRIVER_S3_TYPE_STR);

/***********************************************************************************************************************************
Defaults
***********************************************************************************************************************************/
#define STORAGE_DRIVER_S3_PORT_DEFAULT                              443
#define STORAGE_DRIVER_S3_TIMEOUT_DEFAULT                           60000
#define STORAGE_DRIVER_S3_PARTSIZE_MIN                              ((size_t)5 * 1024 * 1024)

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Storage *storageDriverS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, const String *region, const String *accessKey, const String *secretAccessKey,
    const String *securityToken, size_t partSize, const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer,
    const String *caFile, const String *caPath);

#endif
