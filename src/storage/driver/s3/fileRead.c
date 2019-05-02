/***********************************************************************************************************************************
S3 Storage File Read Driver
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "storage/driver/s3/fileRead.h"
#include "storage/fileRead.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageFileReadDriverS3
{
    MemContext *memContext;                                         // Object mem context
    StorageFileReadInterface interface;                             // Driver interface
    StorageDriverS3 *storage;                                       // Storage that created this object

    HttpClient *httpClient;                                         // Http client for requests
} StorageFileReadDriverS3;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_FILE_READ_DRIVER_S3_TYPE                                                                              \
    StorageFileReadDriverS3 *
#define FUNCTION_LOG_STORAGE_FILE_READ_DRIVER_S3_FORMAT(value, buffer, bufferSize)                                                 \
    objToLog(value, "StorageFileReadDriverS3", buffer, bufferSize)

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageFileReadDriverS3Open(THIS_VOID)
{
    THIS(StorageFileReadDriverS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_READ_DRIVER_S3, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpClient == NULL);

    bool result = false;

    // Request the file
    storageDriverS3Request(this->storage, HTTP_VERB_GET_STR, this->interface.name, NULL, NULL, false, true);

    // On success
    this->httpClient = storageDriverS3HttpClient(this->storage);

    if (httpClientResponseCode(this->httpClient) == HTTP_RESPONSE_CODE_OK)
        result = true;

    // Else error unless ignore missing
    else if (!this->interface.ignoreMissing)
        THROW_FMT(FileMissingError, "unable to open '%s': No such file or directory", strPtr(this->interface.name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageFileReadDriverS3(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageFileReadDriverS3);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_FILE_READ_DRIVER_S3, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->httpClient != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    FUNCTION_LOG_RETURN(SIZE, ioRead(httpClientIoRead(this->httpClient), buffer));
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageFileReadDriverS3Eof(THIS_VOID)
{
    THIS(StorageFileReadDriverS3);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_DRIVER_S3, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL && this->httpClient != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpClientIoRead(this->httpClient)));
}

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadDriverS3New(StorageDriverS3 *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageFileRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileReadDriverS3")
    {
        StorageFileReadDriverS3 *driver = memNew(sizeof(StorageFileReadDriverS3));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageFileReadInterface)
        {
            .type = STORAGE_DRIVER_S3_TYPE_STR,
            .name = strDup(name),
            .ignoreMissing = ignoreMissing,

            .ioInterface = (IoReadInterface)
            {
                .eof = storageFileReadDriverS3Eof,
                .open = storageFileReadDriverS3Open,
                .read = storageFileReadDriverS3,
            },
        };

        driver->storage = storage;

        this = storageFileReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_FILE_READ, this);
}
