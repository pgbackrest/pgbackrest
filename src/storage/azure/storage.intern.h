/***********************************************************************************************************************************
Azure Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_INTERN_H
#define STORAGE_AZURE_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageAzure StorageAzure;

#include "common/io/http/request.h"
#include "storage/azure/storage.h"

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
#define AZURE_QUERY_COMP                                            "comp"
    STRING_DECLARE(AZURE_QUERY_COMP_STR);
#define AZURE_QUERY_RESTYPE                                         "restype"
    STRING_DECLARE(AZURE_QUERY_RESTYPE_STR);
#define AZURE_QUERY_VALUE_CONTAINER                                 "container"
    STRING_DECLARE(AZURE_QUERY_VALUE_CONTAINER_STR);

/***********************************************************************************************************************************
Perform an Azure Request
***********************************************************************************************************************************/
// Perform async request
typedef struct StorageAzureRequestAsyncParam
{
    VAR_PARAM_HEADER;
    const String *uri;                                              // Request URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
} StorageAzureRequestAsyncParam;

#define storageAzureRequestAsyncP(this, verb, ...)                                                                            \
    storageAzureRequestAsync(this, verb, (StorageAzureRequestAsyncParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpRequest *storageAzureRequestAsync(StorageAzure *this, const String *verb, StorageAzureRequestAsyncParam param);

// Get async response
typedef struct StorageAzureResponseParam
{
    VAR_PARAM_HEADER;
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageAzureResponseParam;

#define storageAzureResponseP(request, ...)                                                                                        \
    storageAzureResponse(request, (StorageAzureResponseParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageAzureResponse(HttpRequest *request, StorageAzureResponseParam param);

typedef struct StorageAzureRequestParam
{
    VAR_PARAM_HEADER;
    const String *uri;                                              // Request URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *content;                                          // Request content
    bool allowMissing;                                              // Allow missing files (caller can check response code)
    bool contentIo;                                                 // Is IoRead interface required to read content?
} StorageAzureRequestParam;

#define storageAzureRequestP(this, verb, ...)                                                                                      \
    storageAzureRequest(this, verb, (StorageAzureRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

HttpResponse *storageAzureRequest(StorageAzure *this, const String *verb, StorageAzureRequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_AZURE_TYPE                                                                                            \
    StorageAzure *
#define FUNCTION_LOG_STORAGE_AZURE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "StorageAzure", buffer, bufferSize)

#endif
