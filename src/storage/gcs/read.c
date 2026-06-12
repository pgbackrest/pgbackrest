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
struct StorageReadGcs
{
    const StorageReadInterface *interface;                          // Interface
    StorageGcs *storage;                                            // Storage that created this object

    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)
    const String *versionId;                                        // Version id (NULL for most recent)

    HttpResponse *httpResponse;                                     // HTTP response
};

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadGcsOpen(THIS_VOID)
{
    THIS(StorageReadGcs);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_GCS, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpResponse == NULL);

    // Request the file
    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        HttpQuery *const query = httpQueryAdd(httpQueryNewP(), GCS_QUERY_ALT_STR, GCS_QUERY_MEDIA_STR);

        if (this->versionId != NULL)
            httpQueryAdd(query, varStr(GCS_JSON_GENERATION_VAR), this->versionId);

        this->httpResponse = storageGcsRequestP(
            this->storage, HTTP_VERB_GET_STR, .object = this->name,
            .header = httpHeaderPutRange(httpHeaderNew(NULL), this->offset, this->limit), .allowMissing = true, .contentIo = true,
            .query = query);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_LOG_RETURN(BOOL, httpResponseCodeOk(this->httpResponse));
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

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

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

    ASSERT(this != NULL && this->httpResponse != NULL);
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
    ASSERT(this->httpResponse != NULL);

    httpResponseFree(this->httpResponse);
    this->httpResponse = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadGcsInterface =
{
    .close = storageReadGcsClose,
    .eof = storageReadGcsEof,
    .open = storageReadGcsOpen,
    .read = storageReadGcs,
};

FN_EXTERN StorageReadGcs *
storageReadGcsNew(
    StorageGcs *const storage, const String *const name, const uint64_t offset, const Variant *const limit,
    const String *const versionId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(STRING, versionId);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadGcs, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadGcs)
        {
            .interface = &storageReadGcsInterface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
            .versionId = strDup(versionId),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_GCS, this);
}
