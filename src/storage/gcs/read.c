/***********************************************************************************************************************************
GCS Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/gcs/read.h"
#include "storage/read.intern.h"

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

    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadGcs;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_GCS_TYPE                                                                                         \
    StorageReadGcs *
#define FUNCTION_LOG_STORAGE_READ_GCS_FORMAT(value, buffer, bufferSize)                                                            \
    objToLog(value, "StorageReadGcs", buffer, bufferSize)

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

    bool result = false;

    // Request the file
    MEM_CONTEXT_BEGIN(THIS_MEM_CONTEXT())
    {
        this->httpResponse = storageGcsRequestP(
            this->storage, HTTP_VERB_GET_STR, .object = this->interface.name,
            .header = httpHeaderPutRange(httpHeaderNew(NULL), this->interface.offset, this->interface.limit),
            .allowMissing = true, .contentIo = true,
            .query = httpQueryAdd(httpQueryNewP(), GCS_QUERY_ALT_STR, GCS_QUERY_MEDIA_STR));
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
storageReadGcs(THIS_VOID, Buffer *buffer, bool block)
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

    FUNCTION_TEST_RETURN(ioReadEof(httpResponseIoRead(this->httpResponse)));
}

/**********************************************************************************************************************************/
StorageRead *
storageReadGcsNew(
    StorageGcs *const storage, const String *const name, const bool ignoreMissing, const uint64_t offset,
    const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_GCS, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageReadGcs)
    {
        StorageReadGcs *driver = OBJ_NEW_ALLOC();

        *driver = (StorageReadGcs)
        {
            .storage = storage,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_GCS_TYPE,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .offset = offset,
                .limit = varDup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .eof = storageReadGcsEof,
                    .open = storageReadGcsOpen,
                    .read = storageReadGcs,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
