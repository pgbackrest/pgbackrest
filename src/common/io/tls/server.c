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
#include "common/io/tls/common.h"
#include "common/io/tls/server.h"
#include "common/io/tls/session.h"
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
    String *host;                                                   // Host
    SSL_CTX *context;                                               // TLS context
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifyPeer;                                                // Will the client cert be verified?
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
Set DH parameters for generating ephemeral DH keys. The DH parameters can take a long time to compute, so they must be precomputed
using parameters provided by the OpenSSL project.

These values can be static since the OpenSSL library can efficiently generate random keys from the information provided.

Adapted from PostgreSQL initialize_dh() in src/backend/libpq/be-secure-openssl.c. Also see https://weakdh.org and
https://en.wikipedia.org/wiki/Logjam_(computer_security).
***********************************************************************************************************************************/
// Hardcoded DH parameters, used in ephemeral DH keying. This is the 2048-bit DH parameter from RFC 3526. The generation of the
// prime is specified in RFC 2412 Appendix E, which also discusses the design choice of the generator. Note that when loaded with
// OpenSSL this causes DH_check() to fail on DH_NOT_SUITABLE_GENERATOR, where leaking a bit is preferred.
#define DH_2048                                                                                                                    \
    "-----BEGIN DH PARAMETERS-----\n"                                                                                              \
    "MIIBCAKCAQEA///////////JD9qiIWjCNMTGYouA3BzRKQJOCIpnzHQCC76mOxOb\n"                                                           \
    "IlFKCHmONATd75UZs806QxswKwpt8l8UN0/hNW1tUcJF5IW1dmJefsb0TELppjft\n"                                                           \
    "awv/XLb0Brft7jhr+1qJn6WunyQRfEsf5kkoZlHs5Fs9wgB8uKFjvwWY2kg2HFXT\n"                                                           \
    "mmkWP6j9JM9fg2VdI9yjrZYcYvNWIIVSu57VKQdwlpZtZww1Tkq8mATxdGwIyhgh\n"                                                           \
    "fDKQXkYuNs474553LBgOhgObJ4Oi7Aeij7XFXfBvTFLJ3ivL9pVYFxg5lUl86pVq\n"                                                           \
    "5RXSJhiY+gUQFXKOWoqsqmj//////////wIBAg==\n"                                                                                   \
    "-----END DH PARAMETERS-----"

static void
tlsServerDh(SSL_CTX *const context)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, context);
    FUNCTION_TEST_END();

    SSL_CTX_set_options(context, SSL_OP_SINGLE_DH_USE);

	BIO *const bio = BIO_new_mem_buf(DH_2048, sizeof(DH_2048));
    cryptoError(bio == NULL, "unable create buffer for DH parameters");

    TRY_BEGIN()
    {
    	DH *const dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);

        TRY_BEGIN()
        {
            cryptoError(SSL_CTX_set_tmp_dh(context, dh) != 1, "unable to set temp dh parameters");
        }
        FINALLY()
        {
            DH_free(dh);
        }
        TRY_END();
    }
    FINALLY()
    {
        BIO_free(bio);
    }
    TRY_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Set ECDH parameters for generating ephemeral Elliptic Curve DH keys.

Adapted from PostgreSQL initialize_ecdh() in src/backend/libpq/be-secure-openssl.c.
***********************************************************************************************************************************/
#define ECHD_CURVE                                                  "prime256v1"

static void
tlsServerEcdh(SSL_CTX *const context)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, context);
    FUNCTION_TEST_END();

    const int nid = OBJ_sn2nid(ECHD_CURVE);
    cryptoError(nid == NID_undef, "unrecognized ECDH curve " ECHD_CURVE);

    EC_KEY *const ecdh = EC_KEY_new_by_curve_name(nid);
    cryptoError(ecdh == NULL, "could not create ecdh key");

    SSL_CTX_set_options(context, SSL_OP_SINGLE_ECDH_USE);

    TRY_BEGIN()
    {
        cryptoError(SSL_CTX_set_tmp_ecdh(context, ecdh) != 1, "unable to set temp ecdh key");
    }
    FINALLY()
    {
        EC_KEY_free(ecdh);
    }
    TRY_END();

    FUNCTION_TEST_RETURN_VOID();
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

    FUNCTION_TEST_RETURN(this->host);                                                                               // {vm_covered}
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

IoServer *
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

    IoServer *this = NULL;

    OBJ_NEW_BEGIN(TlsServer)
    {
        TlsServer *const driver =  OBJ_NEW_ALLOC();

        *driver = (TlsServer)
        {
            .host = strDup(host),
            .context = tlsContext(),
            .timeout = timeout,
        };

        // Set callback to free context
        memContextCallbackSet(objMemContext(driver), tlsServerFreeResource, driver);

        // Set options
        SSL_CTX_set_options(driver->context,
            // Disable SSL and TLS v1/v1.1
            SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 |
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
        SSL_CTX_set_session_cache_mode(driver->context, SSL_SESS_CACHE_OFF);

        // Setup ephemeral DH and ECDH keys
        tlsServerDh(driver->context);
        tlsServerEcdh(driver->context);

        // Load certificate and key
        tlsCertKeyLoad(driver->context, certFile, keyFile);

        // If a CA store is specified then client certificates will be verified
        // -------------------------------------------------------------------------------------------------------------------------
        if (caFile != NULL)                                                                                         // {vm_covered}
        {
            // Load CA store
            cryptoError(                                                                                            // {vm_covered}
                SSL_CTX_load_verify_locations(driver->context, strZ(caFile), NULL) != 1,                            // {vm_covered}
                strZ(strNewFmt("unable to load CA file '%s'", strZ(caFile))));                                      // {vm_covered}

            // Tell OpenSSL to send the list of root certs we trust to clients in CertificateRequests. This lets a client with a
            // keystore select the appropriate client certificate to send to us. Also, this ensures that the SSL context will own
            // the rootCertList and free it when no longer needed.
            STACK_OF(X509_NAME) *rootCertList = SSL_load_client_CA_file(strZ(caFile));                              // {vm_covered}
            cryptoError(                                                                                            // {vm_covered}
                rootCertList == NULL, strZ(strNewFmt("unable to generate CA list from '%s'", strZ(caFile))));       // {vm_covered}

            SSL_CTX_set_client_CA_list(driver->context, rootCertList);                                              // {vm_covered}

            // Always ask for SSL client cert, but don't fail when not presented. In this case the server will disconnect after
            // sending a data end message to the client. The client can use this to verify that the server is running without the
            // need to authenticate.
            SSL_CTX_set_verify(driver->context, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, NULL);                    // {vm_covered}

            // Set a flag so the client cert will be checked later
            driver->verifyPeer = true;                                                                              // {vm_covered}
        }

        statInc(TLS_STAT_SERVER_STR);

        this = ioServerNew(driver, &tlsServerInterface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_SERVER, this);
}
