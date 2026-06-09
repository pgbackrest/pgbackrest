/***********************************************************************************************************************************
Azure Storage Read
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/azure/read.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Azure query tokens
***********************************************************************************************************************************/
STRING_STATIC(AZURE_QUERY_VERSION_ID_STR,                           "versionid");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageReadAzure
{
    const StorageReadInterface *interface;                          // Interface
    StorageAzure *storage;                                          // Storage that created this object

    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)
    const String *versionId;                                        // Version id (NULL for most recent)

    HttpRequest *httpRequest;                                       // HTTP request
    HttpResponse *httpResponse;                                     // HTTP response
};

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadAzureOpenAsync(StorageReadAzure *const this, const bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    bool result = false;

    // Wait for response
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->httpResponse = storageAzureResponseP(this->httpRequest, .allowMissing = true, .contentIo = true);

        httpRequestFree(this->httpRequest);
        this->httpRequest = NULL;
    }
    MEM_CONTEXT_OBJ_END();

    // If file exists
    if (httpResponseCodeOk(this->httpResponse))
    {
        result = true;
    }
    // Else error unless ignore missing
    else if (!ignoreMissing)
        THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(this->name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

static bool
storageReadAzureOpen(THIS_VOID, const bool ignoreMissing)
{
    THIS(StorageReadAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse == NULL);

    bool result = false;

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->httpRequest = storageAzureRequestAsyncP(
            this->storage, HTTP_VERB_GET_STR, .path = this->name,
            .query = this->versionId != NULL ? httpQueryPut(httpQueryNewP(), AZURE_QUERY_VERSION_ID_STR, this->versionId) : NULL,
            .header = httpHeaderPutRange(httpHeaderNew(NULL), this->offset, this->limit));
    }
    MEM_CONTEXT_OBJ_END();

    // Wait for response when file missing needs to be reported
    if (ignoreMissing)
    {
        result = storageReadAzureOpenAsync(this, true);
    }
    // Else assume that the file exists for now (it will be checked during read)
    else
        result = true;

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadAzure(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadAzure);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_AZURE, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Complete the open if it was async
    if (this->httpResponse == NULL)
        storageReadAzureOpenAsync(this, false);

    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

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

    ASSERT(this != NULL);

    // In async mode we may not have a response yet so return false
    if (this->httpResponse == NULL)
        FUNCTION_TEST_RETURN(BOOL, false);

    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_TEST_RETURN(BOOL, ioReadEof(httpResponseIoRead(this->httpResponse)));
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

    httpRequestFree(this->httpRequest);
    this->httpRequest = NULL;
    httpResponseFree(this->httpResponse);
    this->httpResponse = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadAzureInterface =
{
    .close = storageReadAzureClose,
    .eof = storageReadAzureEof,
    .open = storageReadAzureOpen,
    .read = storageReadAzure,
};

FN_EXTERN StorageReadAzure *
storageReadAzureNew(
    StorageAzure *const storage, const String *const name, const uint64_t offset, const Variant *const limit,
    const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_AZURE, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadAzure, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadAzure)
        {
            .interface = &storageReadAzureInterface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
            .versionId = strDup(versionId),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_AZURE, this);
}
