/***********************************************************************************************************************************
Azure Storage Internal
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_STORAGE_INTERN_H
#define STORAGE_AZURE_STORAGE_INTERN_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageAzure StorageAzure;

#include "common/io/http/client.h"
#include "storage/azure/storage.h"

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
#define AZURE_QUERY_COMP                                            "comp"
    STRING_DECLARE(AZURE_QUERY_COMP_STR);

/***********************************************************************************************************************************
Perform an Azure Request
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_AZURE_REQUEST_RESULT_TYPE                                                                             \
    StorageAzureRequestResult
#define FUNCTION_LOG_STORAGE_AZURE_REQUEST_RESULT_FORMAT(value, buffer, bufferSize)                                                \
    objToLog(&value, "StorageAzureRequestResult", buffer, bufferSize)

typedef struct StorageAzureRequestResult
{
    HttpClient *httpClient;
    HttpHeader *responseHeader;
    Buffer *response;
} StorageAzureRequestResult;

typedef struct StorageAzureRequestParam
{
    VAR_PARAM_HEADER;
    const String *uri;                                              // Request URI
    const HttpHeader *header;                                       // Request headers
    const HttpQuery *query;                                         // Query parameters
    const Buffer *body;                                             // Request body
    bool content;                                                   // Will the response have content?
    bool contentBuffer;                                             // Return response content in a single buffer
    bool allowMissing;                                              // Allow missing files (caller can check response code)
} StorageAzureRequestParam;

#define storageAzureRequestP(this, verb, ...)                                                                                      \
    storageAzureRequest(this, verb, (StorageAzureRequestParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageAzureRequestResult storageAzureRequest(StorageAzure *this, const String *verb, StorageAzureRequestParam param);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_AZURE_TYPE                                                                                            \
    StorageAzure *
#define FUNCTION_LOG_STORAGE_AZURE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "StorageAzure", buffer, bufferSize)

#endif
