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

    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadAzure;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_AZURE_TYPE                                                                                       \
    StorageReadAzure *
#define FUNCTION_LOG_STORAGE_READ_AZURE_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(value, "StorageReadAzure", buffer, bufferSize)

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
    ASSERT(this->httpResponse == NULL);

    bool result = false;

    // Request the file
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->httpResponse = storageAzureRequestP(
            this->storage, HTTP_VERB_GET_STR, .path = this->interface.name, .allowMissing = true, .contentIo = true);
    }
    MEM_CONTEXT_END();

    if (httpResponseCodeOk(this->httpResponse))
    {
        result = true;
    }
    // Else error unless ignore missing
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, "unable to open '%s': No such file or directory", strZ(this->interface.name));

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

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpResponseIoRead(this->httpResponse), buffer));
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

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpResponseIoRead(this->httpResponse)));
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
