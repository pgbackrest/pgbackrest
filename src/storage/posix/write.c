/***********************************************************************************************************************************
Posix Storage File write
***********************************************************************************************************************************/
#include <build.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>

#include "common/debug.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/user.h"
#include "storage/posix/direct.h"
#include "storage/posix/write.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageWritePosix
{
    const StorageWriteInterface *interface;                          // Interface
    StoragePosix *storage;                                           // Storage that created this object

    const String *name;                                              // File name
    mode_t modeFile;                                                 // File mode
    mode_t modePath;                                                 // Path mode
    bool createPath;                                                 // Create path if missing
    bool atomic;                                                     // Atomic write
    bool truncate;                                                   // Truncate existing file
    bool syncPath;                                                   // Sync path after write
    bool syncFile;                                                   // Sync file after write
    const String *user;                                              // User owner
    const String *group;                                             // Group owner
    time_t timeModified;                                             // Modified time

    const String *nameTmp;
    const String *path;
    int fd;                                                          // File descriptor

    StoragePosixIoMode ioMode;                                       // I/O mode
    StoragePosixDirect dio;                                          // Aligned bounce buffer
    size_t dioUsed;                                                  // Bytes in bounce buffer
    bool dioPadded;                                                  // Was padding written that needs to be truncated?
    uint64_t position;                                               // Logical write position
    uint64_t size;                                                   // Logical size of the data written
};

/***********************************************************************************************************************************
File open constants

Since open is called more than once use constants to make sure these parameters are always the same
***********************************************************************************************************************************/
#define FILE_OPEN_PURPOSE                                           "write"

