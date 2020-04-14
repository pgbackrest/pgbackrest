/***********************************************************************************************************************************
TLS Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/tls/session.intern.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct TlsSession
{
    MemContext *memContext;                                         // Mem context
    SocketSession *socketSession;                                   // Socket session
    SSL *session;                                                   // TLS session on the socket
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)

    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
};

OBJECT_DEFINE_MOVE(TLS_SESSION);

OBJECT_DEFINE_GET(IoRead, , TLS_SESSION, IoRead *, read);
OBJECT_DEFINE_GET(IoWrite, , TLS_SESSION, IoWrite *, write);

OBJECT_DEFINE_FREE(TLS_SESSION);

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TLS_SESSION, LOG, logLevelTrace)
{
    SSL_free(this->session);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Close the connection
***********************************************************************************************************************************/
static void
tlsSessionClose(TlsSession *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If not already closed
    if (this->session != NULL)
    {
        // Free the socket session
        sckSessionFree(this->socketSession);
        this->socketSession = NULL;

        // Free the TLS session
        memContextCallbackClear(this->memContext);
        tlsSessionFreeResource(this);
        this->session = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Report TLS errors.  Returns true if the command should continue and false if it should exit.
***********************************************************************************************************************************/
static bool
tlsSessionError(TlsSession *this, int code)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(INT, code);
    FUNCTION_LOG_END();

    bool result = false;

    switch (code)
    {
        // The connection was closed
        case SSL_ERROR_ZERO_RETURN:
        {
            tlsSessionClose(this);
            break;
        }

        // Try the read/write again
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        {
            result = true;
            break;
        }

        // A syscall failed (usually indicates unexpected eof)
        case SSL_ERROR_SYSCALL:
        {
            // Get the error before closing so it is not cleared
            int errNo = errno;
            tlsSessionClose(this);

            // Throw the sys error
            THROW_SYS_ERROR_CODE(errNo, KernelError, "tls failed syscall");
        }

        // Some other tls error that cannot be handled
        default:
            THROW_FMT(ServiceError, "tls error [%d]", code);
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from the TLS session
***********************************************************************************************************************************/
static size_t
tlsSessionRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(TlsSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);
    ASSERT(buffer != NULL);
    ASSERT(!bufFull(buffer));

    ssize_t result = 0;

    // If blocking read keep reading until buffer is full
    do
    {
        // If no tls data pending then check the socket
        if (!SSL_pending(this->session))
            sckSessionReadWait(this->socketSession);

        // Read and handle errors
        result = SSL_read(this->session, bufRemainsPtr(buffer), (int)bufRemains(buffer));

        if (result <= 0)
        {
            // Break if the error indicates that we should not continue trying
            if (!tlsSessionError(this, SSL_get_error(this->session, (int)result)))
                break;
        }
        // Update amount of buffer used
        else
            bufUsedInc(buffer, (size_t)result);
    }
    while (block && bufRemains(buffer) > 0);

    FUNCTION_LOG_RETURN(SIZE, (size_t)result);
}

/***********************************************************************************************************************************
Write to the tls session
***********************************************************************************************************************************/
static bool
tlsSessionWriteContinue(TlsSession *this, int writeResult, int writeError, size_t writeSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(INT, writeResult);
        FUNCTION_LOG_PARAM(INT, writeError);
        FUNCTION_LOG_PARAM(SIZE, writeSize);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(writeSize > 0);

    bool result = true;

    // Handle errors
    if (writeResult <= 0)
    {
        // If error = SSL_ERROR_NONE then this is the first write attempt so continue
        if (writeError != SSL_ERROR_NONE)
        {
            // Error if the error indicates that we should not continue trying
            if (!tlsSessionError(this, writeError))
                THROW_FMT(FileWriteError, "unable to write to tls [%d]", writeError);

            // Wait for the socket to be readable for tls renegotiation
            sckSessionReadWait(this->socketSession);
        }
    }
    else
    {
        if ((size_t)writeResult != writeSize)
        {
            THROW_FMT(
                FileWriteError, "unable to write to tls, write size %d does not match expected size %zu", writeResult, writeSize);
        }

        result = false;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

static void
tlsSessionWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(TlsSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);
    ASSERT(buffer != NULL);

    int result = 0;
    int error = SSL_ERROR_NONE;

    while (tlsSessionWriteContinue(this, result, error, bufUsed(buffer)))
    {
        result = SSL_write(this->session, bufPtrConst(buffer), (int)bufUsed(buffer));
        error = SSL_get_error(this->session, result);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has session been closed by the server?
***********************************************************************************************************************************/
static bool
tlsSessionEof(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->session == NULL);
}

/**********************************************************************************************************************************/
TlsSession *
tlsSessionNew(SSL *session, SocketSession *socketSession, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, socketSession);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(session != NULL);
    ASSERT(socketSession != NULL);

    TlsSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsSession")
    {
        this = memNew(sizeof(TlsSession));

        *this = (TlsSession)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .session = session,
            .socketSession = sckSessionMove(socketSession, MEM_CONTEXT_NEW()),
            .timeout = timeout,
        };

        // Initiate TLS connection
        cryptoError(
            SSL_set_fd(this->session, sckSessionFd(this->socketSession)) != 1, "unable to add socket to TLS session");
        cryptoError(SSL_connect(this->session) != 1, "unable to negotiate TLS connection");

        memContextCallbackSet(this->memContext, tlsSessionFreeResource, this);

        // Create read and write interfaces
        this->write = ioWriteNewP(this, .write = tlsSessionWrite);
        ioWriteOpen(this->write);
        this->read = ioReadNewP(this, .block = true, .eof = tlsSessionEof, .read = tlsSessionRead);
        ioReadOpen(this->read);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(TLS_SESSION, this);
}
