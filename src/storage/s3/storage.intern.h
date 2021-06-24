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
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
} StorageS3RequestAsyncParam;

#define storageS3RequestAsyncP(this, verb, path, ...)                                                                              \
    storageS3RequestAsync(this, verb, path, (StorageS3RequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *storageS3RequestAsync(StorageS3 *this, const String *verb, const String *path, StorageS3RequestAsyncParam param);

// Get async response
typedef struct StorageS3ResponseParam
{
    VAR_PARAM_HEADER;
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageS3ResponseParam;

#define storageS3ResponseP(request, ...)                                                                                           \
    storageS3Response(request, (StorageS3ResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageS3Response(HttpRequest *request, StorageS3ResponseParam param);

// Perform sync request
typedef struct StorageS3RequestParam
{
    VAR_PARAM_HEADER;
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageS3RequestParam;

#define storageS3RequestP(this, verb, path, ...)                                                                                   \
    storageS3Request(this, verb, path, (StorageS3RequestParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageS3Request(StorageS3 *this, const String *verb, const String *path, StorageS3RequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_S3_TYPE                                                                                               \
    StorageS3 *
#define FUNCTION_LOG_STORAGE_S3_FORMAT(value, buffer, bufferSize)                                                                  \
    objToLog(value, "StorageS3", buffer, bufferSize)

#endif
