/***********************************************************************************************************************************
SFTP Storage File Write
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/log.h"
#include "common/user.h"
#include "common/wait.h"
#include "storage/sftp/storage.intern.h"
#include "storage/sftp/write.h"
#include "storage/write.intern.h"

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
***********************************************************************************************************************************/
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

    const unsigned long int flags = LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE | (this->interface.truncate ? LIBSSH2_FXF_TRUNC : 0);

    // Open the file
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), flags, (int)this->interface.modeFile,
            LIBSSH2_SFTP_OPENFILE);
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
                this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), flags, (int)this->interface.modeFile,
                LIBSSH2_SFTP_OPENFILE);
        }
        while (this->sftpHandle == NULL &&
               (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait)));
    }

    // Handle error
    if (this->sftpHandle == NULL)
    {
        int rc = libssh2_session_last_errno(this->session);

        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
        {
            uint64_t sftpErr = libssh2_sftp_last_error(this->sftpSession);

            if (sftpErr == LIBSSH2_FX_NO_SUCH_FILE)
                THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->interface.name));
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, sftpErr, &FileOpenError, strNewFmt(STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name)), NULL);
            }
        }
        else
            THROW_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name));
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

        if (rc)
            THROW_FMT(FileOwnerError, "unable to set ownership for '%s'", strZ(this->nameTmp));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write to the file
***********************************************************************************************************************************/
static void
storageWriteSftp(THIS_VOID, const Buffer *const buffer)
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
    int shift = 0;                                                  // Amount to shift into the buffer
    this->wait = waitNew(this->timeoutConnect);

    // Loop until all the data is written
    do
    {
        do
        {
            rc = libssh2_sftp_write(this->sftpHandle, (const char *)bufPtrConst(buffer) + shift, nwrite);
        }
        while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        // Break on error. Error will be thrown below the loop
        if (rc < 0)
            break;

        this->wait = waitNew(this->timeoutConnect);

        // Shift for next write start point
        shift += (int)rc;

        // Update amount left to write
        nwrite -= (size_t)rc;
    }
    while (nwrite);

    if (rc < 0)
        THROW_FMT(FileWriteError, "unable to write '%s'", strZ(this->nameTmp));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Unlink already existing file
***********************************************************************************************************************************/
static void
storageWriteSftpUnlinkExisting(THIS_VOID)
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
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        storageSftpEvalLibSsh2Error(
            rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
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
                this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), strZ(this->interface.name),
                (unsigned int)strSize(this->interface.name),
                LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    if (rc)
    {
        storageSftpEvalLibSsh2Error(
            rc, libssh2_sftp_last_error(this->sftpSession), &FileRemoveError,
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

    // Close if the file has not already been closed
    if (this->sftpHandle != NULL)
    {
        int rc = 0;
        char *libssh2_errmsg;
        int errmsg_len;
        int libssh2_errno;

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

            attr.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
            attr.atime = (unsigned int)this->interface.timeModified;
            attr.mtime = (unsigned int)this->interface.timeModified;

            this->wait = waitNew(this->timeoutConnect);

            do
            {
                rc = libssh2_sftp_fsetstat(this->sftpHandle, &attr);
            }
            while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

            if (rc)
            {
                libssh2_errno = libssh2_session_last_error(this->session, &libssh2_errmsg, &errmsg_len, 0);

                THROW_FMT(
                    FileInfoError,
                    "unable to set time for '%s': libssh2 error [%d] %s", strZ(this->nameTmp), libssh2_errno, libssh2_errmsg);
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
                STORAGE_ERROR_WRITE_CLOSE ": libssh2 error [%d] %s", strZ(this->nameTmp), libssh2_errno, libssh2_errmsg);
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

            if (rc)
            {
                // some/most sftp servers will not rename over an existing file, in testing this returned LIBSSH2_FX_FAILURE
                if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL && libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_FAILURE)
                {
                    // remove the existing file and retry the rename
                    storageWriteSftpUnlinkExisting(this);
                    storageWriteSftpRename(this);
                }
                else
                {
                    storageSftpEvalLibSsh2Error(
                        rc, libssh2_sftp_last_error(this->sftpSession), &FileCloseError,
                        strNewFmt("unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name)), NULL);
                }
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageWrite *
storageWriteSftpNew(
    StorageSftp *const storage, const String *const name, IoSession *const ioSession, LIBSSH2_SESSION *const session,
    LIBSSH2_SFTP *const sftpSession, LIBSSH2_SFTP_HANDLE *const sftpHandle, const TimeMSec timeoutConnect,
    const TimeMSec timeoutSession, const mode_t modeFile, const mode_t modePath, const String *const user,
    const String *const group, const time_t timeModified, const bool createPath, const bool syncFile, const bool syncPath,
    const bool atomic, const bool truncate)
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
        FUNCTION_LOG_PARAM(BOOL, truncate);
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
                .truncate = truncate,
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
