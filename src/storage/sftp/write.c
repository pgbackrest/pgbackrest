/***********************************************************************************************************************************
Sftp Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

//#ifdef HAVE_LIBSSH2

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>

#include "common/debug.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/user.h"
#include "storage/sftp/storage.intern.h"
#include "storage/sftp/write.h"
#include "storage/write.intern.h"

//jrt !!! look at implementing an error checking/handling function - libssh2 session error => sftp session error
//
/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteSftp
{
    StorageWriteInterface interface;                                // Interface
    StorageSftp *storage;                                           // Storage that created this object

    const String *nameTmp;
    const String *path;
    int fd;                                                         // File descriptor
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    LIBSSH2_SFTP_ATTRIBUTES *attr;
} StorageWriteSftp;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_TYPE                                                                                      \
    StorageWriteSftp *
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageWriteSftp", buffer, bufferSize)

/***********************************************************************************************************************************
File open constants

Since open is called more than once use constants to make sure these parameters are always the same
***********************************************************************************************************************************/
#define FILE_OPEN_FLAGS                                             (LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC | LIBSSH2_FXF_WRITE)
#define FILE_OPEN_PURPOSE                                           "write"

/***********************************************************************************************************************************
Close file descriptor
***********************************************************************************************************************************/
static void
storageWriteSftpFreeResource(THIS_VOID)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    //jrt fd becomes handle, libssh2*close()
    THROW_ON_SYS_ERROR_FMT(
        libssh2_sftp_close(this->sftpHandle) != 0, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static void
storageWriteSftpOpen(THIS_VOID)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    //jrt fd becomes handle
    //ASSERT(this->fd == -1);

    // Open the file
    //this->fd = open(strZ(this->nameTmp), FILE_OPEN_FLAGS, this->interface.modeFile);
    //jrt !!! rework file open flags if they don't match LIBSSH2_FXF* flags,
    //same with mode -> see  LIBSSH2_SFTP_S_* convenience defines in <lib‐ssh2_sftp.h>
    this->sftpHandle = libssh2_sftp_open(this->sftpSession, strZ(this->nameTmp), FILE_OPEN_FLAGS, this->interface.modeFile);

    //if (this->fd == -1 && errno == ENOENT && this->interface.createPath)                                 // {vm_covered}
    // Attempt to create the path if it is missing
    if (this->sftpHandle == NULL && this->interface.createPath)                                            // {vm_covered} need explanation  on this
    {
         // Create the path
        storageInterfacePathCreateP(this->storage, this->path, false, false, this->interface.modePath);

        // Open file again
        //jrt fd becomes handle, libssh2*open()
        this->sftpHandle = libssh2_sftp_open(this->sftpSession, strZ(this->nameTmp), FILE_OPEN_FLAGS, this->interface.modeFile);
    }

    // Handle errors
    //jrt ??libssh2 errors and handlers
    if (this->sftpHandle == NULL)
    {
        // If session indicates sftp error, can query for sftp error
        // !!! see also libssh2_session_last_error()
        if(libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_PATH)                        // {vm_covered}
                THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->interface.name));
            else
                THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name));    // {vm_covered}
        }
    }

    // Set free callback to ensure the file descriptor is freed
    memContextCallbackSet(objMemContext(this), storageWriteSftpFreeResource, this);

    // Update user/group owner
    // jrt libssh2_sftp_setstat() libssh2_sftp_fsetstat()
    if (this->interface.user != NULL || this->interface.group != NULL)
    {
        this->attr->flags = LIBSSH2_SFTP_ATTR_UIDGID;

        this->attr->uid = userIdFromName(this->interface.user);

        if (this->attr->uid == userId())
            this->attr->uid = (uid_t)-1;

        this->attr->gid = groupIdFromName(this->interface.group);

        if (this->attr->gid == groupId())
            this->attr->gid = (gid_t)-1;

        // !!! see also libssh2_session_last_error()
        THROW_ON_SYS_ERROR_FMT(
            libssh2_sftp_fsetstat(this->sftpHandle, this->attr) != 0, FileOwnerError, "unable to set ownership for '%s'",
            strZ(this->nameTmp));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWriteSftp(THIS_VOID, const Buffer *buffer)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);
    //jrt fd becomes handle
    ASSERT(this->fd != -1);

    // Write the data
    // jrt libssh2_sftp_write()
    // fd becomes handle
    //if (write(this->fd, bufPtrConst(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
    // !!! verify this cast is valid
    if (libssh2_sftp_write(this->sftpHandle, (const char *)bufPtrConst(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strZ(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageWriteSftpClose(THIS_VOID)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Close if the file has not already been closed
    // jrt fd becomes handle
    // jrt libssh2*close()
    //if (this->fd != -1)
    if (this->sftpHandle != NULL)
    {
        // Sync the file
        //jrt fd becomes handle, libssh2_sftp_fsync if available -- determine how to query hello for fsync capability
        //!!! per below, if cannot query hellow for capability then we can act accordingly upon receipt of LIBSSH2_FX_OP_UNSUPPORTED
        //bail out/issue/log warning etc
        //jrt !!! LIBSSH2_ERROR_SFTP_PROTOCOL - An invalid SFTP protocol response was received on the socket, or an SFTP operation
        //caused an errorcode to be returned by the server. In particular, this can be returned if the SSH server does not support
        //the fsync operation: the SFTP subcode LIBSSH2_FX_OP_UNSUPPORTED will be returned in this case.
        if (this->interface.syncFile)
        {
            THROW_ON_SYS_ERROR_FMT(
                libssh2_sftp_fsync(this->sftpHandle) != 0, FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(this->nameTmp));
        }

        // Close the file
        //jrt fd becomes handle, libssh2_*close()
        memContextCallbackClear(objMemContext(this));

        // Update modified time
        if (this->interface.timeModified != 0)
        {
            //jrt verify this
            this->attr->flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
            this->attr->atime = this->attr->mtime = this->interface.timeModified;
            THROW_ON_SYS_ERROR_FMT(
                libssh2_sftp_fsetstat(
                    this->sftpHandle, this->attr) != 0,
                FileInfoError, "unable to set time for '%s'", strZ(this->nameTmp));
        }

        THROW_ON_SYS_ERROR_FMT(
            libssh2_sftp_close(this->sftpHandle) != 0, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));
        this->sftpHandle = NULL;
//        this->fd = -1;

        // Rename from temp file
        // jrt libssh2_sftp_rename
        if (this->interface.atomic)
        {
            if (libssh2_sftp_rename(this->sftpSession, strZ(this->nameTmp), strZ(this->interface.name)) != 0)
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name));
        }

        // Sync the path
        // jrt determine if a libssh2 function supports this
        if (this->interface.syncPath)
            storageInterfacePathSyncP(this->storage, this->path);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get file descriptor
***********************************************************************************************************************************/
static int
storageWriteSftpFd(const THIS_VOID)
{
    THIS(const StorageWriteSftp);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // jrt fd becomes handle
    FUNCTION_TEST_RETURN(INT, this->fd);
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteSftpNew(
    StorageSftp *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, storage);
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

    OBJ_NEW_BEGIN(StorageWriteSftp, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        StorageWriteSftp *driver = OBJ_NEW_ALLOC();

        *driver = (StorageWriteSftp)
        {
            .storage = storage,
            .path = strPath(name),
            // jrt fd becomes handle
            .fd = -1,

            .interface = (StorageWriteInterface)
            {
                .type = STORAGE_SFTP_TYPE,
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
                    .close = storageWriteSftpClose,
                    // jrt fd becomes handle
                    .fd = storageWriteSftpFd,
                    .open = storageWriteSftpOpen,
                    .write = storageWriteSftp,
                },
            },
        };

        // Create temp file name
        driver->nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(name)) : driver->interface.name;

        this = storageWriteNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, this);
}

//#endif // HAVE_LIBSSH2
