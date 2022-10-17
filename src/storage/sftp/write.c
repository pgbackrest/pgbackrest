/***********************************************************************************************************************************
Sftp Storage File write
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>
#include <sys/select.h>

#include "common/debug.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/user.h"
#include "common/wait.h"
#include "storage/sftp/storage.intern.h"
#include "storage/sftp/write.h"
#include "storage/write.intern.h"

// jrt !!! look at implementing an error checking/handling function - libssh2 session error => sftp session error
/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteSftp
{
    StorageWriteInterface interface;                                // Interface
    StorageSftp *storage;                                           // Storage that created this object

    const String *nameTmp;
    const String *path;
    IoSession *ioSession;
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
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
    while (this->sftpHandle == NULL && libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

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
        while (this->sftpHandle == NULL &&
               (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait)));
    }

    // Handle error
    if (this->sftpHandle == NULL)
    {
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
                THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->interface.name));
            else
                THROW_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name));
        }
    }

    // Update user/group owner
    if (this->interface.user != NULL || this->interface.group != NULL)
    {
        LIBSSH2_SFTP_ATTRIBUTES attr;

        attr.flags = LIBSSH2_SFTP_ATTR_UIDGID;
        attr.uid = userIdFromName(this->interface.user);

        if (attr.uid == userId())
            attr.uid = (uid_t)-1;

        attr.gid = groupIdFromName(this->interface.group);

        if (attr.gid == groupId())
            attr.gid = (gid_t)-1;

        this->wait = waitNew(this->timeoutConnect);

        int rc = 0;

        do
        {
            rc = libssh2_sftp_fsetstat(this->sftpHandle, &attr);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        // jrt !!! see also libssh2_session_last_error()
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

    ssize_t rc = 0;
    size_t nwrite = bufUsed(buffer);                                // Amount to write
    int shift = 0;
    this->wait = waitNew(this->timeoutConnect);

    // Loop until all the data is written
    do
    {
        do
        {
            rc = libssh2_sftp_write(this->sftpHandle, (const char *)bufPtrConst(buffer) + shift, nwrite);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        // Break on error. Will be thrown below on rc < 0.
        if (rc < 0)
            break;

        if (rc > 0)
            this->wait = waitNew(this->timeoutConnect);

        // Shift for next write start point
        shift += (int)rc;

        // Update amount left to write
        nwrite -= (size_t)rc;
    }
    while (nwrite);

    //if (rc != (ssize_t)bufUsed(buffer))
    if (rc < 0)
    {
        THROW_FMT(FileWriteError, "unable to write '%s'", strZ(this->nameTmp));
        /*
        // jrt maybe expand later ala
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            THROW_FMT(
                FileWriteError, "unable to write '%s': sftp errno [%lu]", strZ(this->nameTmp),
                libssh2_sftp_last_error(this->sftpSession));
        }
        else
        {
            THROW_FMT(
                FileWriteError, "unable to write '%s': libssh2 errno [%d]", strZ(this->nameTmp),
                libssh2_session_last_errno(this->session));
        }
        */
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Did rename fail due to file already existing
***********************************************************************************************************************************/
static bool
storageWriteSftpRenameFileExistsFailure(const int rc, const int ssh2Errno, const uint64_t sftpErrno)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, rc);
        FUNCTION_LOG_PARAM(INT, ssh2Errno);
        FUNCTION_LOG_PARAM(UINT64, sftpErrno);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(BOOL, (rc && (ssh2Errno == LIBSSH2_ERROR_SFTP_PROTOCOL) && (sftpErrno == LIBSSH2_FX_FAILURE) == true));
}

/***********************************************************************************************************************************
Unlink already existing file and complete rename
***********************************************************************************************************************************/
static void
storageWriteSftpUnlink(THIS_VOID)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    int rc = 0;

    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_unlink_ex(
                this->sftpSession, strZ(this->interface.name), (unsigned int)strSize(this->interface.name));
    } while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        storageSftpEvalLibssh2Error(
            libssh2_session_last_errno(this->session), libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
            strNewFmt("unable to remove existing '%s'", strZ(this->interface.name)), NULL);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Rename a file
