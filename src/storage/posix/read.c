/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct StorageReadPosix
{
    StorageReadInterface interface;                                 // Interface
    StoragePosix *storage;                                          // Storage that created this object

    int fd;                                                         // File descriptor
    uint64_t current;                                               // Current bytes read from file
    uint64_t limit;                                                 // Limit bytes to be read from file (UINT64_MAX for no limit)
    bool eof;
} StorageReadPosix;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_POSIX_TYPE                                                                                       \
    StorageReadPosix *
#define FUNCTION_LOG_STORAGE_READ_POSIX_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(value, "StorageReadPosix", buffer, bufferSize)

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
        THROW_ON_SYS_ERROR_FMT(close(this->fd) == -1, FileCloseError, STORAGE_ERROR_READ_CLOSE, strZ(this->interface.name));

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

    bool result = false;

    // Open the file
    this->fd = open(strZ(this->interface.name), O_RDONLY, 0);

    // Handle errors
    if (this->fd == -1)
    {
        if (errno == ENOENT)                                                                                        // {vm_covered}
        {
            if (!this->interface.ignoreMissing)
                THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(this->interface.name));
        }
        else
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_READ_OPEN, strZ(this->interface.name));                // {vm_covered}
    }

    // On success set free callback to ensure the file descriptor is freed
    if (this->fd != -1)
    {
        memContextCallbackSet(THIS_MEM_CONTEXT(), storageReadPosixFreeResource, this);
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadPosix(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageReadPosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->fd != -1);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        // Determine expected bytes to read. If remaining size in the buffer would exceed the limit then reduce the expected read.
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > this->limit)
            expectedBytes = (size_t)(this->limit - this->current);

        // Read from file
        actualBytes = read(this->fd, bufRemainsPtr(buffer), expectedBytes);

        // Error occurred during read
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->interface.name));

        // Update amount of buffer used
        bufUsedInc(buffer, (size_t)actualBytes);
        this->current += (uint64_t)actualBytes;

        // If less data than expected was read or the limit has been reached then EOF.  The file may not actually be EOF but we are
        // not concerned with files that are growing.  Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes || this->current == this->limit)
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

    memContextCallbackClear(THIS_MEM_CONTEXT());
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

    FUNCTION_TEST_RETURN(this->eof);
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

    FUNCTION_TEST_RETURN(this->fd);
}

/**********************************************************************************************************************************/
StorageRead *
storageReadPosixNew(StoragePosix *storage, const String *name, bool ignoreMissing, const Variant *limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageReadPosix)
    {
        StorageReadPosix *driver = OBJ_NEW_ALLOC();

        *driver = (StorageReadPosix)
        {
            .storage = storage,
            .fd = -1,

            // Rather than enable/disable limit checking just use a big number when there is no limit.  We can feel pretty confident
            // that no files will be > UINT64_MAX in size. This is a copy of the interface limit but it simplifies the code during
            // read so it seems worthwhile.
            .limit = limit == NULL ? UINT64_MAX : varUInt64(limit),

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_POSIX_TYPE,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .limit = varDup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .close = storageReadPosixClose,
                    .eof = storageReadPosixEof,
                    .fd = storageReadPosixFd,
                    .open = storageReadPosixOpen,
                    .read = storageReadPosix,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
