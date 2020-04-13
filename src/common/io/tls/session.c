/***********************************************************************************************************************************
TLS Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include <openssl/err.h>

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

/**********************************************************************************************************************************/
void
tlsSessionClose(TlsSession *this, bool shutdown)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(BOOL, shutdown);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If not already closed
    if (this->session != NULL)
    {
        // Shutdown on request
        if (shutdown)
            SSL_shutdown(this->session);

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
Process result from SSL_read(), SSL_write(), SSL_connect(), and SSL_accept().
***********************************************************************************************************************************/
static int
tlsSessionResultProcess(TlsSession *this, int errorTls, int errorSys, bool closeOk)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(INT, errorTls);
        FUNCTION_LOG_PARAM(INT, errorSys);
        FUNCTION_LOG_PARAM(BOOL, closeOk);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);

    int result = -1;

    switch (errorTls)
    {
        // The connection was closed
        case SSL_ERROR_ZERO_RETURN:
        {
            if (!closeOk)
                THROW(ProtocolError, "unexpected eof");

            tlsSessionClose(this, false);
            break;
        }

        // Try the read again
        case SSL_ERROR_WANT_READ:
        {
            sckSessionReadyRead(this->socketSession);
            result = 0;
            break;
        }

        // Try the write again
        case SSL_ERROR_WANT_WRITE:
        {
            sckSessionReadyWrite(this->socketSession);
            result = 0;
            break;
        }

        // A syscall failed (this usually indicates eof)
        case SSL_ERROR_SYSCALL:
            THROW_SYS_ERROR_CODE(errorSys, KernelError, "tls failed syscall");

        // Else an error that we cannot handle
        default:
            THROW_FMT(ServiceError, "tls error [%d]", errorTls);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

static int
tlsSessionResult(TlsSession *this, int result, bool closeOk)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(INT, result);
        FUNCTION_LOG_PARAM(BOOL, closeOk);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);

    if (result <= 0)
    {
        // Get tls error and store errno in case of syscall error
        int errorTls = SSL_get_error(this->session, result);
        int errorSys = errno;

        result = tlsSessionResultProcess(this, errorTls, errorSys, closeOk);
    }

    FUNCTION_LOG_RETURN(INT, result);
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

    // If no tls data pending then check the socket to reduce blocking
    if (!SSL_pending(this->session))
        sckSessionReadyRead(this->socketSession);

    // If blocking read keep reading until buffer is full
    do
    {
        // Read and handle errors
        size_t expectedBytes = bufRemains(buffer);
        result = tlsSessionResult(this, SSL_read(this->session, bufRemainsPtr(buffer), (int)expectedBytes), true);

        // Update amount of buffer used
        if (result > 0)
        {
            bufUsedInc(buffer, (size_t)result);
        }
        // If the connection was closed then we are at eof. It is up to the layer above tls to decide if this is an error.
        else if (result == -1)
            break;
    }
    while (block && bufRemains(buffer) > 0);

    FUNCTION_LOG_RETURN(SIZE, (size_t)result);
}

/***********************************************************************************************************************************
Write to the tls session
***********************************************************************************************************************************/
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

    while (result == 0)
    {
        result = tlsSessionResult(this, SSL_write(this->session, bufPtrConst(buffer), (int)bufUsed(buffer)), false);

        CHECK(result == 0 || (size_t)result == bufUsed(buffer));
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

        // Ensure session is freed
        memContextCallbackSet(this->memContext, tlsSessionFreeResource, this);

        // Assign socket to TLS session
        cryptoError(
            SSL_set_fd(this->session, sckSessionFd(this->socketSession)) != 1, "unable to add socket to TLS session");

        // Negotiate TLS session
        int result = 0;

        while (result == 0)
        {
            if (sckSessionType(this->socketSession) == sckSessionTypeClient)
                result = tlsSessionResult(this, SSL_connect(this->session), false);
            else
                result = tlsSessionResult(this, SSL_accept(this->session), false);
        }

        // Create read and write interfaces
        this->write = ioWriteNewP(this, .write = tlsSessionWrite);
        ioWriteOpen(this->write);
        this->read = ioReadNewP(this, .block = true, .eof = tlsSessionEof, .read = tlsSessionRead);
        ioReadOpen(this->read);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(TLS_SESSION, this);
}
