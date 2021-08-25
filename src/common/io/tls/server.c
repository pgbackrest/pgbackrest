/***********************************************************************************************************************************
TLS Server
***********************************************************************************************************************************/
#include "build.auto.h"

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/io/server.h"
#include "common/io/tls/server.h"
#include "common/io/tls/session.h"
#include "common/memContext.h"
#include "common/stat.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(TLS_STAT_SERVER_STR,                                  TLS_STAT_SERVER);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TlsServer
{
    MemContext *memContext;                                         // Mem context
    String *host;                                                   // Host
    SSL_CTX *context;                                               // TLS context
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
} TlsServer;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
tlsServerToLog(const THIS_VOID)
{
    THIS(const TlsServer);

    return strNewFmt("{host: %s, timeout: %" PRIu64 "}", strZ(this->host), this->timeout);
}

#define FUNCTION_LOG_TLS_SERVER_TYPE                                                                                               \
    TlsServer *
#define FUNCTION_LOG_TLS_SERVER_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, tlsServerToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free context
***********************************************************************************************************************************/
static void
tlsServerFreeResource(THIS_VOID)
{
    THIS(TlsServer);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SERVER, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    SSL_CTX_free(this->context);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
!!!INIT STEPS FOR SERVER CERT PULLED FROM src/backend/libpq/be-secure-openssl.c be_tls_init()
***********************************************************************************************************************************/
static void
tlsServerInit(const TlsServer *const this, SSL *const tlsSession)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_SERVER, this);
        FUNCTION_LOG_PARAM_P(VOID, tlsSession);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
!!!AUTH STEPS FOR SERVER CERT PULLED FROM src/backend/libpq/be-secure-openssl.c be_tls_open_server()
***********************************************************************************************************************************/
static bool
tlsServerAuth(const TlsServer *const this, SSL *const tlsSession)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_SERVER, this);
        FUNCTION_LOG_PARAM_P(VOID, tlsSession);
    FUNCTION_LOG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static IoSession *
tlsServerAccept(THIS_VOID, IoSession *const ioSession)
{
    THIS(TlsServer);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SERVER, this);
        FUNCTION_LOG_PARAM(IO_SESSION, ioSession);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(ioSession != NULL);

    IoSession *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        SSL *tlsSession = SSL_new(this->context);

        // Initialize TLS session
        TRY_BEGIN()
        {
            tlsServerInit(this, tlsSession);
        }
        CATCH_ANY()
        {
            SSL_free(tlsSession);                                                   // {uncovered - !!! add test}
            RETHROW();                                                              // {uncovered - !!! add test}
        }
        TRY_END();

        // Open TLS session
        result = tlsSessionNew(tlsSession, ioSession, 5000); // !!! FIX TIMEOUT

        // Authenticate TLS session
        ioSessionAuthenticatedSet(result, tlsServerAuth(this, tlsSession));

        // Move session
        ioSessionMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    statInc(TLS_STAT_SESSION_STR);

    FUNCTION_LOG_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
static const String *
tlsServerName(THIS_VOID)
{
    THIS(TlsServer);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_SERVER, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->host);
}

/**********************************************************************************************************************************/
static const IoServerInterface tlsServerInterface =
{
    .type = IO_SERVER_TLS_TYPE,
    .name = tlsServerName,
    .accept = tlsServerAccept,
    .toLog = tlsServerToLog,
};

IoServer *
tlsServerNew(const String *const host, const String *const keyFile, const String *const certFile, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(STRING, keyFile);
        FUNCTION_LOG_PARAM(STRING, certFile);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(keyFile != NULL);
    ASSERT(certFile != NULL);

    IoServer *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsServer")
    {
        TlsServer *const driver = memNew(sizeof(TlsServer));

        *driver = (TlsServer)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .timeout = timeout,
        };

        // Initialize TLS
        cryptoInit();

        // Initialize ssl and create a context
        const SSL_METHOD *const method = SSLv23_method();
        cryptoError(method == NULL, "unable to load TLS method");

        driver->context = SSL_CTX_new(method);
        cryptoError(driver->context == NULL, "unable to create TLS context");

        // !!! NEED TO LIMIT TLS VERSIONS

        // Set callback to free context
        memContextCallbackSet(driver->memContext, tlsServerFreeResource, driver);

        // Configure the context by setting key and cert
        cryptoError(
            SSL_CTX_use_certificate_file(driver->context, strZ(certFile), SSL_FILETYPE_PEM) <= 0,
            "unable to load server certificate");
        cryptoError(
            SSL_CTX_use_PrivateKey_file(driver->context, strZ(keyFile), SSL_FILETYPE_PEM) <= 0,
            "unable to load server private key");

        statInc(TLS_STAT_SERVER_STR);

        this = ioServerNew(driver, &tlsServerInterface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(IO_SERVER, this);
}
