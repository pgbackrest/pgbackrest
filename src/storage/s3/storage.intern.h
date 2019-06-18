/***********************************************************************************************************************************
S3 Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_S3_STORAGE_INTERN_H
#define STORAGE_S3_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageS3 StorageS3;

#include "common/io/http/client.h"
#include "storage/s3/storage.h"

/***********************************************************************************************************************************
Perform an S3 Request
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_S3_REQUEST_RESULT_TYPE                                                                                \
    StorageS3RequestResult
#define FUNCTION_LOG_STORAGE_S3_REQUEST_RESULT_FORMAT(value, buffer, bufferSize)                                                   \
    objToLog(&value, "StorageS3RequestResult", buffer, bufferSize)

typedef struct StorageS3RequestResult
{
    HttpClient *httpClient;
    HttpHeader *responseHeader;
    Buffer *response;
} StorageS3RequestResult;

StorageS3RequestResult storageS3Request(
    StorageS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_S3_TYPE                                                                                               \
    StorageS3 *
#define FUNCTION_LOG_STORAGE_S3_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "StorageS3", buffer, bufferSize)

#endif
