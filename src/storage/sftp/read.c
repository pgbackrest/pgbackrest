/***********************************************************************************************************************************
SFTP Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

#ifdef HAVE_LIBSSH2

#include "common/debug.h"
#include "common/io/session.h"
#include "common/log.h"
#include "storage/read.h"
#include "storage/sftp/read.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct StorageReadSftp
{
    StorageReadInterface interface;                                 // Interface
    StorageSftp *storage;                                           // Storage that created this object

    LIBSSH2_SESSION *session;                                       // LibSsh2 session
    LIBSSH2_SFTP *sftpSession;                                      // LibSsh2 session sftp session
    LIBSSH2_SFTP_HANDLE *sftpHandle;                                // LibSsh2 session sftp handle
    LIBSSH2_SFTP_ATTRIBUTES *attr;                                  // LibSsh2 file attributes
    uint64_t current;                                               // Current bytes read from file
    uint64_t limit;                                                 // Limit bytes to be read from file (UINT64_MAX for no limit)
    bool eof;                                                       // Did we reach end of file
} StorageReadSftp;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_SFTP_TYPE                                                                                        \
    StorageReadSftp *
#define FUNCTION_LOG_STORAGE_READ_SFTP_FORMAT(value, buffer, bufferSize)                                                           \
    objNameToLog(value, "StorageReadSftp", buffer, bufferSize)

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

    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(this->interface.name), (unsigned int)strSize(this->interface.name), LIBSSH2_FXF_READ, 0,
            LIBSSH2_SFTP_OPENFILE);
    }
    while (this->sftpHandle == NULL && storageSftpWaitFd(this->storage, libssh2_session_last_errno(this->session)));

    if (this->sftpHandle == NULL)
    {
        const int rc = libssh2_session_last_errno(this->session);

        if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL || rc == LIBSSH2_ERROR_EAGAIN)
        {
            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_FILE)
            {
                if (!this->interface.ignoreMissing)
                    THROW_FMT(FileMissingError, STORAGE_ERROR_READ_MISSING, strZ(this->interface.name));
            }
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &FileOpenError,
                    strNewFmt(STORAGE_ERROR_READ_OPEN, strZ(this->interface.name)), NULL);
            }
        }
    }
    // Else success
    else
    {
        // Seek to offset, libssh2_sftp_seek64 returns void
        if (this->interface.offset != 0)
            libssh2_sftp_seek64(this->sftpHandle, this->interface.offset);
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
        // Determine expected bytes to read. If remaining size in the buffer would exceed the limit then reduce the expected read.
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > this->limit)
            expectedBytes = (size_t)(this->limit - this->current);

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
                if ((sftpErr = libssh2_sftp_last_error(this->sftpSession)) == LIBSSH2_FX_BAD_MESSAGE && this->interface.offset > 0)
                    THROW_FMT(FileOpenError, STORAGE_ERROR_READ_SEEK, this->interface.offset, strZ(this->interface.name));
                else
                    THROW_FMT(FileReadError, "unable to read '%s': sftp errno [%" PRIu64 "]", strZ(this->interface.name), sftpErr);
            }
            else if (rc == LIBSSH2_ERROR_EAGAIN)
                THROW_FMT(FileReadError, "timeout reading '%s'", strZ(this->interface.name));
            else
            {
                storageSftpEvalLibSsh2Error(
                    (int)rc, libssh2_sftp_last_error(this->sftpSession), &FileReadError,
                    strNewFmt("unable to read '%s'", strZ(this->interface.name)), NULL);
            }
        }

        // Update amount of buffer used
        this->current += (uint64_t)actualBytes;

        // If less data than expected was read or the limit has been reached then EOF. The file may not actually be EOF but we are
        // not concerned with files that are growing. Just read up to the point where the file is being extended.
        if ((size_t)actualBytes != expectedBytes || this->current == this->limit)
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
                    STORAGE_ERROR_READ_CLOSE ": libssh2 errno [%d]%s", strZ(this->interface.name), rc,
                    rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
                        strZ(strNewFmt(": sftp errno [%lu]", libssh2_sftp_last_error(this->sftpSession))) : "");
            else
            {
                storageSftpEvalLibSsh2Error(
                    rc, libssh2_sftp_last_error(this->sftpSession), &FileCloseError,
                    strNewFmt("timeout closing file '%s'", strZ(this->interface.name)), NULL);
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
FN_EXTERN StorageRead *
storageReadSftpNew(
    StorageSftp *const storage, const String *const name, const bool ignoreMissing, LIBSSH2_SESSION *const session,
    LIBSSH2_SFTP *const sftpSession, LIBSSH2_SFTP_HANDLE *const sftpHandle, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM_P(VOID, sftpSession);
        FUNCTION_LOG_PARAM_P(VOID, sftpHandle);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    OBJ_NEW_BEGIN(StorageReadSftp, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageReadSftp)
        {
            .storage = storage,
            .session = session,
            .sftpSession = sftpSession,
            .sftpHandle = sftpHandle,

            // Rather than enable/disable limit checking just use a big number when there is no limit. We can feel pretty confident
            // that no files will be > UINT64_MAX in size. This is a copy of the interface limit but it simplifies the code during
            // read so it seems worthwhile.
            .limit = limit == NULL ? UINT64_MAX : varUInt64(limit),

            .interface = (StorageReadInterface)
            {
                .type = STORAGE_SFTP_TYPE,
                .name = strDup(name),
                .ignoreMissing = ignoreMissing,
                .offset = offset,
                .limit = varDup(limit),

                .ioInterface = (IoReadInterface)
                {
                    .close = storageReadSftpClose,
                    .eof = storageReadSftpEof,
                    .open = storageReadSftpOpen,
                    .read = storageReadSftp,
                },
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadNew(this, &this->interface));
}

#endif // HAVE_LIBSSH2
