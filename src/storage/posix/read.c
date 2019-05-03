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
#include "storage/posix/common.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct StorageReadPosix
{
    MemContext *memContext;                                         // Object mem context
    StorageReadInterface interface;                                 // Interface
    StoragePosix *storage;                                          // Storage that created this object

    int handle;
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

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Close the file
        storagePosixFileClose(this->handle, this->interface.name, true);

        this->handle = -1;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
static void
storageReadPosixFree(StorageReadPosix *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_POSIX, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        storageReadPosixClose(this);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

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
    ASSERT(this->handle == -1);

    bool result = false;

    // Open the file and handle errors
        this->handle = storagePosixFileOpen(this->interface.name, O_RDONLY, 0, this->interface.ignoreMissing, true, "read");

    // On success set free callback to ensure file handle is freed
    if (this->handle != -1)
    {
        memContextCallbackSet(this->memContext, (MemContextCallback)storageReadPosixFree, this);
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
        actualBytes = read(this->handle, bufRemainsPtr(buffer), expectedBytes);

        // Error occurred during read
        if (actualBytes == -1)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strPtr(this->interface.name));

        // Update amount of buffer used
        bufUsedInc(buffer, (size_t)actualBytes);

        // If less data than expected was read then EOF.  The file may not actually be EOF but we are not concerned with files that
        // are growing.  Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes)
            this->eof = true;
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
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
storageReadPosixNew(StoragePosix *storage, const String *name, bool ignoreMissing)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    StorageRead *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageReadPosix")
    {
        StorageReadPosix *driver = memNew(sizeof(StorageReadPosix));
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageReadInterface)
        {
            .type = STORAGE_POSIX_TYPE_STR,
            .name = strDup(name),
            .ignoreMissing = ignoreMissing,

            .ioInterface = (IoReadInterface)
            {
                .eof = storageReadPosixEof,
                .handle = storageReadPosixHandle,
                .open = storageReadPosixOpen,
                .read = storageReadPosix,
            },
        };

        driver->storage = storage;
        driver->handle = -1;

        this = storageReadNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}
