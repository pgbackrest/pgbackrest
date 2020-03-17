/***********************************************************************************************************************************
Posix Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/read.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
#define STORAGE_READ_POSIX_TYPE                                     StorageReadPosix
#define STORAGE_READ_POSIX_PREFIX                                   storageReadPosix

typedef struct StorageReadPosix
{
    MemContext *memContext;                                         // Object mem context
    StorageReadInterface interface;                                 // Interface
    StoragePosix *storage;                                          // Storage that created this object

    int handle;
    uint64_t current;                                               // Current bytes read from file
    uint64_t limit;                                                 // Limit bytes to be read from file
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
Close the file handle
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(STORAGE_READ_POSIX, LOG, logLevelTrace)
{
    if (this->handle != -1)
        THROW_ON_SYS_ERROR_FMT(close(this->handle) == -1, FileCloseError, STORAGE_ERROR_READ_CLOSE, strPtr(this->interface.name));
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

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
    ASSERT(this->handle == -1);

    bool result = false;

    // Open the file
    this->handle = open(strPtr(this->interface.name), O_RDONLY, 0);

    // Handle errors
    if (this->handle == -1)
    {
        if (errno == ENOENT)
        {
            if (!this->interface.ignoreMissing)
                THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strPtr(this->interface.name));
        }
        else
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_READ_OPEN, strPtr(this->interface.name));
    }
    // On success set free callback to ensure file handle is freed
    if (this->handle != -1)
    {
        memContextCallbackSet(this->memContext, storageReadPosixFreeResource, this);
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

    ASSERT(this != NULL && this->handle != -1);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        // Read and handle errors
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > this->limit)
            expectedBytes = (size_t)(this->limit - this->current);

        actualBytes = read(this->handle, bufRemainsPtr(buffer), expectedBytes);

        // Error occurred during read
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strPtr(this->interface.name));

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

    storageReadPosixFreeResource(this);
    memContextCallbackClear(this->memContext);
    this->handle = -1;

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
Get handle (file descriptor)
***********************************************************************************************************************************/
static int
storageReadPosixHandle(const THIS_VOID)
{
    THIS(const StorageReadPosix);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->handle);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
StorageRead *
storageReadPosixNew(StoragePosix *storage, const String *name, bool ignoreMissing, const PrmUInt64 *limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(PRM_UINT64, limit);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    StorageRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageReadPosix")
    {
        StorageReadPosix *driver = memNew(sizeof(StorageReadPosix));

        *driver = (StorageReadPosix)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .handle = -1,

            // Rather than enable/disable limit checking just always use a limit
            .limit = limit == NULL ? UINT64_MAX : prmUInt64(limit),

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_POSIX_TYPE_STR,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .limit = prmUInt64Dup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .close = storageReadPosixClose,
                    .eof = storageReadPosixEof,
                    .handle = storageReadPosixHandle,
                    .open = storageReadPosixOpen,
                    .read = storageReadPosix,
                },
            },
        };

        this = storageReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
