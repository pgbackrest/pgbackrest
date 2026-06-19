/***********************************************************************************************************************************
SFTP Storage Read
***********************************************************************************************************************************/
#include <build.h>

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/io/session.h"
#include "common/log.h"
#include "storage/read.h"
#include "storage/sftp/read.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
struct StorageReadSftp
{
    const StorageReadInterface *interface;                          // Interface
    StorageSftp *storage;                                           // Storage that created this object

    const String *name;                                             // File name
    uint64_t offset;                                                // Read offset
    const Variant *limit;                                           // Read limit (NULL for no limit)

    LIBSSH2_SFTP_HANDLE *sftpHandle;                                // LibSsh2 session sftp handle
    LIBSSH2_SFTP_ATTRIBUTES *attr;                                  // LibSsh2 file attributes
    uint64_t current;                                               // Current bytes read from file
    bool eof;                                                       // Did we reach end of file
};

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
static bool
storageReadSftpOpen(THIS_VOID)
{
    THIS(StorageReadSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Retry the open once if the connection was lost (e.g. the server dropped an idle connection) and reopened successfully
    unsigned int retry = 0;
    int connErrno = 0;

    do
    {
        do
        {
            this->sftpHandle = libssh2_sftp_open_ex(
                storageSftpSessionSftp(this->storage), strZ(this->name), (unsigned int)strSize(this->name), LIBSSH2_FXF_READ, 0,
                LIBSSH2_SFTP_OPENFILE);
        }
        while (this->sftpHandle == NULL &&
               storageSftpWaitFd(this->storage, (connErrno = libssh2_session_last_errno(storageSftpSession(this->storage)))));
    }
    while (this->sftpHandle == NULL && retry++ == 0 && storageSftpReconnect(this->storage, connErrno));

    if (this->sftpHandle == NULL)
    {
        const int rc = libssh2_session_last_errno(storageSftpSession(this->storage));
        const uint64_t sftpErr = libssh2_sftp_last_error(storageSftpSessionSftp(this->storage));

        // Throw on any error except missing, which is reported to the caller via the result
        if (rc != LIBSSH2_ERROR_SFTP_PROTOCOL || sftpErr != LIBSSH2_FX_NO_SUCH_FILE)
        {
            storageSftpEvalLibSsh2Error(
                rc, sftpErr, &FileOpenError, strNewFmt(STORAGE_ERROR_READ_OPEN, strZ(this->name)), NULL);
        }
    }
    // Else success
    else
    {
        // Seek to offset, libssh2_sftp_seek64 returns void
        if (this->offset != 0)
            libssh2_sftp_seek64(this->sftpHandle, this->offset);
    }

    FUNCTION_LOG_RETURN(BOOL, this->sftpHandle != NULL);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadSftp(THIS_VOID, Buffer *const buffer, const bool block)
{
    THIS(StorageReadSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_SFTP, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->sftpHandle != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    ssize_t actualBytes = 0;

    // Read if EOF has not been reached
    if (!this->eof)
    {
        // Rather than enable/disable limit checking just use a big number when there is no limit. We can feel pretty confident that
        // no files will be > UINT64_MAX in size.
        const uint64_t limit = this->limit == NULL ? UINT64_MAX : varUInt64(this->limit);

        // Determine expected bytes to read. If remaining size in the buffer would exceed the limit then reduce the expected read.
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > limit)
            expectedBytes = (size_t)(limit - this->current);

        bufLimitSet(buffer, expectedBytes);
        ssize_t rc = 0;

        // Read until EOF or buffer is full
        do
        {
            do
            {
                rc = libssh2_sftp_read(this->sftpHandle, (char *)bufRemainsPtr(buffer), bufRemains(buffer));
            }
            while (storageSftpWaitFd(this->storage, rc));

            // Break on EOF or error
            if (rc <= 0)
                break;

            // Account/shift for bytes read
            bufUsedInc(buffer, (size_t)rc);
        }
        while (!bufFull(buffer));

        // Total bytes read into the buffer
        actualBytes = (ssize_t)bufUsed(buffer);

        // Error occurred during read
        if (rc < 0)
        {
            if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL)
            {
                uint64_t sftpErr = 0;

                // libssh2 sftp lseek seems to return LIBSSH2_FX_BAD_MESSAGE on a seek too far
                if ((sftpErr = libssh2_sftp_last_error(storageSftpSessionSftp(this->storage))) == LIBSSH2_FX_BAD_MESSAGE &&
                    this->offset > 0)
                {
                    THROW_FMT(FileOpenError, STORAGE_ERROR_READ_SEEK, this->offset, strZ(this->name));
                }
                else
                    THROW_FMT(FileReadError, "unable to read '%s': sftp errno [%" PRIu64 "]", strZ(this->name), sftpErr);
            }
            else if (rc == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(FileReadError, "timeout reading '%s'", strZ(this->name));
            else
            {
                storageSftpEvalLibSsh2Error(
                    (int)rc, libssh2_sftp_last_error(storageSftpSessionSftp(this->storage)), &FileReadError,
                    strNewFmt("unable to read '%s'", strZ(this->name)), NULL);
            }
        }

        // Update amount of buffer used
        this->current += (uint64_t)actualBytes;

        // If less data than expected was read or the limit has been reached then EOF. The file may not actually be EOF but we are
        // not concerned with files that are growing. Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes || this->current == limit)
            this->eof = true;
    }

    FUNCTION_LOG_RETURN(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
static void
storageReadSftpClose(THIS_VOID)
{
    THIS(StorageReadSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    if (this->sftpHandle != NULL)
    {
        int rc;

        // Close the file
        do
        {
            rc = libssh2_sftp_close(this->sftpHandle);
        }
        while (storageSftpWaitFd(this->storage, rc));

        if (rc != 0)
        {
            if (rc != LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(
                    FileCloseError,
                    STORAGE_ERROR_READ_CLOSE ": libssh2 errno [%d]%s", strZ(this->name), rc,
                    rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                        strZ(strNewFmt(": sftp errno [%lu]", libssh2_sftp_last_error(storageSftpSessionSftp(this->storage)))) : "");
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(storageSftpSessionSftp(this->storage)), &FileCloseError,
                    strNewFmt("timeout closing file '%s'", strZ(this->name)), NULL);
            }
        }
    }

    this->sftpHandle = NULL;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has file reached EOF?
***********************************************************************************************************************************/
static bool
storageReadSftpEof(THIS_VOID)
{
    THIS(StorageReadSftp);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_READ_SFTP, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(BOOL, this->eof);
}

/**********************************************************************************************************************************/
static const StorageReadInterface storageReadSftpInterface =
{
    .close = storageReadSftpClose,
    .eof = storageReadSftpEof,
    .open = storageReadSftpOpen,
    .read = storageReadSftp,
};

FN_EXTERN StorageReadSftp *
storageReadSftpNew(
    StorageSftp *const storage, const String *const name, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_SFTP, storage);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadSftp, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadSftp)
        {
            .interface = &storageReadSftpInterface,
            .storage = storage,
            .name = strDup(name),
            .offset = offset,
            .limit = varDup(limit),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ_SFTP, this);
}

#endif // HAVE_LIBSSH2
