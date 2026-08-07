/***********************************************************************************************************************************
TLS Server
***********************************************************************************************************************************/
#include <build.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/err.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/io/server.h"
#include "common/io/tls/common.h"
#include "common/io/tls/server.h"
#include "common/io/tls/session.h"
#include "common/log.h"
#include "common/stat.h"
#include "common/type/object.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(TLS_STAT_SERVER_STR,                                  TLS_STAT_SERVER);

/***********************************************************************************************************************************
Curve used to generate ephemeral Elliptic Curve DH keys
***********************************************************************************************************************************/
#define ECDH_CURVE                                                  "prime256v1"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TlsServer
{
    String *host;                                                   // Host
    SSL_CTX *context;                                               // TLS context
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifyPeer;                                                // Will the client cert be verified?
} TlsServer;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static void
tlsServerToLog(const THIS_VOID, StringStatic *const debugLog)
{
    THIS(const TlsServer);

    strStcFmt(debugLog, "{host: %s, timeout: %" PRIu64 "}", strZ(this->host), this->timeout);
}

#define FUNCTION_LOG_TLS_SERVER_TYPE                                                                                               \
    TlsServer *
#define FUNCTION_LOG_TLS_SERVER_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_OBJECT_FORMAT(value, tlsServerToLog, buffer, bufferSize)

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
Authenticate client

Adapted from PostgreSQL be_tls_open_server() in src/backend/libpq/be-secure-openssl.c.
***********************************************************************************************************************************/
static void
tlsServerAuth(const TlsServer *const this, IoSession *const ioSession, SSL *const tlsSession)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_SERVER, this);
        FUNCTION_LOG_PARAM(IO_SESSION, ioSession);
        FUNCTION_LOG_PARAM_P(VOID, tlsSession);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If peer verification requested
        if (this->verifyPeer)                                                                                       // {vm_covered}
        {
            // If the client cert was presented then the session is authenticated. An error will be thrown automatically if the
            // client cert is not valid.
            X509 *const clientCert = SSL_get_peer_certificate(tlsSession);                                          // {vm_covered}
            ioSessionAuthenticatedSet(ioSession, clientCert != NULL);                                               // {vm_covered}

            // Set the peer name to the client cert common name
            if (clientCert != NULL)                                                                                 // {vm_covered}
                ioSessionPeerNameSet(ioSession, tlsCertCommonName(clientCert));                                     // {vm_covered}

            // Free the cert
            X509_free(clientCert);                                                                                  // {vm_covered}
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
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
        // Open TLS session
        SSL *tlsSession = SSL_new(this->context);
        result = tlsSessionNew(tlsSession, ioSession, this->timeout);

        // Authenticate TLS session
        tlsServerAuth(this, result, tlsSession);

        // Move session
        ioSessionMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    statInc(TLS_STAT_SESSION_STR);

    FUNCTION_LOG_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
static const String *
tlsServerName(THIS_VOID)                                                                                            // {vm_covered}
{
    THIS(TlsServer);                                                                                                // {vm_covered}

    FUNCTION_TEST_BEGIN();                                                                                          // {vm_covered}
        FUNCTION_TEST_PARAM(TLS_SERVER, this);                                                                      // {vm_covered}
    FUNCTION_TEST_END();                                                                                            // {vm_covered}

    ASSERT(this != NULL);                                                                                           // {vm_covered}

    FUNCTION_TEST_RETURN_CONST(STRING, this->host);                                                                 // {vm_covered}
}

/***********************************************************************************************************************************
Initialize TLS context with all required security features

Adapted from PostgreSQL be_tls_init() in src/backend/libpq/be-secure-openssl.c.
***********************************************************************************************************************************/
static const IoServerInterface tlsServerInterface =
{
    .type = IO_SERVER_TLS_TYPE,
    .name = tlsServerName,
    .accept = tlsServerAccept,
    .toLog = tlsServerToLog,
};

FN_EXTERN IoServer *
tlsServerNew(
    const String *const host, const String *const caFile, const String *const keyFile, const String *const certFile,
    const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, keyFile);
        FUNCTION_LOG_PARAM(STRING, certFile);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(keyFile != NULL);
    ASSERT(certFile != NULL);

    OBJ_NEW_BEGIN(TlsServer, .childQty = MEM_CONTEXT_QTY_MAX, .callbackQty = 1)
    {
        *this = (TlsServer)
        {
            .host = strDup(host),
            .context = tlsContext(true),
            .timeout = timeout,
        };

        // Set callback to free context
        memContextCallbackSet(objMemContext(this), tlsServerFreeResource, this);

        // Set options
        SSL_CTX_set_options(
            this->context,
            // Let server set cipher order
            SSL_OP_CIPHER_SERVER_PREFERENCE |
#ifdef SSL_OP_NO_RENEGOTIATION
            // Disable renegotiation, available since 1.1.0h. This affects only TLSv1.2 and older protocol versions as TLSv1.3 has
            // no support for renegotiation.
            SSL_OP_NO_RENEGOTIATION |
#endif
            // Disable session tickets
            SSL_OP_NO_TICKET);

        // Disable session caching
        SSL_CTX_set_session_cache_mode(this->context, SSL_SESS_CACHE_OFF);

        // Set parameters for generating ephemeral DH keys. Auto selects parameters that match the strength of the certificate key.
        // Also see https://weakdh.org and https://en.wikipedia.org/wiki/Logjam_(computer_security).
        cryptoError(SSL_CTX_set_dh_auto(this->context, 1) != 1, "unable to set auto dh parameters");

        // Set the curve used to generate ephemeral Elliptic Curve DH keys
        cryptoError(SSL_CTX_set1_groups_list(this->context, ECDH_CURVE) != 1, "unable to set ecdh curve " ECDH_CURVE);

        // Load certificate and key
        tlsCertKeyLoad(this->context, certFile, keyFile);

        // If a CA store is specified then client certificates will be verified
        // -------------------------------------------------------------------------------------------------------------------------
        if (caFile != NULL)                                                                                         // {vm_covered}
        {
            // Load CA store
            cryptoError(                                                                                            // {vm_covered}
                SSL_CTX_load_verify_locations(this->context, strZ(caFile), NULL) != 1,                              // {vm_covered}
                zNewFmt("unable to load CA file '%s'", strZ(caFile)));                                              // {vm_covered}

            // Tell OpenSSL to send the list of root certs we trust to clients in CertificateRequests. This lets a client with a
            // keystore select the appropriate client certificate to send to us. Also, this ensures that the SSL context will own
            // the rootCertList and free it when no longer needed.
            STACK_OF(X509_NAME) *rootCertList = SSL_load_client_CA_file(strZ(caFile));                              // {vm_covered}
            cryptoError(rootCertList == NULL, zNewFmt("unable to generate CA list from '%s'", strZ(caFile)));       // {vm_covered}

            SSL_CTX_set_client_CA_list(this->context, rootCertList);                                                // {vm_covered}

            // Always ask for SSL client cert, but don't fail when not presented. In this case the server will disconnect after
            // sending a data end message to the client. The client can use this to verify that the server is running without the
            // need to authenticate.
            SSL_CTX_set_verify(this->context, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, NULL);                      // {vm_covered}

            // Set a flag so the client cert will be checked later
            this->verifyPeer = true;                                                                                // {vm_covered}
        }
    }
    OBJ_NEW_END();

    statInc(TLS_STAT_SERVER_STR);

    FUNCTION_LOG_RETURN(IO_SERVER, ioServerNew(this, &tlsServerInterface));
}
