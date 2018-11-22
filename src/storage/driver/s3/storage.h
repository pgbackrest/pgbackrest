/***********************************************************************************************************************************
S3 Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_STORAGE_H
#define STORAGE_DRIVER_S3_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverS3 StorageDriverS3;

#include "common/io/http/client.h"
#include "common/type/string.h"
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

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverS3 * storageDriverS3New(
    const String *path, bool write, StoragePathExpressionCallback pathExpressionFunction, const String *bucket,
    const String *endPoint, const String *region, const String *accessKey, const String *secretAccessKey,
    const String *securityToken, const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile,
    const String *caPath);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool storageDriverS3Exists(StorageDriverS3 *this, const String *path);
StorageInfo storageDriverS3Info(StorageDriverS3 *this, const String *file, bool ignoreMissing);
StringList *storageDriverS3List(StorageDriverS3 *this, const String *path, bool errorOnMissing, const String *expression);
StorageFileRead *storageDriverS3NewRead(StorageDriverS3 *this, const String *file, bool ignoreMissing);
StorageFileWrite *storageDriverS3NewWrite(
    StorageDriverS3 *this, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath,
    bool atomic);
void storageDriverS3PathCreate(StorageDriverS3 *this, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
void storageDriverS3PathRemove(StorageDriverS3 *this, const String *path, bool errorOnMissing, bool recurse);
void storageDriverS3PathSync(StorageDriverS3 *this, const String *path, bool ignoreMissing);
void storageDriverS3Remove(StorageDriverS3 *this, const String *file, bool errorOnMissing);

Buffer *storageDriverS3Request(
    StorageDriverS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
HttpClient *storageDriverS3HttpClient(const StorageDriverS3 *this);
Storage *storageDriverS3Interface(const StorageDriverS3 *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverS3Free(StorageDriverS3 *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_DRIVER_S3_TYPE                                                                                      \
    StorageDriverS3 *
#define FUNCTION_DEBUG_STORAGE_DRIVER_S3_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageDriverS3", buffer, bufferSize)

#endif