/***********************************************************************************************************************************
Close file descriptor
***********************************************************************************************************************************/
static void
storageWritePosixFreeResource(THIS_VOID)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    THROW_ON_SYS_ERROR_FMT(close(this->fd) == -1, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static const char *
storageWritePosixOpenOwner(const String *const owner, const char *const defaultOwner)
{
    return owner == NULL ? defaultOwner : strZ(owner);
}

static const char *
storageWritePosixOpenOwnerId(const String *const owner, const unsigned int ownerId)
{
    return owner == NULL ? "" : zNewFmt("[%u]", ownerId);
}

static void
storageWritePosixOpen(THIS_VOID)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->fd == -1);

    // Open the file
    const int flags = O_CREAT | O_WRONLY | (this->truncate ? O_TRUNC : 0);
    const int flagsOpen = flags | (this->ioMode == storagePosixIoModeDirect ? storagePosixDirectFlag() : 0);

    this->fd = open(strZ(this->nameTmp), flagsOpen, this->modeFile);

    // Attempt to create the path if it is missing
    if (this->fd == -1 && errno == ENOENT && this->createPath)                                                      // {vm_covered}
    {
        // Create the path
        storageInterfacePathCreateP(this->storage, this->path, false, false, this->modePath);

        // Open file again
        this->fd = open(strZ(this->nameTmp), flagsOpen, this->modeFile);
    }

    // Fall back to buffered I/O when direct I/O is not supported, e.g. by the file system
    if (this->fd == -1 && errno == EINVAL && this->ioMode == storagePosixIoModeDirect)
    {
        this->ioMode = storagePosixIoModeDefault;
        this->fd = open(strZ(this->nameTmp), flags, this->modeFile);
    }

    // Handle errors
    if (this->fd == -1)
    {
        if (errno == ENOENT)                                                                                        // {vm_covered}
            THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->name));
        else
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->name));                         // {vm_covered}
    }

    // Set free callback to ensure the file descriptor is freed
    memContextCallbackSet(objMemContext(this), storageWritePosixFreeResource, this);

    if (this->ioMode == storagePosixIoModeDirect)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            storagePosixDirectNew(this->fd, &this->dio);
        }
        MEM_CONTEXT_OBJ_END();
    }

    // Update user/group owner
    if (this->user != NULL || this->group != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const StorageInfo info = storageInterfaceInfoP(
                this->storage, this->nameTmp, storageInfoLevelDetail, .followLink = true);
            ASSERT(info.exists);
            uid_t updateUserId = userIdFromName(this->user);
            gid_t updateGroupId = groupIdFromName(this->group);

            if (updateUserId == (uid_t)-1)
                updateUserId = info.userId;

            if (updateGroupId == (gid_t)-1)
                updateGroupId = info.groupId;

            // Continue if one of the owners would be changed
            if (updateUserId != info.userId || updateGroupId != info.groupId)
            {
                THROW_ON_SYS_ERROR_FMT(
                    chown(strZ(this->nameTmp), updateUserId, updateGroupId) == -1, FileOwnerError,
                    "unable to set ownership for '%s' to %s%s:%s%s from %s[%u]:%s[%u]", strZ(this->nameTmp),
                    storageWritePosixOpenOwner(this->user, "[none]"),
                    storageWritePosixOpenOwnerId(this->user, updateUserId),
                    storageWritePosixOpenOwner(this->group, "[none]"),
                    storageWritePosixOpenOwnerId(this->group, updateGroupId),
                    storageWritePosixOpenOwner(info.user, "[unknown]"), info.userId,
                    storageWritePosixOpenOwner(info.group, "[unknown]"), info.groupId);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write the bounce buffer to the file. A partial block is padded with zeros to the alignment and the padding is truncated on close.
***********************************************************************************************************************************/
static void
storageWritePosixDirectFlush(StorageWritePosix *const this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->dioUsed > 0)
    {
        const size_t writeSize = (this->dioUsed + this->dio.align - 1) / this->dio.align * this->dio.align;

        if (writeSize != this->dioUsed)
        {
            memset(this->dio.ptr + this->dioUsed, 0, writeSize - this->dioUsed);
            this->dioPadded = true;
        }

        if (write(this->fd, this->dio.ptr, writeSize) != (ssize_t)writeSize)
            THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strZ(this->nameTmp));

        this->dioUsed = 0;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWritePosix(THIS_VOID, const Buffer *const buffer)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    ASSERT(this->fd != -1);

    // Stage the data through the bounce buffer when using direct I/O
    if (this->ioMode == storagePosixIoModeDirect)
    {
        const uint8_t *bufferPtr = bufPtrConst(buffer);
        size_t bufferRemains = bufUsed(buffer);

        while (bufferRemains > 0)
        {
            const size_t space = this->dio.size - this->dioUsed;
            const size_t copyBytes = bufferRemains < space ? bufferRemains : space;

            memcpy(this->dio.ptr + this->dioUsed, bufferPtr, copyBytes);
            this->dioUsed += copyBytes;
            bufferPtr += copyBytes;
            bufferRemains -= copyBytes;

            if (this->dioUsed == this->dio.size)
                storageWritePosixDirectFlush(this);
        }

        this->position += bufUsed(buffer);

        if (this->position > this->size)
            this->size = this->position;
    }
    // Write the data
    else if (write(this->fd, bufPtrConst(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strZ(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Seek to specified position relative to beginning of file
***********************************************************************************************************************************/
static void
storageWritePosixSeek(THIS_VOID, const uint64_t position)
{
    THIS(StorageWritePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_POSIX, this);
        FUNCTION_LOG_PARAM(UINT64, position);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->fd != -1);

    // Direct I/O can only seek to an aligned position
    if (this->ioMode == storagePosixIoModeDirect)
    {
        storageWritePosixDirectFlush(this);

        if (position % this->dio.align != 0)
        {
            THROW_FMT(
                FileWriteError, "unable to seek to %" PRIu64 " in '%s': position is not aligned for direct I/O", position,
                strZ(this->nameTmp));
        }

        this->position = position;
    }

    THROW_ON_SYS_ERROR_FMT(
        lseek(this->fd, (off_t)position, SEEK_SET) == -1, FileWriteError, STORAGE_ERROR_WRITE_SEEK, position, strZ(this->nameTmp));

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
    if (this->fd != -1)
    {
        // Write remaining data and remove the padding that was added to meet the alignment
        if (this->ioMode == storagePosixIoModeDirect)
        {
            storageWritePosixDirectFlush(this);

            if (this->dioPadded)
            {
                THROW_ON_SYS_ERROR_FMT(
                    ftruncate(this->fd, (off_t)this->size) == -1, FileWriteError, "unable to truncate '%s'",
                    strZ(this->nameTmp));
            }
        }

        // Sync the file
        if (this->syncFile)
            THROW_ON_SYS_ERROR_FMT(fsync(this->fd) == -1, FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(this->nameTmp));

        // Close the file
        memContextCallbackClear(objMemContext(this));
        THROW_ON_SYS_ERROR_FMT(close(this->fd) == -1, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));
        this->fd = -1;

        // Update modified time
        if (this->timeModified != 0)
        {
            THROW_ON_SYS_ERROR_FMT(
                utime(
                    strZ(this->nameTmp),
                    &((struct utimbuf){.actime = this->timeModified, .modtime = this->timeModified})) == -1,
                FileInfoError, "unable to set time for '%s'", strZ(this->nameTmp));
        }

        // Rename from temp file
        if (this->atomic)
        {
            if (rename(strZ(this->nameTmp), strZ(this->name)) == -1)
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->name));
        }

        // Sync the path
        if (this->syncPath)
            storageInterfacePathSyncP(this->storage, this->path);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
storageWritePosixFd(const THIS_VOID)
{
    THIS(const StorageWritePosix);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE_POSIX, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(INT, this->fd);
}

/**********************************************************************************************************************************/
static const StorageWriteInterface storageWritePosixInterface =
{
    .close = storageWritePosixClose,
    .fd = storageWritePosixFd,
    .open = storageWritePosixOpen,
    .seek = storageWritePosixSeek,
    .write = storageWritePosix,
};

FN_EXTERN StorageWritePosix *
storageWritePosixNew(
    StoragePosix *const storage, const String *const name, const mode_t modeFile, const mode_t modePath, const String *const user,
    const String *const group, const time_t timeModified, const bool createPath, const bool syncFile, const bool syncPath,
    const bool atomic, const bool truncate, const StoragePosixIoMode ioMode)
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
        FUNCTION_LOG_PARAM(BOOL, truncate);
        FUNCTION_LOG_PARAM(ENUM, ioMode);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(name != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    OBJ_NEW_BEGIN(StorageWritePosix, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (StorageWritePosix)
        {
            .interface = &storageWritePosixInterface,
            .storage = storage,
            .name = strDup(name),
            .modeFile = modeFile,
            .modePath = modePath,
            .createPath = createPath,
            .atomic = atomic,
            .truncate = truncate,
            .syncFile = syncFile,
            .syncPath = syncPath,
            .user = strDup(user),
            .group = strDup(group),
            .timeModified = timeModified,
            .nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(name)) : strDup(name),
            .path = strPath(name),
            .fd = -1,
            .ioMode = ioMode,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE_POSIX, this);
}
