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
HTTP headers
***********************************************************************************************************************************/
#define GCS_HEADER_UPLOAD_ID                                        "x-guploader-uploadid"
    STRING_DECLARE(GCS_HEADER_UPLOAD_ID_STR);

/***********************************************************************************************************************************
Query tokens
***********************************************************************************************************************************/
#define GCS_QUERY_FIELDS                                            "fields"
    STRING_DECLARE(GCS_QUERY_FIELDS_STR);
#define GCS_QUERY_NAME                                              "name"
    STRING_DECLARE(GCS_QUERY_NAME_STR);
#define GCS_QUERY_UPLOAD_ID                                         "upload_id"
    STRING_DECLARE(GCS_QUERY_UPLOAD_ID_STR);

/***********************************************************************************************************************************
JSON tokens
***********************************************************************************************************************************/
#define GCS_JSON_MD5_HASH                                           "md5Hash"
    VARIANT_DECLARE(GCS_JSON_MD5_HASH_VAR);
#define GCS_JSON_NAME                                               "name"
    VARIANT_DECLARE(GCS_JSON_NAME_VAR);
#define GCS_JSON_SIZE                                               "size"
    VARIANT_DECLARE(GCS_JSON_SIZE_VAR);

/***********************************************************************************************************************************
Perform an GCS Request
***********************************************************************************************************************************/
// Perform async request
typedef struct StorageGcsRequestAsyncParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    bool upload;                                                    // Is an object upload?
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
    bool allowIncomplete;                                           // Allow incomplete resume (used for resumable upload)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageGcsResponseParam;

#define storageGcsResponseP(request, ...)                                                                                          \
    storageGcsResponse(request, (StorageGcsResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageGcsResponse(HttpRequest *request, StorageGcsResponseParam param);

typedef struct StorageGcsRequestParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    bool upload;                                                    // Is an object upload?
    const String *object;                                           // Object to include in URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool allowIncomplete;                                           // Allow incomplete resume (used for resumable upload)
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
