/***********************************************************************************************************************************
S3 Storage File Read Driver
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/http/client.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/s3/fileRead.h"
#include "storage/fileRead.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageDriverS3FileRead
{
    MemContext *memContext;
    StorageDriverS3 *storage;
    StorageFileRead *interface;
    IoRead *io;
    String *name;
    bool ignoreMissing;

    HttpClient *httpClient;                                         // Http client for requests
};

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageDriverS3FileRead *
storageDriverS3FileReadNew(StorageDriverS3 *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);

    StorageDriverS3FileRead *this = NULL;

    // Create the file object
    MEM_CONTEXT_NEW_BEGIN("StorageDriverS3FileRead")
    {
        this = memNew(sizeof(StorageDriverS3FileRead));
        this->memContext = MEM_CONTEXT_NEW();
        this->storage = storage;
        this->name = strDup(name);
        this->ignoreMissing = ignoreMissing;

        this->interface = storageFileReadNewP(
            strNew(STORAGE_DRIVER_S3_TYPE), this,
            .ignoreMissing = (StorageFileReadInterfaceIgnoreMissing)storageDriverS3FileReadIgnoreMissing,
            .io = (StorageFileReadInterfaceIo)storageDriverS3FileReadIo,
            .name = (StorageFileReadInterfaceName)storageDriverS3FileReadName);

        this->io = ioReadNewP(
            this, .eof = (IoReadInterfaceEof)storageDriverS3FileReadEof,
            .open = (IoReadInterfaceOpen)storageDriverS3FileReadOpen, .read = (IoReadInterfaceRead)storageDriverS3FileRead);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_DRIVER_S3_FILE_READ, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageDriverS3FileReadOpen(StorageDriverS3FileRead *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->httpClient == NULL);

    bool result = false;

    // Request the file
    storageDriverS3Request(this->storage, HTTP_VERB_GET_STR, this->name, NULL, NULL, false, true);

    // On success
    this->httpClient = storageDriverS3HttpClient(this->storage);

    if (httpClientResponseCode(this->httpClient) == HTTP_RESPONSE_CODE_OK)
        result = true;

    // Else error unless ignore missing
    else if (!this->ignoreMissing)
        THROW_FMT(FileMissingError, "unable to open '%s': No such file or directory", strPtr(this->name));

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
size_t
storageDriverS3FileRead(StorageDriverS3FileRead *this, Buffer *buffer, bool block)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
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
bool
storageDriverS3FileReadEof(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL && this->httpClient != NULL);

    FUNCTION_TEST_RETURN(ioReadEof(httpClientIoRead(this->httpClient)));
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageDriverS3FileReadIgnoreMissing(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->ignoreMissing);
}

/***********************************************************************************************************************************
Get the interface
***********************************************************************************************************************************/
StorageFileRead *
storageDriverS3FileReadInterface(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}

/***********************************************************************************************************************************
Get the I/O interface
***********************************************************************************************************************************/
IoRead *
storageDriverS3FileReadIo(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->io);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageDriverS3FileReadName(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->name);
}
