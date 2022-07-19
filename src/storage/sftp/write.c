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
#include "common/wait.h"
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
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    LIBSSH2_SFTP_ATTRIBUTES *attr;
    Wait *wait;
    TimeMSec timeoutConnect;
    TimeMSec timeoutSession;
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
    ASSERT(this->sftpHandle != NULL);

    // Close the libssh2 sftpHandle
    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_close(this->sftpHandle);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    THROW_ON_SYS_ERROR_FMT(
        rc != 0, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));

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
    ASSERT(this->sftpSession != NULL);

    // Open the file
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), FILE_OPEN_FLAGS,
            (int)this->interface.modeFile, LIBSSH2_SFTP_OPENFILE);
    }
    while (this->sftpHandle == NULL && waitMore(this->wait));

    // Attempt to create the path if it is missing
    if (this->sftpHandle == NULL && this->interface.createPath)
    {
         // Create the path
        storageInterfacePathCreateP(this->storage, this->path, false, false, this->interface.modePath);

        // Open file again
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            this->sftpHandle = libssh2_sftp_open_ex(
                    this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), FILE_OPEN_FLAGS,
                    (int)this->interface.modeFile, LIBSSH2_SFTP_OPENFILE);
        }
        while (this->sftpHandle == NULL && waitMore(this->wait));
    }

    // Handle errors
    //jrt ??libssh2 errors and handlers
    if (this->sftpHandle == NULL)
    {
        // If session indicates sftp error, can query for sftp error
        // !!! see also libssh2_session_last_error() - possible to return more detailed error
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            //jrt should should check be against LIBSSH2_FX_NO_SUCH_FILE and LIBSSH2_FX_NO_SUCH_PATH
            //verify PATH or FILE or both
            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
                THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->interface.name));
            else
                THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name));
        }
    }

    // Set free callback to ensure the file descriptor is freed
    memContextCallbackSet(objMemContext(this), storageWriteSftpFreeResource, this);

    // Update user/group owner
    // jrt libssh2_sftp_setstat() libssh2_sftp_fsetstat()
    if (this->interface.user != NULL || this->interface.group != NULL)
    {
        int rc = 0;

        this->attr->flags = LIBSSH2_SFTP_ATTR_UIDGID;
        this->attr->uid = userIdFromName(this->interface.user);

        if (this->attr->uid == userId())
            this->attr->uid = (uid_t)-1;

        this->attr->gid = groupIdFromName(this->interface.group);

        if (this->attr->gid == groupId())
            this->attr->gid = (gid_t)-1;

        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_fsetstat(this->sftpHandle, this->attr);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        // !!! see also libssh2_session_last_error()
        THROW_ON_SYS_ERROR_FMT(rc != 0, FileOwnerError, "unable to set ownership for '%s'", strZ(this->nameTmp));
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
    ASSERT(this->sftpHandle != NULL);

    // Write the data
    // !!! verify this cast is valid
    ssize_t rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_write(this->sftpHandle, (const char *)bufPtrConst(buffer), bufUsed(buffer));
    }
    while (rc == LIBSSH2_ERROR_EAGAIN  && waitMore(this->wait));

    if (rc != (ssize_t)bufUsed(buffer))
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
    ASSERT(this->sftpHandle != NULL);

    // Close if the file has not already been closed
    if (this->sftpHandle != NULL)
    {
        int rc = 0;

        // Sync the file
        //!!! per below, if cannot query hellow for capability then we can act accordingly upon receipt of LIBSSH2_FX_OP_UNSUPPORTED
        //bail out/issue/log warning etc
        //jrt !!! LIBSSH2_ERROR_SFTP_PROTOCOL - An invalid SFTP protocol response was received on the socket, or an SFTP operation
        //caused an errorcode to be returned by the server. In particular, this can be returned if the SSH server does not support
        //the fsync operation: the SFTP subcode LIBSSH2_FX_OP_UNSUPPORTED will be returned in this case.
        if (this->interface.syncFile)
        {
            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_fsync(this->sftpHandle);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            THROW_ON_SYS_ERROR_FMT(rc != 0, FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(this->nameTmp));
        }

        memContextCallbackClear(objMemContext(this));

        // Update modified time
        if (this->interface.timeModified != 0)
        {
            //jrt verify this
            this->attr->flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
            this->attr->atime = (unsigned int)this->interface.timeModified;
            this->attr->mtime = (unsigned int)this->interface.timeModified;

            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_fsetstat( this->sftpHandle, this->attr);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            THROW_ON_SYS_ERROR_FMT(rc != 0, FileInfoError, "unable to set time for '%s'", strZ(this->nameTmp));
        }

        // Close the file
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        THROW_ON_SYS_ERROR_FMT(rc != 0, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp));

        this->sftpHandle = NULL;

        // Rename from temp file
        if (this->interface.atomic)
        {
            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_rename_ex(
                    this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), strZ(this->interface.name),
                    (unsigned int)strSize(this->interface.name),
                    LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            // Most versions of sftp do not support overwriting an existing file and will return LIBSSH2_FX_FAILURE
            // need to find out if we can determine server version
            // ??? Do we want to to just fail, or do we want to check if the file exists and rm it and try the rename again
            if (rc)
            {
                if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
                {
                    // libssh2 may return LIBSSH2_FX_FAILURE if the file already exists
                    if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_FAILURE)
                    {
                        LIBSSH2_SFTP_ATTRIBUTES attrs;
                        this->wait = waitNew(this->timeoutConnect);

                        // Check if the file already exists
                        do
                        {
                            rc = libssh2_sftp_stat_ex(
                                this->sftpSession, strZ(this->interface.name), (unsigned int)strSize(this->interface.name),
                                LIBSSH2_SFTP_STAT, &attrs);
                        }
                        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

                        // If it already exists, remove it and retry the rename
                        if (rc == 0)
                        {
                            storageInterfaceRemoveP(this->storage, this->interface.name);

                            this->wait = waitNew(this->timeoutConnect);

                            do
                            {
                                rc = libssh2_sftp_rename_ex(
                                    this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp),
                                    strZ(this->interface.name), (unsigned int)strSize(this->interface.name),
                                    LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
                            }
                            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));
                        }
                    }
                }

                if(rc)
                    THROW_SYS_ERROR_FMT(
                        FileMoveError, "unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name));
            }
        }

        // Sync the path
        // jrt determine if a libssh2 function/sftp version supports this
        if (this->interface.syncPath)
            storageInterfacePathSyncP(this->storage, this->path);
    }

    FUNCTION_LOG_RETURN_VOID();
}

// jrt !!! declare const where needed
/**********************************************************************************************************************************/
StorageWrite *
storageWriteSftpNew(
    StorageSftp *storage, const String *name, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle,
    LIBSSH2_SFTP_ATTRIBUTES *attr, const TimeMSec timeoutConnect, const TimeMSec timeoutSession, const mode_t modeFile,
    const mode_t modePath, const String *user, const String *group, const time_t timeModified, const bool createPath,
    const bool syncFile, const bool syncPath, const bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM_P(VOID, sftpSession);
        FUNCTION_LOG_PARAM_P(VOID, sftpHandle);
        FUNCTION_LOG_PARAM_P(VOID, attr);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
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
            .session = session,
            .sftpSession = sftpSession,
            .sftpHandle = sftpHandle,
            .attr = attr,
            .timeoutConnect = timeoutConnect,
            .timeoutSession = timeoutSession,

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
