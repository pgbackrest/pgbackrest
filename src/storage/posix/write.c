/***********************************************************************************************************************************
Posix Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>

#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/user.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_WRITE_POSIX_TYPE                                    StorageWritePosix
#define STORAGE_WRITE_POSIX_PREFIX                                  storageWritePosix

typedef struct StorageWritePosix
{
    MemContext *memContext;                                         // Object mem context
    StorageWriteInterface interface;                                // Interface
    StoragePosix *storage;                                          // Storage that created this object

    const String *nameTmp;
    const String *path;
    int handle;
} StorageWritePosix;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_POSIX_TYPE                                                                                      \
    StorageWritePosix *
#define FUNCTION_LOG_STORAGE_WRITE_POSIX_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageWritePosix", buffer, bufferSize)

/***********************************************************************************************************************************
File open constants

Since open is called more than once use constants to make sure these parameters are always the same
***********************************************************************************************************************************/
#define FILE_OPEN_FLAGS                                             (O_CREAT | O_TRUNC | O_WRONLY)
#define FILE_OPEN_PURPOSE                                           "write"

/***********************************************************************************************************************************
Close file handle
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(STORAGE_WRITE_POSIX, LOG, logLevelTrace)
{
    THROW_ON_SYS_ERROR_FMT(close(this->handle) == -1, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strPtr(this->nameTmp));
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWritePosixOpen(THIS_VOID)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->handle == -1);

    // Open the file
    this->handle = open(strPtr(this->nameTmp), FILE_OPEN_FLAGS, this->interface.modeFile);

    // Attempt the create the path if it is missing
    if (this->handle == -1 && errno == ENOENT && this->interface.createPath)
    {
         // Create the path
        storageInterfacePathCreateP(this->storage, this->path, false, false, this->interface.modePath);

        // Open file again
        this->handle = open(strPtr(this->nameTmp), FILE_OPEN_FLAGS, this->interface.modeFile);
    }

    // Handle errors
    if (this->handle == -1)
    {
        if (errno == ENOENT)
            THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strPtr(this->interface.name));
        else
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strPtr(this->interface.name));
    }

    // Set free callback to ensure file handle is freed
    memContextCallbackSet(this->memContext, storageWritePosixFreeResource, this);

    // Update user/group owner
    if (this->interface.user != NULL || this->interface.group != NULL)
    {
        uid_t updateUserId = userIdFromName(this->interface.user);

        if (updateUserId == userId())
            updateUserId = (uid_t)-1;

        gid_t updateGroupId = groupIdFromName(this->interface.group);

        if (updateGroupId == groupId())
            updateGroupId = (gid_t)-1;

        THROW_ON_SYS_ERROR_FMT(
            chown(strPtr(this->nameTmp), updateUserId, updateGroupId) == -1,
            FileOwnerError, "unable to set ownership for '%s'", strPtr(this->nameTmp));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWritePosix(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->handle != -1);

    // Write the data
    if (write(this->handle, bufPtrConst(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strPtr(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWritePosixClose(THIS_VOID)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Sync the file
        if (this->interface.syncFile)
            THROW_ON_SYS_ERROR_FMT(fsync(this->handle) == -1, FileSyncError, STORAGE_ERROR_WRITE_SYNC, strPtr(this->nameTmp));

        // Close the file
        memContextCallbackClear(this->memContext);
        THROW_ON_SYS_ERROR_FMT(close(this->handle) == -1, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strPtr(this->nameTmp));
        this->handle = -1;

        // Update modified time
        if (this->interface.timeModified != 0)
        {
            THROW_ON_SYS_ERROR_FMT(
                utime(
                    strPtr(this->nameTmp),
                    &((struct utimbuf){.actime = this->interface.timeModified, .modtime = this->interface.timeModified})) == -1,
                FileInfoError, "unable to set time for '%s'", strPtr(this->nameTmp));
        }

        // Rename from temp file
        if (this->interface.atomic)
        {
            if (rename(strPtr(this->nameTmp), strPtr(this->interface.name)) == -1)
            {
                THROW_SYS_ERROR_FMT(
                    FileMoveError, "unable to move '%s' to '%s'", strPtr(this->nameTmp), strPtr(this->interface.name));
            }
        }

        // Sync the path
        if (this->interface.syncPath)
            storageInterfacePathSyncP(this->storage, this->path);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get handle (file descriptor)
***********************************************************************************************************************************/
static int
storageWritePosixHandle(const THIS_VOID)
{
    THIS(const StorageWritePosix);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->handle);
}

/**********************************************************************************************************************************/
StorageWrite *
storageWritePosixNew(
    StoragePosix *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(TIME, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    StorageWrite *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageWritePosix")
    {
        StorageWritePosix *driver = memNew(sizeof(StorageWritePosix));

        *driver = (StorageWritePosix)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .storage = storage,
            .path = strPath(name),
            .handle = -1,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_POSIX_TYPE_STR,
                .name = strDup(name),
                .atomic = atomic,
                .createPath = createPath,
                .group = strDup(group),
                .modeFile = modeFile,
                .modePath = modePath,
                .syncFile = syncFile,
                .syncPath = syncPath,
                .user = strDup(user),
                .timeModified = timeModified,

                .ioInterface = (IoWriteInterface)
                {
                    .close = storageWritePosixClose,
                    .handle = storageWritePosixHandle,
                    .open = storageWritePosixOpen,
                    .write = storageWritePosix,
                },
            },
        };

        // Create temp file name
        driver->nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strPtr(name)) : driver->interface.name;

        this = storageWriteNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
