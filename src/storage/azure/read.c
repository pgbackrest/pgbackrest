/***********************************************************************************************************************************
Azure Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "storage/azure/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_READ_AZURE_TYPE                                     StorageReadAzure
#define STORAGE_READ_AZURE_PREFIX                                   storageReadAzure

typedef struct StorageReadAzure
{
    MemContext *memContext;                                         // Object mem context
    StorageReadInterface interface;                                 // Interface
    StorageAzure *storage;                                          // Storage that created this object

    HttpClient *httpClient;                                         // Http client for requests
} StorageReadAzure;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_AZURE_TYPE                                                                                       \
    StorageReadAzure *
#define FUNCTION_LOG_STORAGE_READ_AZURE_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(value, "StorageReadAzure", buffer, bufferSize)

/***********************************************************************************************************************************
Mark http client as done so it can be reused
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(STORAGE_READ_AZURE, LOG, logLevelTrace)
{
    httpClientDone(this->httpClient);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadAzureOpen(THIS_VOID)
{
    THIS(StorageReadAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpClient == NULL);

    bool result = false;

    // Request the file
    this->httpClient = storageAzureRequestP(
        this->storage, HTTP_VERB_GET_STR, .uri = this->interface.name, .content = true, .allowMissing = true).httpClient;

    if (httpClientResponseCodeOk(this->httpClient))
    {
        memContextCallbackSet(this->memContext, storageReadAzureFreeResource, this);
        result = true;
    }
    // Else error unless ignore missing
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, "unable to open '%s': No such file or directory", strPtr(this->interface.name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadAzure(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageReadAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->httpClient != NULL);
    ASSERT(httpClientIoRead(this->httpClient) != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpClientIoRead(this->httpClient), buffer));
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageReadAzureClose(THIS_VOID)
{
    THIS(StorageReadAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpClient != NULL);

    memContextCallbackClear(this->memContext);
    storageReadAzureFreeResource(this);
    this->httpClient = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadAzureEof(THIS_VOID)
{
    THIS(StorageReadAzure);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_AZURE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL && this->httpClient != NULL);
    ASSERT(httpClientIoRead(this->httpClient) != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpClientIoRead(this->httpClient)));
}

/**********************************************************************************************************************************/
StorageRead *
storageReadAzureNew(StorageAzure *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageReadAzure")
    {
        StorageReadAzure *driver = memNew(sizeof(StorageReadAzure));

        *driver = (StorageReadAzure)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_AZURE_TYPE_STR,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,

                .ioInterface = (IoReadInterface)
                {
                    .close = storageReadAzureClose,
                    .eof = storageReadAzureEof,
                    .open = storageReadAzureOpen,
                    .read = storageReadAzure,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
