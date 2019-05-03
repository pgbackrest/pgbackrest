/***********************************************************************************************************************************
Posix Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>

#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "storage/posix/common.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
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
            storagePosixFileSync(this->handle, this->nameTmp, true, false);

        // Close the file
        storagePosixFileClose(this->handle, this->nameTmp, true);

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
            storagePosixPathSync(this->storage, this->path, false);

        // This marks the file as closed
        this->handle = -1;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Free object
***********************************************************************************************************************************/
static void
storageWritePosixFree(StorageWritePosix *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    if (this != NULL)
    {
        memContextCallbackClear(this->memContext);

        // Close the temp file.  *Close() must be called explicitly in order for the file to be sycn'ed, renamed, etc.  If *Free()
        // is called first the assumption is that some kind of error occurred and we should only close the handle to free
        // resources.
        if (this->handle != -1)
            storagePosixFileClose(this->handle, this->interface.name, true);

        memContextFree(this->memContext);
    }

    FUNCTION_LOG_RETURN_VOID();
}

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

    // Open the file and handle errors
    this->handle = storagePosixFileOpen(
        this->nameTmp, FILE_OPEN_FLAGS, this->interface.modeFile, this->interface.createPath, true, FILE_OPEN_PURPOSE);

    // If path is missing
    if (this->handle == -1)
    {
        // Create the path
        storagePosixPathCreate(this->storage, this->path, false, false, this->interface.modePath);

        // Try the open again
        this->handle = storagePosixFileOpen(
            this->nameTmp, FILE_OPEN_FLAGS, this->interface.modeFile, false, true, FILE_OPEN_PURPOSE);
    }

    // Set free callback to ensure file handle is freed
    memContextCallback(this->memContext, (MemContextCallback)storageWritePosixFree, this);

    // Update user/group owner
    if (this->interface.user != NULL || this->interface.group != NULL)
    {
        struct passwd *userData = NULL;
        struct group *groupData = NULL;

        if (this->interface.user != NULL)
        {
            THROW_ON_SYS_ERROR_FMT(
                (userData = getpwnam(strPtr(this->interface.user))) == NULL, UserMissingError, "unable to find user '%s'",
                strPtr(this->interface.user));
        }

        if (this->interface.group != NULL)
        {
            THROW_ON_SYS_ERROR_FMT(
                (groupData = getgrnam(strPtr(this->interface.group))) == NULL, GroupMissingError, "unable to find group '%s'",
                strPtr(this->interface.group));
        }

        THROW_ON_SYS_ERROR_FMT(
            chown(
                strPtr(this->nameTmp), userData != NULL ? userData->pw_uid : (uid_t)-1,
                groupData != NULL ? groupData->gr_gid : (gid_t)-1) == -1,
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
    if (write(this->handle, bufPtr(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strPtr(this->nameTmp));

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

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
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
        FUNCTION_LOG_PARAM(INT64, timeModified);
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
        driver->memContext = MEM_CONTEXT_NEW();

        driver->interface = (StorageWriteInterface)
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
        };

        driver->storage = storage;
        driver->nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strPtr(name)) : driver->interface.name;
        driver->path = strPath(name);
        driver->handle = -1;

        this = storageWriteNew(driver, &driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}
