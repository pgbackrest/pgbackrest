/***********************************************************************************************************************************
Storage File Read Driver For Posix
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/assert.h"
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
    StorageFileReadPosix *this = NULL;

    ASSERT_DEBUG(name != NULL);
    ASSERT_DEBUG(bufferSize > 0);

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

    return this;
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageFileReadPosixOpen(StorageFileReadPosix *this)
{
    bool result = false;

    ASSERT_DEBUG(this != NULL);
    ASSERT_DEBUG(this->handle == -1);

    // Open the file and handle errors
    this->handle = storageFilePosixOpen(this->name, O_RDONLY, 0, this->ignoreMissing, &FileOpenError, "read");

    // On success set free callback to ensure file handle is freed
    if (this->handle != -1)
    {
        memContextCallback(this->memContext, (MemContextCallback)storageFileReadPosixFree, this);
        result = true;
    }

    return result;
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
Buffer *
storageFileReadPosix(StorageFileReadPosix *this)
{
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

    return result;
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileReadPosixClose(StorageFileReadPosix *this)
{
    ASSERT_DEBUG(this != NULL);

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Close the file
        storageFilePosixClose(this->handle, this->name, &FileCloseError);

        this->handle = -1;
    }
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadPosixIgnoreMissing(StorageFileReadPosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->ignoreMissing;
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageFileReadPosixName(StorageFileReadPosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->name;
}

/***********************************************************************************************************************************
File size
***********************************************************************************************************************************/
size_t
storageFileReadPosixSize(StorageFileReadPosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->size;
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadPosixFree(StorageFileReadPosix *this)
{
    if (this != NULL)
    {
        storageFileReadPosixClose(this);
        memContextFree(this->memContext);
    }
}
