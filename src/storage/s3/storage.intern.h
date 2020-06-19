/***********************************************************************************************************************************
S3 Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_S3_STORAGE_INTERN_H
#define STORAGE_S3_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageS3 StorageS3;

#include "common/io/http/request.h"
#include "storage/s3/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Perform async S3 request and get response
typedef struct StorageS3RequestAsyncParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;
    const Buffer *content;
} StorageS3RequestAsyncParam;

#define storageS3RequestAsyncP(this, verb, uri, ...)                                                                               \
    storageS3RequestAsync(this, verb, uri, (StorageS3RequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *storageS3RequestAsync(StorageS3 *this, const String *verb, const String *uri, StorageS3RequestAsyncParam param);

// Get async S3 response
typedef struct StorageS3ResponseParam
{
    VAR_PARAM_HEADER;
    bool allowMissing;
    bool contentIo;
} StorageS3ResponseParam;

#define storageS3ResponseP(request, ...)                                                                                            \
    storageS3Response(request, (StorageS3ResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageS3Response(HttpRequest *request, StorageS3ResponseParam param);

// Perform sync S3 request
HttpResponse *storageS3Request(
    StorageS3 *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *content,
    bool contentIo, bool allowMissing);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_S3_TYPE                                                                                               \
    StorageS3 *
#define FUNCTION_LOG_STORAGE_S3_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "StorageS3", buffer, bufferSize)

#endif
