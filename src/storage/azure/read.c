/***********************************************************************************************************************************
Azure Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/azure/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadAzure
{
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

    MEM_CONTEXT_BEGIN(THIS_MEM_CONTEXT())
    {
        // Add offset and/or limit
        HttpHeader *header = NULL;

        if (this->interface.offset != 0 || this->interface.limit != NULL)
        {
            String *const range = strCatFmt(strNew(), HTTP_HEADER_RANGE_BYTES "=%" PRIu64 "-", this->interface.offset);

            if (this->interface.limit != NULL)
                strCatFmt(range, "%" PRIu64, this->interface.offset + varUInt64(this->interface.limit) - 1);

            header = httpHeaderAdd(httpHeaderNew(NULL), HTTP_HEADER_RANGE_STR, range);
        }

        // Request the file
        this->httpResponse = storageAzureRequestP(
            this->storage, HTTP_VERB_GET_STR, .path = this->interface.name, .header = header, .allowMissing = true,
            .contentIo = true);
    }
    MEM_CONTEXT_END();

    if (httpResponseCodeOk(this->httpResponse))
    {
        result = true;
    }
    // Else error unless ignore missing
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(this->interface.name));

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
storageReadAzureNew(
    StorageAzure *const storage, const String *const name, const bool ignoreMissing, const uint64_t offset,
    const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageReadAzure)
    {
        StorageReadAzure *driver = OBJ_NEW_ALLOC();

        *driver = (StorageReadAzure)
        {
            .storage = storage,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_AZURE_TYPE,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .offset = offset,
                .limit = varDup(limit),

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
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
