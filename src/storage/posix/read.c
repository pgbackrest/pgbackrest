/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#include <build.h>

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/posix/read.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct StorageReadPosix
{
    const StorageReadInterface *interface;                          // Interface
    StoragePosix *storage;                                          // Storage that created this object

    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)

    int fd;                                                         // File descriptor
    uint64_t current;                                               // Current bytes read from file
    bool eof;

    StoragePosixIoMode ioMode;                                      // I/O mode
    Buffer *dioBuffer;                                              // Aligned bounce buffer
    size_t dioSkip;                                                 // Bytes to skip in first block to reach offset
    size_t dioPos;                                                  // Bytes consumed from bounce buffer
    bool dioEnd;                                                    // Has the end of the file been read?
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
    if (this->ioMode == storagePosixIoModeDirect)
    {
        this->fd = open(strZ(this->name), O_RDONLY | STORAGE_POSIX_DIRECT_FLAG, 0);

        // Fall back to buffered I/O when direct I/O is not supported, e.g. by the file system
        if (this->fd == -1 && errno == EINVAL)
            this->ioMode = storagePosixIoModeDefault;
    }

    if (this->ioMode != storagePosixIoModeDirect)
        this->fd = open(strZ(this->name), O_RDONLY, 0);

    // Handle errors except missing, which is reported to the caller via the result
    if (this->fd == -1)
    {
        if (errno != ENOENT)                                                                                        // {vm_covered}
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(this->name));                          // {vm_covered}
    }
    // Else success
    else
    {
        // Set free callback to ensure the file descriptor is freed
        memContextCallbackSet(objMemContext(this), storageReadPosixFreeResource, this);

        // Seek to offset. Direct I/O can only seek to an aligned offset so seek below the offset
        // and skip the remainder when reading.
        uint64_t seekOffset = this->offset;

        if (this->ioMode == storagePosixIoModeDirect)
        {
            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->dioBuffer = bufNewAlign(STORAGE_POSIX_DIRECT_ALIGN_UP(ioBufferSize()), STORAGE_POSIX_DIRECT_ALIGN);
            }
            MEM_CONTEXT_OBJ_END();

            seekOffset = this->offset / STORAGE_POSIX_DIRECT_ALIGN * STORAGE_POSIX_DIRECT_ALIGN;
            this->dioSkip = (size_t)(this->offset - seekOffset);
        }

        if (seekOffset != 0)
        {
            THROW_ON_SYS_ERROR_FMT(
                lseek(this->fd, (off_t)seekOffset, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, seekOffset,
                strZ(this->name));
        }
    }

    FUNCTION_LOG_RETURN(BOOL, this->fd != -1);
}

/***********************************************************************************************************************************
Read from a file using direct I/O. Data is read in aligned blocks into the bounce buffer and then copied to the caller's buffer.
***********************************************************************************************************************************/
static size_t
storageReadPosixDirect(StorageReadPosix *const this, Buffer *const buffer)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    const size_t usedBegin = bufUsed(buffer);

    if (!this->eof)
    {
        const uint64_t limit = this->limit == NULL ? UINT64_MAX : varUInt64(this->limit);

        while (bufRemains(buffer) > 0 && this->current < limit)
        {
            // Fill the bounce buffer when it has been consumed
            if (this->dioPos == bufUsed(this->dioBuffer))
            {
                if (this->dioEnd)
                    break;

                const ssize_t actualBytes = read(this->fd, bufPtr(this->dioBuffer), bufSize(this->dioBuffer));

                if (actualBytes == -1)
                    THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->name));

                bufUsedSet(this->dioBuffer, (size_t)actualBytes);
                this->dioPos = this->dioSkip < bufUsed(this->dioBuffer) ? this->dioSkip : bufUsed(this->dioBuffer);
                this->dioSkip = 0;

                // A short read means the end of the file has been reached
                if (!bufFull(this->dioBuffer))
                    this->dioEnd = true;
            }

            // Copy from the bounce buffer without exceeding the limit
            size_t copyBytes = bufUsed(this->dioBuffer) - this->dioPos;

            if (copyBytes > bufRemains(buffer))
                copyBytes = bufRemains(buffer);

            if (this->current + copyBytes > limit)
                copyBytes = (size_t)(limit - this->current);

            bufCatSub(buffer, this->dioBuffer, this->dioPos, copyBytes);
            this->dioPos += copyBytes;
            this->current += copyBytes;
        }

        if (this->current == limit || (this->dioEnd && this->dioPos == bufUsed(this->dioBuffer)))
            this->eof = true;
    }

    FUNCTION_LOG_RETURN(SIZE, bufUsed(buffer) - usedBegin);
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
        (void)block;                                                // Blocking is handled by the caller
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->fd != -1);
    ASSERT(buffer != NULL && !bufFull(buffer));

    if (this->ioMode == storagePosixIoModeDirect)
    {
        FUNCTION_LOG_RETURN(SIZE, storageReadPosixDirect(this, buffer));
    }

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        // Rather than enable/disable limit checking just use a big number when there is no limit. We can feel pretty confident that
        // no files will be > UINT64_MAX in size.
        const uint64_t limit = this->limit == NULL ? UINT64_MAX : varUInt64(this->limit);

        // Determine expected bytes to read. If remaining size in the buffer would exceed the limit then reduce the expected read.
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > limit)
            expectedBytes = (size_t)(limit - this->current);

        // Read from file
        actualBytes = read(this->fd, bufRemainsPtr(buffer), expectedBytes);

        // Error occurred during read
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->name));

        // Update amount of buffer used
        bufUsedInc(buffer, (size_t)actualBytes);
        this->current += (uint64_t)actualBytes;

        // If less data than expected was read or the limit has been reached then EOF. The file may not actually be EOF but we are
        // not concerned with files that are growing. Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes || this->current == limit)
            this->eof = true;
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
    const StoragePosixIoMode ioMode)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
        FUNCTION_LOG_PARAM(ENUM, ioMode);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadPosix, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageReadPosix)
        {
            .interface = &storageReadPosixInterface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
            .fd = -1,
            .ioMode = ioMode,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_POSIX, this);
}
