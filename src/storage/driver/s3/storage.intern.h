/***********************************************************************************************************************************
S3 Storage Driver Internal
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_S3_STORAGE_INTERN_H
#define STORAGE_DRIVER_S3_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverS3 StorageDriverS3;

#include "common/io/http/client.h"
#include "storage/driver/s3/storage.h"

/***********************************************************************************************************************************
Perform an S3 Request
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_S3_REQUEST_RESULT_TYPE                                                                         \
    StorageDriverS3RequestResult
#define FUNCTION_LOG_STORAGE_DRIVER_S3_REQUEST_RESULT_FORMAT(value, buffer, bufferSize)                                            \
    objToLog(&value, "StorageDriverS3RequestResult", buffer, bufferSize)

typedef struct StorageDriverS3RequestResult
{
    HttpHeader *responseHeader;
    Buffer *response;
} StorageDriverS3RequestResult;

StorageDriverS3RequestResult storageDriverS3Request(
    StorageDriverS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
HttpClient *storageDriverS3HttpClient(const StorageDriverS3 *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_S3_TYPE                                                                                        \
    StorageDriverS3 *
#define FUNCTION_LOG_STORAGE_DRIVER_S3_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "StorageDriverS3", buffer, bufferSize)

#endif
