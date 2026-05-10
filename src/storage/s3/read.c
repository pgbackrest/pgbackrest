/***********************************************************************************************************************************
S3 Storage Read

Based on the documentation at https://cloud.google.com/storage/docs/json_api/v1/objects/get
***********************************************************************************************************************************/
#include <build.h>

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
    const StorageReadInterface *interface;                          // Interface
    StorageS3 *storage;                                             // Storage that created this object
    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)
    const String *versionId;                                        // Version id (NULL for most recent)

    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadS3;

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadS3Open(THIS_VOID)
{
    THIS(StorageReadS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse == NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        this->httpResponse = storageS3RequestP(
            this->storage, HTTP_VERB_GET_STR, this->name,
            .header = httpHeaderPutRange(httpHeaderNew(NULL), this->offset, this->limit),
            .query = this->versionId != NULL ? httpQueryPut(httpQueryNewP(), STRDEF("versionId"), this->versionId) : NULL,
            .allowMissing = true, .contentIo = true, .sseC = true);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN(BOOL, httpResponseCodeOk(this->httpResponse));
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
    ASSERT(this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

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

    ASSERT(this != NULL && this->httpResponse != NULL);
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

    httpResponseFree(this->httpResponse);
    this->httpResponse = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadS3Interface =
{
    .close = storageReadS3Close,
    .eof = storageReadS3Eof,
    .open = storageReadS3Open,
    .read = storageReadS3,
};

/**********************************************************************************************************************************/
FN_EXTERN StorageReadS3 *
storageReadS3New(
    StorageS3 *const storage, const String *const name, const uint64_t offset, const Variant *const limit,
    const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);
    ASSERT(limit == NULL || varUInt64(limit) > 0);

    OBJ_NEW_BEGIN(StorageReadS3, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadS3)
        {
            .interface = &storageReadS3Interface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
            .versionId = strDup(versionId),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_S3, this);
}
