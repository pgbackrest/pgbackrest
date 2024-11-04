/***********************************************************************************************************************************
SFTP Storage File Write
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/log.h"
#include "common/user.h"
#include "storage/sftp/write.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteSftp
{
    StorageWriteInterface interface;                                // Interface
    StorageSftp *storage;                                           // Storage that created this object

    const String *nameTmp;                                          // Temporary filename utilized for atomic ops
    const String *path;                                             // Utilized for path operations
    LIBSSH2_SESSION *session;                                       // LibSsh2 session
    LIBSSH2_SFTP *sftpSession;                                      // LibSsh2 session sftp session
    LIBSSH2_SFTP_HANDLE *sftpHandle;                                // LibSsh2 session sftp handle
} StorageWriteSftp;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_TYPE                                                                                       \
    StorageWriteSftp *
#define FUNCTION_LOG_STORAGE_WRITE_SFTP_FORMAT(value, buffer, bufferSize)                                                          \
    objNameToLog(value, "StorageWriteSftp", buffer, bufferSize)

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

    const unsigned long int flags = LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE | LIBSSH2_FXF_TRUNC;

    // Open the file
    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), flags, (int)this->interface.modeFile,
            LIBSSH2_SFTP_OPENFILE);
    }
    while (this->sftpHandle == NULL && storageSftpWaitFd(this->storage, libssh2_session_last_errno(this->session)));

    // Attempt to create the path if it is missing
    if (this->sftpHandle == NULL && libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL &&
        libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
    {
        // Create the path
        storageInterfacePathCreateP(this->storage, this->path, false, false, this->interface.modePath);

        // Open file again
        do
        {
            this->sftpHandle = libssh2_sftp_open_ex(
                this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), flags, (int)this->interface.modeFile,
                LIBSSH2_SFTP_OPENFILE);
        }
        while (this->sftpHandle == NULL && storageSftpWaitFd(this->storage, libssh2_session_last_errno(this->session)));
    }

    // Handle error
    if (this->sftpHandle == NULL)
    {
        const int rc = libssh2_session_last_errno(this->session);

        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileOpenError, "timeout while opening file '%s'", strZ(this->interface.name));

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
        {
            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &FileOpenError,
                strNewFmt(STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name)), NULL);
        }
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

    ssize_t rc;
    size_t remains = bufUsed(buffer);                               // Amount left to write
    size_t offset = 0;                                              // Offset into the buffer

    // Loop until all the data is written
    do
    {
        do
        {
            rc = libssh2_sftp_write(this->sftpHandle, (const char *)bufPtrConst(buffer) + offset, remains);
        }
        while (storageSftpWaitFd(this->storage, rc));

        // Break on error. Error will be thrown below the loop.
        if (rc < 0)
            break;

        // Offset for next write start point
        offset += (size_t)rc;

        // Update amount left to write
        remains -= (size_t)rc;
    }
    while (remains);

    if (rc == LIBSSH2_ERROR_EAGAIN)
        THROW_FMT(FileWriteError, "timeout writing '%s'", strZ(this->nameTmp));

    if (rc < 0)
    {
        storageSftpEvalLibSsh2Error(
            (int)rc, libssh2_sftp_last_error(this->sftpSession), &FileWriteError,
            strNewFmt("unable to write '%s'", strZ(this->nameTmp)), NULL);
    }

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

    int rc;

    do
    {
        rc = libssh2_sftp_unlink_ex(this->sftpSession, strZ(this->interface.name), (unsigned int)strSize(this->interface.name));
    }
    while (storageSftpWaitFd(this->storage, rc));

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileWriteError, "timeout unlinking '%s'", strZ(this->interface.name));

        else
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

    int rc;

    do
    {
        rc = libssh2_sftp_rename_ex(
            this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), strZ(this->interface.name),
            (unsigned int)strSize(this->interface.name),
            LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
    }
    while (storageSftpWaitFd(this->storage, rc));

    if (rc != 0)
    {
        if (rc == LIBSSH2_ERROR_EAGAIN)
            THROW_FMT(FileWriteError, "timeout moving '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name));
        else
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
        int rc;

        if (this->interface.syncFile)
        {
            do
            {
                rc = libssh2_sftp_fsync(this->sftpHandle);
            }
            while (storageSftpWaitFd(this->storage, rc));

            if (rc != 0)
            {
                if (rc == LIBSSH2_ERROR_EAGAIN)
                    THROW_FMT(FileSyncError, "timeout syncing file '%s'", strZ(this->nameTmp));
                else
                {
                    storageSftpEvalLibSsh2Error(
                        rc, libssh2_sftp_last_error(this->sftpSession), &FileSyncError,
                        strNewFmt(STORAGE_ERROR_WRITE_SYNC, strZ(this->nameTmp)), NULL);
                }
            }
        }

        // Close the file
        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (storageSftpWaitFd(this->storage, rc));

        if (rc != 0)
        {
            if (rc == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(FileCloseError, "timeout closing file '%s'", strZ(this->nameTmp));

            storageSftpEvalLibSsh2Error(
                rc, libssh2_sftp_last_error(this->sftpSession), &FileCloseError,
                strNewFmt(STORAGE_ERROR_WRITE_CLOSE, strZ(this->nameTmp)), NULL);
        }

        this->sftpHandle = NULL;

        // Rename from temp file
        if (this->interface.atomic)
        {
            do
            {
                rc = libssh2_sftp_rename_ex(
                    this->sftpSession, strZ(this->nameTmp), (unsigned int)strSize(this->nameTmp), strZ(this->interface.name),
                    (unsigned int)strSize(this->interface.name),
                    LIBSSH2_SFTP_RENAME_OVERWRITE | LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE);
            }
            while (storageSftpWaitFd(this->storage, rc));

            if (rc != 0)
            {
                if (rc == LIBSSH2_ERROR_EAGAIN)
                    THROW_FMT(FileCloseError, "timeout renaming file '%s'", strZ(this->nameTmp));

                const uint64_t sftpErr = libssh2_sftp_last_error(this->sftpSession);

                // Some/most sftp servers will not rename over an existing file, in testing this returned LIBSSH2_FX_FAILURE
                if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL &&
                    (sftpErr == LIBSSH2_FX_FAILURE || sftpErr == LIBSSH2_FX_FILE_ALREADY_EXISTS))
                {
                    // Remove the existing file and retry the rename
                    storageWriteSftpUnlinkExisting(this);
                    storageWriteSftpRename(this);
                }
                else
                {
                    storageSftpEvalLibSsh2Error(
                        rc, sftpErr, &FileCloseError,
                        strNewFmt("unable to move '%s' to '%s'", strZ(this->nameTmp), strZ(this->interface.name)), NULL);
                }
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN StorageWrite *
storageWriteSftpNew(
    StorageSftp *const storage, const String *const name, LIBSSH2_SESSION *const session, LIBSSH2_SFTP *const sftpSession,
    LIBSSH2_SFTP_HANDLE *const sftpHandle, const mode_t modeFile, const mode_t modePath, const String *const user,
    const String *const group, const time_t timeModified, const bool createPath, const bool syncFile, const bool syncPath,
    const bool atomic, const bool truncate)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM_P(VOID, sftpSession);
        FUNCTION_LOG_PARAM_P(VOID, sftpHandle);
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

    OBJ_NEW_BEGIN(StorageWriteSftp, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageWriteSftp)
        {
            .storage = storage,
            .path = strPath(name),
            .session = session,
            .sftpSession = sftpSession,
            .sftpHandle = sftpHandle,

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
        this->nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(name)) : this->interface.name;
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, storageWriteNew(this, &this->interface));
}

#endif // HAVE_LIBSSH2
