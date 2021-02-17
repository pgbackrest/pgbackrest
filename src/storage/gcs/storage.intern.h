/***********************************************************************************************************************************
GCS Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_GCS_STORAGE_INTERN_H
#define STORAGE_GCS_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageGcs StorageGcs;

#include "common/io/http/request.h"
#include "storage/gcs/storage.h"

/***********************************************************************************************************************************
GCS query tokens
***********************************************************************************************************************************/
// #define GCS_QUERY_COMP                                              "comp"
//     STRING_DECLARE(GCS_QUERY_COMP_STR);
// #define GCS_QUERY_RESTYPE                                           "restype"
//     STRING_DECLARE(GCS_QUERY_RESTYPE_STR);
// #define GCS_QUERY_VALUE_CONTAINER                                   "container"
//     STRING_DECLARE(GCS_QUERY_VALUE_CONTAINER_STR);

/***********************************************************************************************************************************
Perform an GCS Request
***********************************************************************************************************************************/
// Perform async request
typedef struct StorageGcsRequestAsyncParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    const String *object;                                           // Object to include in URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
} StorageGcsRequestAsyncParam;

#define storageGcsRequestAsyncP(this, verb, ...)                                                                                   \
    storageGcsRequestAsync(this, verb, (StorageGcsRequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *storageGcsRequestAsync(StorageGcs *this, const String *verb, StorageGcsRequestAsyncParam param);

// Get async response
typedef struct StorageGcsResponseParam
{
    VAR_PARAM_HEADER;
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageGcsResponseParam;

#define storageGcsResponseP(request, ...)                                                                                          \
    storageGcsResponse(request, (StorageGcsResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageGcsResponse(HttpRequest *request, StorageGcsResponseParam param);

typedef struct StorageGcsRequestParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    const String *object;                                           // Object to include in URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageGcsRequestParam;

#define storageGcsRequestP(this, verb, ...)                                                                                        \
    storageGcsRequest(this, verb, (StorageGcsRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageGcsRequest(StorageGcs *this, const String *verb, StorageGcsRequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_GCS_TYPE                                                                                              \
    StorageGcs *
#define FUNCTION_LOG_STORAGE_GCS_FORMAT(value, buffer, bufferSize)                                                                 \
    objToLog(value, "StorageGcs", buffer, bufferSize)

#endif
