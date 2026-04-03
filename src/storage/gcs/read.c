/***********************************************************************************************************************************
GCS Storage Read
***********************************************************************************************************************************/
#include <build.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/gcs/read.h"
#include "storage/read.h"

/***********************************************************************************************************************************
GCS query tokens
***********************************************************************************************************************************/
STRING_STATIC(GCS_QUERY_ALT_STR,                                    "alt");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadGcs
{
    StorageReadInterface interface;                                 // Interface
    StorageGcs *storage;                                            // Storage that created this object

    HttpRequest *httpRequest;                                       // HTTP request
    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadGcs;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_GCS_TYPE                                                                                         \
    StorageReadGcs *
#define FUNCTION_LOG_STORAGE_READ_GCS_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "StorageReadGcs", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadGcsOpenAsync(StorageReadGcs *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_LOG_END();

    bool result = false;

    // Wait for response
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->httpResponse = storageGcsResponseP(this->httpRequest, .allowMissing = true, .contentIo = true);

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
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(this->interface.name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

static bool
storageReadGcsOpen(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse == NULL);

    bool result = false;

    // Read if not versioned or if versionId is not null
    if (!this->interface.version || this->interface.versionId != NULL)
    {
        // Request the file
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            HttpQuery *const query = httpQueryAdd(httpQueryNewP(), GCS_QUERY_ALT_STR, GCS_QUERY_MEDIA_STR);

            if (this->interface.versionId)
                httpQueryAdd(query, varStr(GCS_JSON_GENERATION_VAR), this->interface.versionId);

            this->httpRequest = storageGcsRequestAsyncP(
                this->storage, HTTP_VERB_GET_STR, .object = this->interface.name,
                .header = httpHeaderPutRange(httpHeaderNew(NULL), this->interface.offset, this->interface.limit),
                .query = query);
        }
        MEM_CONTEXT_OBJ_END();

        // Wait for response when file missing needs to be reported
        if (this->interface.ignoreMissing)
        {
            result = storageReadGcsOpenAsync(this);
        }
        // Else assume that the file exists for now (it will be checked during read)
        else
            result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadGcs(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Complete the open if it was async
    if (this->httpResponse == NULL)
        storageReadGcsOpenAsync(this);

    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpResponseIoRead(this->httpResponse), buffer));
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadGcsEof(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_GCS, this);
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
storageReadGcsClose(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    httpRequestFree(this->httpRequest);
    this->httpRequest = NULL;
    httpResponseFree(this->httpResponse);
    this->httpResponse = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const IoReadInterface storageReadGcsInterface =
{
    .close = storageReadGcsClose,
    .eof = storageReadGcsEof,
    .open = storageReadGcsOpen,
    .read = storageReadGcs,
};

FN_EXTERN StorageRead *
storageReadGcsNew(
    StorageGcs *const storage, const String *const name, const bool ignoreMissing, const uint64_t offset,
    const Variant *const limit, const bool version, const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(BOOL, version);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadGcs, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadGcs)
        {
            .storage = storage,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadNewP(
            this, STORAGE_GCS_TYPE, name, ignoreMissing, offset, limit, &storageReadGcsInterface, .version = version,
            .versionId = versionId, .retry = true));
}
