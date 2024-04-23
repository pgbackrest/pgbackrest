/***********************************************************************************************************************************
TLS Session
***********************************************************************************************************************************/
#include "build.auto.h"

#include <openssl/err.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/io/read.h"
#include "common/io/session.h"
#include "common/io/tls/client.h"
#include "common/io/tls/session.h"
#include "common/io/write.h"
#include "common/log.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TlsSession
{
    IoSession *ioSession;                                           // Io session
    SSL *session;                                                   // TLS session on the file descriptor
    TimeMSec timeout;                                               // Timeout for any i/o operation (read, write, etc.)
    bool shutdownOnClose;                                           // Shutdown the TLS connection when closing the session
    bool ignoreUnexpectedEof;                                       // Ignore unexpected EOF when requested

    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
} TlsSession;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
tlsSessionToLog(const THIS_VOID, StringStatic *const debugLog)
{
    THIS(const TlsSession);

    strStcCat(debugLog, "{ioSession: ");
    ioSessionToLog(this->ioSession, debugLog);
    strStcFmt(debugLog, ", timeout: %" PRIu64 ", shutdownOnClose: %s}", this->timeout, cvtBoolToConstZ(this->shutdownOnClose));
}

#define FUNCTION_LOG_TLS_SESSION_TYPE                                                                                              \
    TlsSession *
#define FUNCTION_LOG_TLS_SESSION_FORMAT(value, buffer, bufferSize)                                                                 \
    FUNCTION_LOG_OBJECT_FORMAT(value, tlsSessionToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
static void
tlsSessionFreeResource(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SESSION, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    SSL_free(this->session);

    FUNCTION_LOG_RETURN_VOID();
}

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

        // Close the io session
        ioSessionClose(this->ioSession);

        // Free the TLS session
        memContextCallbackClear(objMemContext(this));
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
tlsSessionResultProcess(
    TlsSession *const this, const int errorTls, const long unsigned int errorTlsDetail, const int errorSys, const bool closeOk)
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
        // Try again after waiting for read ready
        case SSL_ERROR_WANT_READ:
            ioReadReadyP(ioSessionIoReadP(this->ioSession), .error = true);
            result = 0;
            break;

        // Try again after waiting for write ready
        case SSL_ERROR_WANT_WRITE:
            ioWriteReadyP(ioSessionIoWrite(this->ioSession), .error = true);
            result = 0;
            break;

        // Handle graceful termination by the server or unexpected EOF/error
        default:
        {
            // Close connection on graceful termination by the server or unexpected EOF/error when allowed
            if (errorTls == SSL_ERROR_ZERO_RETURN || this->ignoreUnexpectedEof)
            {
                if (!closeOk)
                    THROW(ProtocolError, "unexpected TLS eof");

                this->shutdownOnClose = false;
                tlsSessionClose(this);
            }
            // Else error
            else
            {
                // Get detailed error message when available
                const char *const errorTlsDetailMessage = ERR_reason_error_string(errorTlsDetail);

                THROW_FMT(
                    ServiceError, "TLS error [%d:%lu] %s", errorTls, errorTlsDetail,
                    errorTlsDetailMessage == NULL ? "no details available" : errorTlsDetailMessage);
            }

            break;
        }
    }

    FUNCTION_LOG_RETURN(INT, result);
}

static int
tlsSessionResult(TlsSession *const this, int result, const bool closeOk)
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
        const int errorTls = SSL_get_error(this->session, result);
        const long unsigned int errorTlsDetail = ERR_get_error();
        const int errorSys = errno;

        result = tlsSessionResultProcess(this, errorTls, errorTlsDetail, errorSys, closeOk);
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
Read from the TLS session
***********************************************************************************************************************************/
static size_t
tlsSessionRead(THIS_VOID, Buffer *const buffer, const bool block)
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
        // If no TLS data pending then check the io to reduce blocking
        if (!SSL_pending(this->session))
            ioReadReadyP(ioSessionIoReadP(this->ioSession), .error = true);

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
tlsSessionWrite(THIS_VOID, const Buffer *const buffer)
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

        CHECK(ServiceError, result == 0 || (size_t)result == bufUsed(buffer), "expected retry or complete write");
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
static IoRead *
tlsSessionIoRead(THIS_VOID, const bool ignoreUnexpectedEof)
{
    THIS(TlsSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SESSION, this);
        FUNCTION_TEST_PARAM(BOOL, ignoreUnexpectedEof);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    this->ignoreUnexpectedEof = ignoreUnexpectedEof;

    FUNCTION_TEST_RETURN(IO_READ, this->read);
}

/**********************************************************************************************************************************/
static IoWrite *
tlsSessionIoWrite(THIS_VOID)
{
    THIS(TlsSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(IO_WRITE, this->write);
}

/**********************************************************************************************************************************/
static IoSessionRole
tlsSessionRole(const THIS_VOID)
{
    THIS(const TlsSession);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SESSION, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(STRING_ID, ioSessionRole(this->ioSession));
}

/**********************************************************************************************************************************/
static const IoSessionInterface tlsSessionInterface =
{
    .type = IO_CLIENT_TLS_TYPE,
    .close = tlsSessionClose,
    .ioRead = tlsSessionIoRead,
    .ioWrite = tlsSessionIoWrite,
    .role = tlsSessionRole,
    .toLog = tlsSessionToLog,
};

FN_EXTERN IoSession *
tlsSessionNew(SSL *const session, IoSession *const ioSession, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, session);
        FUNCTION_LOG_PARAM(IO_SESSION, ioSession);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(session != NULL);
    ASSERT(ioSession != NULL);

    OBJ_NEW_BEGIN(TlsSession, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (TlsSession)
        {
            .session = session,
            .ioSession = ioSessionMove(ioSession, objMemContext(this)),
            .timeout = timeout,
            .shutdownOnClose = true,
        };

        // Ensure session is freed
        memContextCallbackSet(objMemContext(this), tlsSessionFreeResource, this);

        // Assign file descriptor to TLS session
        cryptoError(SSL_set_fd(this->session, ioSessionFd(this->ioSession)) != 1, "unable to add fd to TLS session");

        // Negotiate TLS session. The error queue must be cleared before this operation.
        int result = 0;

        while (result == 0)
        {
            ERR_clear_error();

            if (ioSessionRole(this->ioSession) == ioSessionRoleClient)
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
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_SESSION, ioSessionNew(this, &tlsSessionInterface));
}
