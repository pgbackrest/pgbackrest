/***********************************************************************************************************************************
Storage File Read Driver For Posix
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/posix/driverFile.h"
#include "storage/driver/posix/driverRead.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileReadPosix
{
    MemContext *memContext;

    String *name;
    bool ignoreMissing;
    size_t bufferSize;

    int handle;
    bool eof;
    size_t size;
};

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageFileReadPosix *
storageFileReadPosixNew(const String *name, bool ignoreMissing, size_t bufferSize)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(BOOL, ignoreMissing);
        FUNCTION_DEBUG_PARAM(BOOL, bufferSize);

        FUNCTION_TEST_ASSERT(name != NULL);
        FUNCTION_TEST_ASSERT(bufferSize > 0);
    FUNCTION_DEBUG_END();

    StorageFileReadPosix *this = NULL;

    // Create the file object
    MEM_CONTEXT_NEW_BEGIN("StorageFileReadPosix")
    {
        this = memNew(sizeof(StorageFileReadPosix));
        this->memContext = MEM_CONTEXT_NEW();
        this->name = strDup(name);
        this->ignoreMissing = ignoreMissing;
        this->bufferSize = bufferSize;

        this->handle = -1;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_READ_POSIX, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageFileReadPosixOpen(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->handle == -1);
    FUNCTION_DEBUG_END();

    bool result = false;

    // Open the file and handle errors
    this->handle = storageFilePosixOpen(this->name, O_RDONLY, 0, this->ignoreMissing, &FileOpenError, "read");

    // On success set free callback to ensure file handle is freed
    if (this->handle != -1)
    {
        memContextCallback(this->memContext, (MemContextCallback)storageFileReadPosixFree, this);
        result = true;
    }

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
Buffer *
storageFileReadPosix(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->handle != -1);
    FUNCTION_DEBUG_END();

    Buffer *result = NULL;

    ASSERT_DEBUG(this != NULL);

    // Read if EOF has not been reached
    if (!this->eof)
    {
        result = bufNew(this->bufferSize);

        // Read and handle errors
        ssize_t actualBytes = read(this->handle, bufPtr(result), this->bufferSize);

        // Error occurred during write
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strPtr(this->name));

        // If no data was read then free the buffer and mark the file as EOF
        if (actualBytes == 0)
        {
            this->eof = true;
            bufFree(result);
            result = NULL;
        }
        else
        {
            bufResize(result, (size_t)actualBytes);
            this->size += (size_t)actualBytes;
        }
    }

    FUNCTION_DEBUG_RESULT(BUFFER, result);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileReadPosixClose(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Close the file
        storageFilePosixClose(this->handle, this->name, &FileCloseError);

        this->handle = -1;
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadPosixIgnoreMissing(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->ignoreMissing);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageFileReadPosixName(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->name);
}

/***********************************************************************************************************************************
File size
***********************************************************************************************************************************/
size_t
storageFileReadPosixSize(StorageFileReadPosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_READ_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(SIZE, this->size);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadPosixFree(StorageFileReadPosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ_POSIX, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        storageFileReadPosixClose(this);
        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
