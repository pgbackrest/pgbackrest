/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/memContext.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage structure
***********************************************************************************************************************************/
struct Storage
{
    const String *path;
    int mode;
    size_t bufferSize;
};

// Static definition of local storage
static const Storage *storageLocalData = NULL;

/***********************************************************************************************************************************
Get a local storage object
***********************************************************************************************************************************/
const Storage *
storageLocal()
{
    if (storageLocalData == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("storage")
            {
                storageLocalData = (Storage *)memNew(sizeof(Storage));
                ((Storage *)storageLocalData)->path = strNew("/");
                ((Storage *)storageLocalData)->mode = 0750;
                ((Storage *)storageLocalData)->bufferSize = 65536;
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    return storageLocalData;
}

/***********************************************************************************************************************************
Check for errors on read.  This is a separate function for testing purposes.
***********************************************************************************************************************************/
static void
storageReadError(ssize_t actualBytes, const String *file)
{
    // Error occurred during write
    if (actualBytes == -1)
    {
        int errNo = errno;
        THROW(FileReadError, "unable to read '%s': %s", strPtr(file), strerror(errNo));
    }
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *storageGet(const Storage *storage, const String *file, bool ignoreMissing)
{
    Buffer volatile *result = NULL;
    volatile int fileHandle = -1;

    TRY_BEGIN()
    {
        // Open the file and handle errors
        fileHandle = open(strPtr(file), O_RDONLY, storage->mode);

        if (fileHandle == -1)
        {
            int errNo = errno;

            if (!ignoreMissing || errNo != ENOENT)
                THROW(FileOpenError, "unable to open '%s' for read: %s", strPtr(file), strerror(errNo));
        }
        else
        {
            // Create result buffer with buffer size
            ssize_t actualBytes = 0;
            size_t totalBytes = 0;

            do
            {
                // Allocate the buffer before first read
                if (result == NULL)
                    result = bufNew(storage->bufferSize);
                // Grow the buffer on subsequent reads
                else
                    bufResize((Buffer *)result, bufSize((Buffer *)result) + (size_t)storage->bufferSize);

                // Read and handle errors
                actualBytes = read(fileHandle, bufPtr((Buffer *)result) + totalBytes, storage->bufferSize);
                storageReadError(actualBytes, file);

                // Track total bytes read
                totalBytes += (size_t)actualBytes;
            }
            while (actualBytes != 0);

            // Resize buffer to total bytes read
            bufResize((Buffer *)result, totalBytes);
        }
    }
    CATCH_ANY()
    {
        // Free buffer on error if it was allocated
        if (result != NULL)
            bufFree((Buffer *)result);                              // {uncoverable - cannot error after buffer is allocated}

        RETHROW();
    }
    FINALLY()
    {
        // Close file
        if (fileHandle != -1)
            close(fileHandle);
    }
    TRY_END();

    return (Buffer *)result;
}

/***********************************************************************************************************************************
Check for errors on write.  This is a separate function for testing purposes.
***********************************************************************************************************************************/
static void
storageWriteError(ssize_t actualBytes, size_t expectedBytes, const String *file)
{
    // Error occurred during write
    if (actualBytes == -1)
    {
        int errNo = errno;
        THROW(FileWriteError, "unable to write '%s': %s", strPtr(file), strerror(errNo));
    }

    // Make sure that all expected bytes were written.  Cast to unsigned since we have already tested for -1.
    if ((size_t)actualBytes != expectedBytes)
        THROW(FileWriteError, "only wrote %lu byte(s) to '%s' but %lu byte(s) expected", actualBytes, strPtr(file), expectedBytes);
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void storagePut(const Storage *storage, const String *file, const Buffer *buffer)
{
    volatile int fileHandle = -1;

    TRY_BEGIN()
    {
        // Open the file and handle errors
        fileHandle = open(strPtr(file), O_CREAT | O_TRUNC | O_WRONLY, storage->mode);

        if (fileHandle == -1)
        {
            int errNo = errno;
            THROW(FileOpenError, "unable to open '%s' for write: %s", strPtr(file), strerror(errNo));
        }

        // Write data if buffer is not null.  Otherwise, and empty file is expected.
        if (buffer != NULL)
            storageWriteError(write(fileHandle, bufPtr(buffer), bufSize(buffer)), bufSize(buffer), file);
    }
    FINALLY()
    {
        if (fileHandle != -1)
            close(fileHandle);
    }
    TRY_END();
}
