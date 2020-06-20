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
// Perform async request
typedef struct StorageS3RequestAsyncParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;
    const Buffer *content;
    TimeMSec *timeMs;
} StorageS3RequestAsyncParam;

#define storageS3RequestAsyncP(this, verb, uri, ...)                                                                               \
    storageS3RequestAsync(this, verb, uri, (StorageS3RequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *storageS3RequestAsync(StorageS3 *this, const String *verb, const String *uri, StorageS3RequestAsyncParam param);

// Get async response
typedef struct StorageS3ResponseParam
{
    VAR_PARAM_HEADER;
    bool allowMissing;
    bool contentIo;
    TimeMSec *timeMs;
} StorageS3ResponseParam;

#define storageS3ResponseP(request, ...)                                                                                           \
    storageS3Response(request, (StorageS3ResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageS3Response(HttpRequest *request, StorageS3ResponseParam param);

// Perform sync request
typedef struct StorageS3RequestParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;
    const Buffer *content;
    bool allowMissing;
    bool contentIo;
    TimeMSec *requestMs;
    TimeMSec *responseMs;
} StorageS3RequestParam;

#define storageS3RequestP(this, verb, uri, ...)                                                                                    \
    storageS3Request(this, verb, uri, (StorageS3RequestParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageS3Request(StorageS3 *this, const String *verb, const String *uri, StorageS3RequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_S3_TYPE                                                                                               \
    StorageS3 *
#define FUNCTION_LOG_STORAGE_S3_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "StorageS3", buffer, bufferSize)

#endif
