/***********************************************************************************************************************************
Sftp Storage Read
***********************************************************************************************************************************/
#include "build.auto.h"

//#ifdef HAVE_LIBSSH2

#include <fcntl.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/read.h"
#include "common/log.h"
#include "common/type/object.h"
#include "common/wait.h"
#include "storage/sftp/read.h"
#include "storage/sftp/storage.intern.h"
#include "storage/read.intern.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct StorageReadSftp
{
    StorageReadInterface interface;                                 // Interface
    StorageSftp *storage;                                           // Storage that created this object

    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftpSession;
    LIBSSH2_SFTP_HANDLE *sftpHandle;
    LIBSSH2_SFTP_ATTRIBUTES *attr;
    uint64_t current;                                               // Current bytes read from file
    uint64_t limit;                                                 // Limit bytes to be read from file (UINT64_MAX for no limit)
    bool eof;
    TimeMSec timeoutSession;
    TimeMSec timeoutConnect;
    Wait *wait;
} StorageReadSftp;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_SFTP_TYPE                                                                                        \
    StorageReadSftp *
#define FUNCTION_LOG_STORAGE_READ_SFTP_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "StorageReadSftp", buffer, bufferSize)

/***********************************************************************************************************************************
Close file descriptor
***********************************************************************************************************************************/
static void
storageReadSftpFreeResource(THIS_VOID)
{
    THIS(StorageReadSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_SFTP, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    int rc = 0;
    this->wait = waitNew(this->timeoutConnect);

    // Close the libssh2 sftpHandle
    do
    {
        rc = libssh2_sftp_close(this->sftpHandle);
    }
    while (rc == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

    THROW_ON_SYS_ERROR_FMT(rc != 0, FileCloseError, STORAGE_ERROR_WRITE_CLOSE, strZ(this->interface.name));

    FUNCTION_LOG_RETURN_VOID();
}

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

    // Open the file
    this->wait = waitNew(this->timeoutConnect);

    do
    {
        this->sftpHandle = libssh2_sftp_open_ex(
            this->sftpSession, strZ(this->interface.name), (unsigned int)strSize(this->interface.name), LIBSSH2_FXF_READ, 0,
            LIBSSH2_SFTP_OPENFILE);
    }
    while (this->sftpHandle == NULL && waitMore(this->wait));

    if (this->sftpHandle == NULL)
    {
        // If session indicates sftp error, can query for sftp error
        // !!! see also libssh2_session_last_error() - possible to return more detailed error
        if (libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_SFTP_PROTOCOL ||
                libssh2_session_last_errno(this->session) == LIBSSH2_ERROR_EAGAIN)
        {
            if (libssh2_sftp_last_error(this->sftpSession) == LIBSSH2_FX_NO_SUCH_PATH)
                THROW_FMT(FileMissingError, STORAGE_ERROR_WRITE_MISSING, strZ(this->interface.name));
            else
                THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_WRITE_OPEN, strZ(this->interface.name));
        }
    }

    FUNCTION_LOG_RETURN(BOOL, this->sftpHandle != NULL);
}

/***********************************************************************************************************************************
Read from a file
***********************************************************************************************************************************/
static size_t
storageReadSftp(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(StorageReadSftp);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_READ_SFTP, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL && this->sftpHandle != NULL);
    ASSERT(buffer != NULL && !bufFull(buffer));

    // Read if EOF has not been reached
    ssize_t actualBytes = 0;

    if (!this->eof)
    {
        // Determine expected bytes to read. If remaining size in the buffer would exceed the limit then reduce the expected read.
        size_t expectedBytes = bufRemains(buffer);

        if (this->current + expectedBytes > this->limit)
            expectedBytes = (size_t)(this->limit - this->current);

        // Read from file
        this->wait = waitNew(this->timeoutConnect);

        do
        {
            actualBytes = libssh2_sftp_read(this->sftpHandle, (char *)bufRemainsPtr(buffer), expectedBytes);
        }
        while (actualBytes == LIBSSH2_ERROR_EAGAIN && waitMore(this->wait));

        // Error occurred during read
        // jrt ??? pull ssh/sftp error???
        if (actualBytes < 0)
            THROW_SYS_ERROR_FMT(FileReadError, "unable to read '%s'", strZ(this->interface.name));

        // Update amount of buffer used
        bufUsedInc(buffer, (size_t)actualBytes);
        this->current += (uint64_t)actualBytes;

        // If less data than expected was read or the limit has been reached then EOF.  The file may not actually be EOF but we are
        // not concerned with files that are growing.  Just read up to the point where the file is being extended.
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

    memContextCallbackClear(objMemContext(this));
    storageReadSftpFreeResource(this);
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
StorageRead *
storageReadSftpNew(
    StorageSftp *const storage, const String *const name, const bool ignoreMissing, LIBSSH2_SESSION *session,
    LIBSSH2_SFTP *sftpSession, LIBSSH2_SFTP_HANDLE *sftpHandle, TimeMSec timeoutSession, TimeMSec timeoutConnect,
    const uint64_t offset, const Variant *const limit)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM_P(VOID, sftpSession);
        FUNCTION_LOG_PARAM_P(VOID, sftpHandle);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutConnect);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeoutSession);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_LOG_END();

    ASSERT(name != NULL);

    StorageRead *this = NULL;

    OBJ_NEW_BEGIN(StorageReadSftp, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        StorageReadSftp *driver = OBJ_NEW_ALLOC();

        *driver = (StorageReadSftp)
        {
            .storage = storage,
            .session = session,
            .sftpSession = sftpSession,
            .sftpHandle = sftpHandle,
            .timeoutSession = timeoutSession,
            .timeoutConnect = timeoutConnect,

            // Rather than enable/disable limit checking just use a big number when there is no limit.  We can feel pretty confident
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

        this = storageReadNew(driver, &driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, this);
}

//#endif // HAVE_LIBSSH2