***********************************************************************************************************************************/
static void
storageWriteSftpRename(THIS_VOID)
{
    THIS(StorageWriteSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_WRITE_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    int rc = 0;

    this->wait = waitNew(this->timeoutConnect);

    do
    {
        rc = libssh2_sftp_rename_ex(
                this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp),
                strZ(this->interface.name), (unsigned int)strSize(this->interface.name),
                LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        storageSftpEvalLibssh2Error(
            libssh2_session_last_errno(this->session), libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
            strNewFmt("unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name)), NULL);
    }

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
    //ASSERT(this->sftpHandle != NULL);

    // Close if the file has not already been closed
    if (this->sftpHandle != NULL)
    {
        int rc = 0;
        char *libssh2_errmsg;
        int errmsg_len;
        int libssh2_errno;

// !!! jrt handle UNSUPPORTED as noted in comment
        // Sync the file
        // !!! per below, if can't query hello for capability then we can act accordingly upon receipt of LIBSSH2_FX_OP_UNSUPPORTED
        // bail out/issue/log warning etc
        // jrt !!! LIBSSH2_ERROR_SFTP_PROTOCOL - An invalid SFTP protocol response was received on the socket, or an SFTP operation
        // caused an errorcode to be returned by the server. In particular, this can be returned if the SSH server does not support
        // the fsync operation: the SFTP subcode LIBSSH2_FX_OP_UNSUPPORTED will be returned in this case.
        if (this->interface.syncFile)
        {
            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_fsync(this->sftpHandle);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            if (rc)
                THROW_FMT(FileSyncError, STORAGE_ERROR_WRITE_SYNC, strZ(this->nameTmp));
        }

        // Update modified time
        if (this->interface.timeModified != 0)
        {
            LIBSSH2_SFTP_ATTRIBUTES attr;
            // jrt verify this
            attr.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
            attr.atime = (unsigned int)this->interface.timeModified;
            attr.mtime = (unsigned int)this->interface.timeModified;

            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_fsetstat(this->sftpHandle, &attr);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            if (rc != 0)
            {
                libssh2_errno = libssh2_session_last_error(this->session, &libssh2_errmsg, &errmsg_len, 0);

                THROW_FMT(
                    FileInfoError,
                    "unable to set time for '%s': libssh2 error [%d] %s", strZ(this->nameTmp), libssh2_errno, libssh2_errmsg);;
            }
        }

        // Close the file
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        if (rc != 0)
        {
            libssh2_errno = libssh2_session_last_error(this->session, &libssh2_errmsg, &errmsg_len, 0);

            THROW_FMT(
                FileCloseError,
                STORAGE_ERROR_WRITE_CLOSE ": libssh2 error [%d] %s", strZ(this->nameTmp), libssh2_errno, libssh2_errmsg);;
        }

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

            // some/most sftp servers will not rename over an existing file
            if (storageWriteSftpRenameFileExistsFailure(
                rc, libssh2_session_last_errno(this->session), libssh2_sftp_last_error(this->sftpSession)))
            {
                // remove the existing file and retry the rename
                storageWriteSftpUnlink(this);
                storageWriteSftpRename(this);
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

// jrt !!! declare const where needed
/**********************************************************************************************************************************/
StorageWrite *
storageWriteSftpNew(
    StorageSftp *storage, const String *name, IoSession *ioSession, LIBSSH2_SESSION *session, LIBSSH2_SFTP *sftpSession,
    LIBSSH2_SFTP_HANDLE *sftpHandle, const TimeMSec timeoutConnect, const TimeMSec timeoutSession, const mode_t modeFile,
    const mode_t modePath, const String *user, const String *group, const time_t timeModified, const bool createPath,
    const bool syncFile, const bool syncPath,
    const bool atomic)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM_P(VOID, sftpSession);
        FUNCTION_LOG_PARAM_P(VOID, sftpHandle);
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

    OBJ_NEW_BEGIN(StorageWriteSftp, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StorageWriteSftp *driver = OBJ_NEW_ALLOC();

        *driver = (StorageWriteSftp)
        {
            .storage = storage,
            .path = strPath(name),
            .ioSession = ioSession,
            .session = session,
            .sftpSession = sftpSession,
            .sftpHandle = sftpHandle,
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

#endif // HAVE_LIBSSH2
