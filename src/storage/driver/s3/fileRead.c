/***********************************************************************************************************************************
S3 Storage File Read Driver
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/assert.h"
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_S3, storage);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);

        FUNCTION_TEST_ASSERT(storage != NULL);
        FUNCTION_TEST_ASSERT(name != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(STORAGE_DRIVER_S3_FILE_READ, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageDriverS3FileReadOpen(StorageDriverS3FileRead *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->httpClient == NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
size_t
storageDriverS3FileRead(StorageDriverS3FileRead *this, Buffer *buffer, bool block)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);
        FUNCTION_DEBUG_PARAM(BOOL, block);

        FUNCTION_DEBUG_ASSERT(this != NULL && this->httpClient != NULL);
        FUNCTION_DEBUG_ASSERT(buffer != NULL && !bufFull(buffer));
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(SIZE, ioRead(httpClientIoRead(this->httpClient), buffer));
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
bool
storageDriverS3FileReadEof(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_DEBUG_ASSERT(this != NULL && this->httpClient != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, ioReadEof(httpClientIoRead(this->httpClient)));
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageDriverS3FileReadIgnoreMissing(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->ignoreMissing);
}

/***********************************************************************************************************************************
Get the interface
***********************************************************************************************************************************/
StorageFileRead *
storageDriverS3FileReadInterface(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STORAGE_FILE_READ, this->interface);
}

/***********************************************************************************************************************************
Get the I/O interface
***********************************************************************************************************************************/
IoRead *
storageDriverS3FileReadIo(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->io);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageDriverS3FileReadName(const StorageDriverS3FileRead *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_S3_FILE_READ, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->name);
}
