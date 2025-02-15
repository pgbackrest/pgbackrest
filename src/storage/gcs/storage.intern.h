/***********************************************************************************************************************************
GCS Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_GCS_STORAGE_INTERN_H
#define STORAGE_GCS_STORAGE_INTERN_H

#include "common/io/http/request.h"
#include "common/io/http/url.h"
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
#define GCS_QUERY_MEDIA                                             "media"
STRING_DECLARE(GCS_QUERY_MEDIA_STR);
#define GCS_QUERY_NAME                                              "name"
STRING_DECLARE(GCS_QUERY_NAME_STR);
#define GCS_QUERY_UPLOAD_ID                                         "upload_id"
STRING_DECLARE(GCS_QUERY_UPLOAD_ID_STR);
#define GCS_QUERY_USER_PROJECT                                      "userProject"
STRING_DECLARE(GCS_QUERY_USER_PROJECT_STR);

/***********************************************************************************************************************************
JSON tokens
***********************************************************************************************************************************/
#define GCS_JSON_GENERATION                                         "generation"
VARIANT_DECLARE(GCS_JSON_GENERATION_VAR);
#define GCS_JSON_MD5_HASH                                           "md5Hash"
VARIANT_DECLARE(GCS_JSON_MD5_HASH_VAR);
#define GCS_JSON_NAME                                               "name"
VARIANT_DECLARE(GCS_JSON_NAME_VAR);
#define GCS_JSON_SIZE                                               "size"
VARIANT_DECLARE(GCS_JSON_SIZE_VAR);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageGcs
{
    STORAGE_COMMON_MEMBER;
    HttpClient *httpClient;                                         // Http client to service requests
    StringList *headerRedactList;                                   // List of headers to redact from logging
    StringList *queryRedactList;                                    // List of query keys to redact from logging

    bool write;                                                     // Storage is writable
    const String *bucket;                                           // Bucket to store data in
    const String *endpoint;                                         // Endpoint
    size_t chunkSize;                                               // Block size for resumable upload
    unsigned int deleteMax;                                         // Maximum objects that can be deleted in one request
    const Buffer *tag;                                              // Tags to be applied to objects

    const String *userProject;                                      // Project ID

    StorageGcsKeyType keyType;                                      // Auth key type
    const String *key;                                              // Key (value depends on key type)
    String *token;                                                  // Token
    time_t tokenTimeExpire;                                         // Token expiration time (if service auth)
    HttpUrl *authUrl;                                               // URL for authentication server
    HttpClient *authClient;                                         // Client to service auth requests
} StorageGcs;

/***********************************************************************************************************************************
Multi-Part request data
***********************************************************************************************************************************/
typedef struct StorageGcsRequestPart
{
    const String *object;                                           // Object to include in URI
    const String *verb;                                             // Verb (GET, PUT, etc)
} StorageGcsRequestPart;

/***********************************************************************************************************************************
Perform a GCS Request
***********************************************************************************************************************************/
// Perform async request
typedef struct StorageGcsRequestAsyncParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    bool upload;                                                    // Is an object upload?
    bool noAuth;                                                    // Exclude authentication header?
    bool tag;                                                       // Add tags when available?
    const String *path;                                             // URI path (this overrides object)
    const String *object;                                           // Object to include in URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    const List *contentList;                                        // Request content part list
} StorageGcsRequestAsyncParam;

#define storageGcsRequestAsyncP(this, verb, ...)                                                                                   \
    storageGcsRequestAsync(this, verb, (StorageGcsRequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN HttpRequest *storageGcsRequestAsync(StorageGcs *this, const String *verb, StorageGcsRequestAsyncParam param);

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

FN_EXTERN HttpResponse *storageGcsResponse(HttpRequest *request, StorageGcsResponseParam param);

typedef struct StorageGcsRequestParam
{
    VAR_PARAM_HEADER;
    bool noBucket;                                                  // Exclude bucket from the URI?
    bool upload;                                                    // Is an object upload?
    bool noAuth;                                                    // Exclude authentication header?
    bool tag;                                                       // Add tags when available?
    const String *path;                                             // URI path (this overrides object)
    const String *object;                                           // Object to include in URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    const List *contentList;                                        // Request content part list
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool allowIncomplete;                                           // Allow incomplete resume (used for resumable upload)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageGcsRequestParam;

#define storageGcsRequestP(this, verb, ...)                                                                                        \
    storageGcsRequest(this, verb, (StorageGcsRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN HttpResponse *storageGcsRequest(StorageGcs *this, const String *verb, StorageGcsRequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_GCS_TYPE                                                                                              \
    StorageGcs *
#define FUNCTION_LOG_STORAGE_GCS_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "StorageGcs", buffer, bufferSize)

#endif
