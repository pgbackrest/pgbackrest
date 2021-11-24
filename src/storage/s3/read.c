/***********************************************************************************************************************************
S3 Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/s3/read.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_READ_S3_TYPE                                        StorageReadS3
#define STORAGE_READ_S3_PREFIX                                      storageReadS3

typedef struct StorageReadS3
{
    StorageReadInterface interface;                                 // Interface
    StorageS3 *storage;                                             // Storage that created this object

    HttpResponse *httpResponse;                                     // HTTP response
} StorageReadS3;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_S3_TYPE                                                                                          \
    StorageReadS3 *
#define FUNCTION_LOG_STORAGE_READ_S3_FORMAT(value, buffer, bufferSize)                                                             \
    objToLog(value, "StorageReadS3", buffer, bufferSize)

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

    bool result = false;

    // Request the file
    MEM_CONTEXT_BEGIN(THIS_MEM_CONTEXT())
    {
        this->httpResponse = storageS3RequestP(
            this->storage, HTTP_VERB_GET_STR, this->interface.name, .allowMissing = true, .contentIo = true);
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
storageReadS3(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageReadS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_S3, this);
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
storageReadS3Eof(THIS_VOID)
{
    THIS(StorageReadS3);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_S3, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL && this->httpResponse != NULL);
    ASSERT(httpResponseIoRead(this->httpResponse) != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpResponseIoRead(this->httpResponse)));
}

/**********************************************************************************************************************************/
StorageRead *
storageReadS3New(StorageS3 *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageReadS3)
    {
        StorageReadS3 *driver = OBJ_NEW_ALLOC();

        *driver = (StorageReadS3)
        {
            .storage = storage,

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_S3_TYPE,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,

                .ioInterface = (IoReadInterface)
                {
                    .eof = storageReadS3Eof,
                    .open = storageReadS3Open,
                    .read = storageReadS3,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
