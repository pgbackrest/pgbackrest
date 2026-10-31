/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#include <build.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/posix/read.h"
#include "storage/posixDio/storage.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct StorageReadPosix
{
    const StorageReadInterface *interface; // Interface
    StoragePosix *storage;                 // Storage that created this object

    const String *name;   // File name
    uint64_t offset;      // Read offset
    const Variant *limit; // Read limit (NULL for no limit)

    int fd;           // File descriptor
    uint64_t current; // Current bytes read from file
    bool eof;

    bool directIo;        // Use direct I/O
    uint8_t *dioBuffer;   // Aligned bounce buffer for direct I/O
    size_t dioBufferSize; // Size of aligned buffer
    size_t dioBufferUsed; // Valid bytes currently in buffer
    size_t dioBufferPos;  // Current read position in buffer
};

/***********************************************************************************************************************************
Close file descriptor
***********************************************************************************************************************************/
static void
storageReadPosixFreeResource(THIS_VOID)
{
    THIS(StorageReadPosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->dioBuffer != NULL)
    {
        free(this->dioBuffer);
        this->dioBuffer = NULL;
    }

    if (this->fd != -1)
        THROW_ON_SYS_ERROR_FMT(close(this->fd) == -1, FileCloseError, STORAGE_ERROR_READ_CLOSE, strZ(this->name));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadPosixOpen(THIS_VOID)
{
    THIS(StorageReadPosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->fd == -1);

    // Open the file
    int flags = O_RDONLY;

#ifdef O_DIRECT
    if (this->directIo)
        flags |= O_DIRECT;
#endif

    this->fd = open(strZ(this->name), flags, 0);

    // Handle errors except missing, which is reported to the caller via the result
    if (this->fd == -1)
    {
        if (errno != ENOENT)
        {
#ifdef O_DIRECT
            if (this->directIo && errno == EINVAL)
            {
                THROW_SYS_ERROR_FMT(
                    FeatureNotSupportedError, "unable to open '%s' with direct I/O: filesystem does not support direct I/O",
                    strZ(this->name));
            }
#endif
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(this->name));
        }
    }
    // Else success
    else
    {
        // Set free callback to ensure the file descriptor is freed
        memContextCallbackSet(objMemContext(this), storageReadPosixFreeResource, this);

        if (this->directIo)
        {
            this->dioBufferSize = ((ioBufferSize() + STORAGE_POSIX_DIO_ALIGN - 1) / STORAGE_POSIX_DIO_ALIGN) *
                                  STORAGE_POSIX_DIO_ALIGN;

            if (this->dioBufferSize < STORAGE_POSIX_DIO_ALIGN)
                this->dioBufferSize = STORAGE_POSIX_DIO_ALIGN;

            if (posix_memalign((void **)&this->dioBuffer, STORAGE_POSIX_DIO_ALIGN, this->dioBufferSize) != 0)
                THROW_SYS_ERROR_FMT(MemoryError, "unable to allocate direct I/O buffer for '%s'", strZ(this->name));

            // Initialize buffer tracking metrics to empty
            this->dioBufferUsed = 0;
            this->dioBufferPos = 0;

            // Seek to offset
            if (this->offset != 0)
            {
                const uint64_t alignedOffset = (this->offset / STORAGE_POSIX_DIO_ALIGN) * STORAGE_POSIX_DIO_ALIGN;
                const size_t discard = (size_t)(this->offset - alignedOffset);

                THROW_ON_SYS_ERROR_FMT(
                    lseek(this->fd, (off_t)alignedOffset, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK,
                    alignedOffset, strZ(this->name));

                if (discard > 0)
                {
                    const ssize_t bytes = read(this->fd, this->dioBuffer, STORAGE_POSIX_DIO_ALIGN);

                    if (bytes == -1)
                        THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->name));

                    this->dioBufferUsed = (size_t)bytes;

                    if ((size_t)bytes <= discard)
                    {
                        this->dioBufferPos = (size_t)bytes;
                        this->eof = true;
                    }
                    else
                    {
                        this->dioBufferPos = discard;
                    }
                }
            }
        }
        else if (this->offset != 0)
        {
            THROW_ON_SYS_ERROR_FMT(
                lseek(this->fd, (off_t)this->offset, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, this->offset,
                strZ(this->name));
        }
    }

    FUNCTION_LOG_RETURN(BOOL, this->fd != -1);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadPosix(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadPosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        (void)block; // Blocking is handled by the caller
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->fd != -1);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        const uint64_t limit = this->limit == NULL ? UINT64_MAX : varUInt64(this->limit);

        if (!this->directIo)
        {
            size_t expectedBytes = bufRemains(buffer);

            if (this->current + expectedBytes > limit)
                expectedBytes = (size_t)(limit - this->current);

            actualBytes = read(this->fd, bufRemainsPtr(buffer), expectedBytes);

            if (actualBytes == -1)
                THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->name));

            bufUsedInc(buffer, (size_t)actualBytes);
            this->current += (uint64_t)actualBytes;

            if ((size_t)actualBytes != expectedBytes || this->current == limit)
                this->eof = true;
        }
        else
        {
            // The destination buffer could be full or size limit hit right from the start
            while (bufRemains(buffer) > 0 && this->current < limit)
            {
                // 1. If there is data in the cache buffer, drain it out systematically
                if (this->dioBufferPos < this->dioBufferUsed)
                {
                    const size_t bytesAvailable = this->dioBufferUsed - this->dioBufferPos;
                    size_t bytesToCopy = bufRemains(buffer);

                    if (this->current + bytesToCopy > limit)
                        bytesToCopy = (size_t)(limit - this->current);

                    if (bytesToCopy > bytesAvailable)
                        bytesToCopy = bytesAvailable;

                    memcpy(bufRemainsPtr(buffer), this->dioBuffer + this->dioBufferPos, bytesToCopy);
                    bufUsedInc(buffer, bytesToCopy);
                    this->dioBufferPos += bytesToCopy;
                    this->current += (uint64_t)bytesToCopy;
                    actualBytes += (ssize_t)bytesToCopy;

                    if (this->current == limit)
                        this->eof = true;

                    continue;
                }

                // If our destination buffer is now filled up, exit the loop
                // immediately BEFORE executing another read operation on disk.
                if (bufRemains(buffer) == 0)
                    break;

                // If the underlying file already hit EOF and our cache buffer is completely dry, exit
                if (this->eof)
                    break;

                // Cache buffer is completely empty. Fetch the next aligned block.
                this->dioBufferPos = 0;
                this->dioBufferUsed = 0;

                const ssize_t bytesRead = read(this->fd, this->dioBuffer, this->dioBufferSize);

                if (bytesRead == -1)
                    THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->name));

                if (bytesRead == 0)
                {
                    this->eof = true;
                    break;
                }

                this->dioBufferUsed = (size_t)bytesRead;

                // If less data than the buffer configuration size was returned, the file on disk is finished.
                // We mark eof = true, but do NOT break, allowing the next loop iteration to copy the data.
                if ((size_t)bytesRead < this->dioBufferSize)
                    this->eof = true;
            }

            // If the limit or physical file end is reached, ensure eof state matches
            if (this->current == limit)
                this->eof = true;
        }
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageReadPosixClose(THIS_VOID)
{
    THIS(StorageReadPosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    memContextCallbackClear(objMemContext(this));
    storageReadPosixFreeResource(this);
    this->fd = -1;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadPosixEof(THIS_VOID)
{
    THIS(StorageReadPosix);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->eof);
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
storageReadPosixFd(const THIS_VOID)
{
    THIS(const StorageReadPosix);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, this->fd);
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadPosixInterface =
{
    .close = storageReadPosixClose,
    .eof = storageReadPosixEof,
    .fd = storageReadPosixFd,
    .open = storageReadPosixOpen,
    .read = storageReadPosix,
};

FN_EXTERN StorageReadPosix *
storageReadPosixNew(
    StoragePosix *const storage, const String *const name, const uint64_t offset, const Variant *const limit,
    const bool directIo)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(BOOL, directIo);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadPosix, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageReadPosix){
            .interface = &storageReadPosixInterface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
            .fd = -1,
            .directIo = directIo,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_POSIX, this);
}
