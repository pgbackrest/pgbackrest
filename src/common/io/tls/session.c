/***********************************************************************************************************************************
TLS Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include <openssl/err.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/session.intern.h"
#include "common/io/socket/session.h"
#include "common/io/tls/client.h"
#include "common/io/tls/session.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define TLS_SESSION_TYPE                                            TlsSession
#define TLS_SESSION_PREFIX                                          tlsSession

typedef struct TlsSession
{
    MemContext *memContext;                                         // Mem context
    SocketSession *socketSession;                                   // Socket session
    SSL *session;                                                   // TLS session on the socket
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool shutdownOnClose;                                           // Shutdown the TLS connection when closing the socket

    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
} TlsSession;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
tlsSessionToLog(const THIS_VOID)
{
    THIS(const TlsSession);

    return strNewFmt(
        "{socketSession: %s, timeout: %" PRIu64", shutdownOnClose: %s}",
        this->socketSession == NULL || memContextFreeing(this->memContext) ? NULL_Z : strZ(sckSessionToLog(this->socketSession)),
        this->timeout, cvtBoolToConstZ(this->shutdownOnClose));
}

#define FUNCTION_LOG_TLS_SESSION_TYPE                                                                                              \
    TlsSession *
#define FUNCTION_LOG_TLS_SESSION_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, tlsSessionToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TLS_SESSION, LOG, logLevelTrace)
{
    SSL_free(this->session);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
static void
tlsSessionClose(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // If not already closed
    if (this->session != NULL)
    {
        // Shutdown on request
        if (this->shutdownOnClose)
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

Returns:
0 if the function should be tried again with the same parameters
-1 if the connection was closed gracefully
> 0 with the read/write size if SSL_read()/SSL_write() was called
***********************************************************************************************************************************/
// Helper to process error conditions
static int
tlsSessionResultProcess(TlsSession *this, int errorTls, long unsigned int errorTlsDetail, int errorSys, bool closeOk)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
        FUNCTION_LOG_PARAM(INT, errorTls);
        FUNCTION_LOG_PARAM(UINT64, errorTlsDetail);
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
                THROW(ProtocolError, "unexpected TLS eof");

            this->shutdownOnClose = false;
            tlsSessionClose(this);
            break;
        }

        // Try again after waiting for read ready
        case SSL_ERROR_WANT_READ:
        {
            ioReadReadyP(sckSessionIoRead(this->socketSession), .error = true);
            result = 0;
            break;
        }

        // Try again after waiting for write ready
        case SSL_ERROR_WANT_WRITE:
        {
            sckSessionReadyWrite(this->socketSession);
            result = 0;
            break;
        }

        // A syscall failed (this usually indicates unexpected eof)
        case SSL_ERROR_SYSCALL:
            THROW_SYS_ERROR_CODE(errorSys, KernelError, "TLS syscall error");

        // Any other error that we cannot handle
        default:
        {
            // Get detailed error message when available
            const char *errorTlsDetailMessage = ERR_reason_error_string(errorTlsDetail);

            THROW_FMT(
                ServiceError, "TLS error [%d:%lu] %s", errorTls, errorTlsDetail,
                errorTlsDetailMessage == NULL ? "no details available" : errorTlsDetailMessage);
        }
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

    // Process errors
    if (result <= 0)
    {
        // Get TLS error and store errno in case of syscall error
        int errorTls = SSL_get_error(this->session, result);
        long unsigned int errorTlsDetail = ERR_get_error();
        int errorSys = errno;

        result = tlsSessionResultProcess(this, errorTls, errorTlsDetail, errorSys, closeOk);
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

    int result = 0;

    // If blocking read keep reading until buffer is full
    do
    {
        // If no TLS data pending then check the socket to reduce blocking
        if (!SSL_pending(this->session))
            ioReadReadyP(sckSessionIoRead(this->socketSession), .error = true);

        // Read and handle errors. The error queue must be cleared before this operation.
        ERR_clear_error();

        result = tlsSessionResult(this, SSL_read(this->session, bufRemainsPtr(buffer), (int)bufRemains(buffer)), true);

        // Update amount of buffer used
        if (result > 0)
        {
            bufUsedInc(buffer, (size_t)result);
        }
        // If the connection was closed then we are at eof. It is up to the layer above TLS to decide if this is an error.
        else if (result == -1)
            break;
    }
    while (block && bufRemains(buffer) > 0);

    FUNCTION_LOG_RETURN(SIZE, (size_t)result);
}

/***********************************************************************************************************************************
Write to the TLS session
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
        // Write and handle errors. The error queue must be cleared before this operation.
        ERR_clear_error();

        result = tlsSessionResult(this, SSL_write(this->session, bufPtrConst(buffer), (int)bufUsed(buffer)), false);

        // Either a retry or all data was written
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
static IoRead *tlsSessionIoRead(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->read);
}

/**********************************************************************************************************************************/
static IoWrite *tlsSessionIoWrite(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->write);
}

/**********************************************************************************************************************************/
static const IoSessionInterface tlsSessionInterface =
{
    .type = &IO_CLIENT_TLS_TYPE_STR,
    .close = tlsSessionClose,
    .ioRead = tlsSessionIoRead,
    .ioWrite = tlsSessionIoWrite,
    .toLog = tlsSessionToLog,
};

IoSession *
tlsSessionNew(SSL *session, SocketSession *socketSession, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, socketSession);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(session != NULL);
    ASSERT(socketSession != NULL);

    IoSession *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsSession")
    {
        TlsSession *driver = memNew(sizeof(TlsSession));

        *driver = (TlsSession)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .session = session,
            .socketSession = sckSessionMove(socketSession, MEM_CONTEXT_NEW()),
            .timeout = timeout,
            .shutdownOnClose = true,
        };

        // Ensure session is freed
        memContextCallbackSet(driver->memContext, tlsSessionFreeResource, driver);

        // Assign socket to TLS session
        cryptoError(
            SSL_set_fd(driver->session, sckSessionFd(driver->socketSession)) != 1, "unable to add socket to TLS session");

        // Negotiate TLS session. The error queue must be cleared before this operation.
        int result = 0;

        while (result == 0)
        {
            ERR_clear_error();

            if (sckSessionType(driver->socketSession) == sckSessionTypeClient)
                result = tlsSessionResult(driver, SSL_connect(driver->session), false);
            else
                result = tlsSessionResult(driver, SSL_accept(driver->session), false);
        }

        // Create read and write interfaces
        driver->write = ioWriteNewP(driver, .write = tlsSessionWrite);
        ioWriteOpen(driver->write);
        driver->read = ioReadNewP(driver, .block = true, .eof = tlsSessionEof, .read = tlsSessionRead);
        ioReadOpen(driver->read);

        // Create session interface
        this = ioSessionNew(driver, &tlsSessionInterface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, this);
}
