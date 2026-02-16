/***********************************************************************************************************************************
S3 Storage Read

Based on the documentation at https://cloud.google.com/storage/docs/json_api/v1/objects/get
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/read.h"
#include "storage/s3/read.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_READ_S3_TYPE                                        StorageReadS3
#define STORAGE_READ_S3_PREFIX                                      storageReadS3

typedef struct StorageReadS3
{
    StorageReadInterface interface;                                 // Interface
    StorageS3 *storage;                                             // Storage that created this object

    HttpRequest *httpRequest;                                       // HTTP request
    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadS3;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_S3_TYPE                                                                                          \
    StorageReadS3 *
#define FUNCTION_LOG_STORAGE_READ_S3_FORMAT(value, buffer, bufferSize)                                                             \
    objNameToLog(value, "StorageReadS3", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadS3OpenAsync(StorageReadS3 *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
    FUNCTION_LOG_END();

    bool result = false;

    // Wait for response
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->httpResponse = storageS3ResponseP(this->httpRequest, .allowMissing = true, .contentIo = true);

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
storageReadS3Open(THIS_VOID)
{
    THIS(StorageReadS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
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
            this->httpRequest = storageS3RequestAsyncP(
                this->storage, HTTP_VERB_GET_STR, this->interface.name,
                .header = httpHeaderPutRange(httpHeaderNew(NULL), this->interface.offset, this->interface.limit),
                .query =
                    this->interface.versionId == NULL
                        ? NULL : httpQueryPut(httpQueryNewP(), STRDEF("versionId"), this->interface.versionId),
                .sseC = true);
        }
        MEM_CONTEXT_OBJ_END();

        // Wait for response when file missing needs to be reported
        if (this->interface.ignoreMissing)
        {
            result = storageReadS3OpenAsync(this);
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
storageReadS3(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Complete the open if it was async
    if (this->httpResponse == NULL)
        storageReadS3OpenAsync(this);

    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpResponseIoRead(this->httpResponse), buffer));
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadS3Eof(THIS_VOID)
{
    THIS(StorageReadS3);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_S3, this);
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
storageReadS3Close(THIS_VOID)
{
    THIS(StorageReadS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse != NULL);

    httpRequestFree(this->httpRequest);
    httpResponseFree(this->httpResponse);
    this->httpResponse = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const IoReadInterface storageReadS3Interface =
{
    .close = storageReadS3Close,
    .eof = storageReadS3Eof,
    .open = storageReadS3Open,
    .read = storageReadS3,
};

FN_EXTERN StorageRead *
storageReadS3New(
    StorageS3 *const storage, const String *const name, const bool ignoreMissing, const uint64_t offset, const Variant *const limit,
    const bool version, const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(BOOL, version);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);
    ASSERT(limit == NULL || varUInt64(limit) > 0);

    OBJ_NEW_BEGIN(StorageReadS3, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadS3)
        {
            .storage = storage,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(
        STORAGE_READ,
        storageReadNewP(
            this, STORAGE_S3_TYPE, name, ignoreMissing, offset, limit, &storageReadS3Interface, .version = version,
            .versionId = versionId, .retry = true));
}
