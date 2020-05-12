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

StorageAzureRequestResult storageAzureRequest(
    StorageAzure *this, const String *verb, const String *uri, const HttpQuery *query, const Buffer *body, bool returnContent,
    bool allowMissing);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_AZURE_TYPE                                                                                            \
    StorageAzure *
#define FUNCTION_LOG_STORAGE_AZURE_FORMAT(value, buffer, bufferSize)                                                               \
    objToLog(value, "StorageAzure", buffer, bufferSize)

#endif
