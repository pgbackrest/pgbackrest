/***********************************************************************************************************************************
TLS Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <strings.h>

#include <openssl/x509v3.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/io/io.h"
#include "common/io/tls/client.h"
#include "common/io/tls/session.intern.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Statistics
***********************************************************************************************************************************/
static TlsClientStat tlsClientStatLocal;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct TlsClient
{
    MemContext *memContext;                                         // Mem context
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifyPeer;                                                // Should the peer (server) certificate be verified?
    SocketClient *socketClient;                                     // Socket client

    SSL_CTX *context;                                               // TLS context
};

OBJECT_DEFINE_FREE(TLS_CLIENT);

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TLS_CLIENT, LOG, logLevelTrace)
{
    SSL_CTX_free(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
TlsClient *
tlsClientNew(SocketClient *socket, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(SOCKET_CLIENT, socket);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(socket != NULL);

    TlsClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsClient")
    {
        this = memNew(sizeof(TlsClient));

        *this = (TlsClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .socketClient = sckClientMove(socket, MEM_CONTEXT_NEW()),
            .timeout = timeout,
            .verifyPeer = verifyPeer,
        };

        // Setup TLS context
        // -------------------------------------------------------------------------------------------------------------------------
        cryptoInit();

        // Select the TLS method to use.  To maintain compatibility with older versions of OpenSSL we need to use an SSL method,
        // but SSL versions will be excluded in SSL_CTX_set_options().
        const SSL_METHOD *method = SSLv23_method();
        cryptoError(method == NULL, "unable to load TLS method");

        // Create the TLS context
        this->context = SSL_CTX_new(method);
        cryptoError(this->context == NULL, "unable to create TLS context");

        memContextCallbackSet(this->memContext, tlsClientFreeResource, this);

        // Exclude SSL versions to only allow TLS and also disable compression
        SSL_CTX_set_options(this->context, (long)(SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION));

        // Disable auto-retry to prevent SSL_read() from hanging
        SSL_CTX_clear_mode(this->context, SSL_MODE_AUTO_RETRY);

        // Set location of CA certificates if the server certificate will be verified
        // -------------------------------------------------------------------------------------------------------------------------
        if (this->verifyPeer)
        {
            // If the user specified a location
            if (caFile != NULL || caPath != NULL)
            {
                cryptoError(
                    SSL_CTX_load_verify_locations(this->context, strPtr(caFile), strPtr(caPath)) != 1,
                    "unable to set user-defined CA certificate location");
            }
            // Else use the defaults
            else
                cryptoError(SSL_CTX_set_default_verify_paths(this->context) != 1, "unable to set default CA certificate location");
        }

        tlsClientStatLocal.object++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(TLS_CLIENT, this);
}

/***********************************************************************************************************************************
Convert an ASN1 string used in certificates to a String
***********************************************************************************************************************************/
static String *
asn1ToStr(ASN1_STRING *nameAsn1)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, nameAsn1);
    FUNCTION_TEST_END();

    // The name should not be null
    if (nameAsn1 == NULL)
        THROW(CryptoError, "TLS certificate name entry is missing");

    FUNCTION_TEST_RETURN(
        strNewN(
#if OPENSSL_VERSION_NUMBER < 0x10100000L
            (const char *)ASN1_STRING_data(nameAsn1),
#else
            (const char *)ASN1_STRING_get0_data(nameAsn1),
#endif
            (size_t)ASN1_STRING_length(nameAsn1)));
}

/***********************************************************************************************************************************
Check if a name from the server certificate matches the hostname

Matching is always case-insensitive since DNS is case insensitive.
***********************************************************************************************************************************/
static bool
tlsClientHostVerifyName(const String *host, const String *name)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(STRING, name);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);
    ASSERT(name != NULL);

    // Reject embedded nulls in certificate common or alternative name to prevent attacks like CVE-2009-4034
    if (strlen(strPtr(name)) != strSize(name))
        THROW(CryptoError, "TLS certificate name contains embedded null");

    bool result = false;

    // Try an exact match
    if (strcasecmp(strPtr(name), strPtr(host)) == 0)
    {
        result = true;
    }
    // Else check if a wildcard certificate matches the host name
    //
    // The rules are:
    // 1. Only match the '*' character as wildcard
    // 2. Only match wildcards at the start of the string
    // 3. The '*' character does *not* match '.', meaning that we match only a single pathname component.
    // 4. Don't support more than one '*' in a single pattern.
    //
    // This is roughly in line with RFC2818, but contrary to what most browsers appear to be implementing (point 3 being the
    // difference)
    else if (strPtr(name)[0] == '*' && strPtr(name)[1] == '.' && strSize(name) > 2 && strSize(name) < strSize(host) &&
             strcasecmp(strPtr(name) + 1, strPtr(host) + strSize(host) - strSize(name) + 1) == 0 &&
             strChr(host, '.') >= (int)(strSize(host) - strSize(name)))
    {
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Verify that the server certificate matches the hostname we connected to

The certificate's Common Name and Subject Alternative Names are considered.
***********************************************************************************************************************************/
static bool
tlsClientHostVerify(const String *host, X509 *certificate)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM_P(VOID, certificate);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    bool result = false;

    // Error if the certificate is NULL
    if (certificate == NULL)
        THROW(CryptoError, "No certificate presented by the TLS server");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // First get the subject alternative names from the certificate and compare them against the hostname
        STACK_OF(GENERAL_NAME) *altNameStack = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(
            certificate, NID_subject_alt_name, NULL, NULL);
        bool altNameFound = false;

        if (altNameStack)
        {
            for (int altNameIdx = 0; altNameIdx < sk_GENERAL_NAME_num(altNameStack); altNameIdx++)
            {
                const GENERAL_NAME *name = sk_GENERAL_NAME_value(altNameStack, altNameIdx);
                altNameFound = true;

                if (name->type == GEN_DNS)
                    result = tlsClientHostVerifyName(host, asn1ToStr(name->d.dNSName));

                if (result != false)
                    break;
            }

            sk_GENERAL_NAME_pop_free(altNameStack, GENERAL_NAME_free);
        }

        // If no subject alternative name was found then check the common name. Per RFC 2818 and RFC 6125, if the subjectAltName
        // extension of type dNSName is present the CN must be ignored.
        if (!altNameFound)
        {
            X509_NAME *subjectName = X509_get_subject_name(certificate);
            CHECK(subjectName != NULL);

            int commonNameIndex = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);
            CHECK(commonNameIndex >= 0);

            result = tlsClientHostVerifyName(
                host, asn1ToStr(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, commonNameIndex))));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Open connection if this is a new client or if the connection was closed by the server
***********************************************************************************************************************************/
TlsSession *
tlsClientOpen(TlsClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    TlsSession *result = NULL;
    SSL *session = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool connected = false;
        bool retry;
        Wait *wait = waitNew(this->timeout);

        do
        {
            // Assume there will be no retry
            retry = false;

            TRY_BEGIN()
            {
                // Create internal TLS session
                cryptoError((session = SSL_new(this->context)) == NULL, "unable to create TLS session");

                // Set server host name used for validation
                cryptoError(
                    SSL_set_tlsext_host_name(session, strPtr(sckClientHost(this->socketClient))) != 1,
                    "unable to set TLS host name");

                // Create the TLS session
                result = tlsSessionNew(session, sckClientOpen(this->socketClient), this->timeout);

                // Connection was successful
                connected = true;
            }
            CATCH_ANY()
            {
                tlsSessionFree(result);
                result = NULL;

                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    tlsClientStatLocal.retry++;
                }
            }
            TRY_END();
        }
        while (!connected && retry);

        if (!connected)
            RETHROW();

        tlsSessionMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    tlsClientStatLocal.session++;

    // Verify that the certificate presented by the server is valid
    if (this->verifyPeer)
    {
        // Verify that the chain of trust leads to a valid CA
        long int verifyResult = SSL_get_verify_result(session);

        if (verifyResult != X509_V_OK)
        {
            THROW_FMT(
                CryptoError, "unable to verify certificate presented by '%s:%u': [%ld] %s",
                strPtr(sckClientHost(this->socketClient)), sckClientPort(this->socketClient), verifyResult,
                X509_verify_cert_error_string(verifyResult));
        }

        // Verify that the hostname appears in the certificate
        X509 *certificate = SSL_get_peer_certificate(session);
        bool nameResult = tlsClientHostVerify(sckClientHost(this->socketClient), certificate);
        X509_free(certificate);

        if (!nameResult)
        {
            THROW_FMT(
                CryptoError,
                "unable to find hostname '%s' in certificate common name or subject alternative names",
                strPtr(sckClientHost(this->socketClient)));
        }
    }

    FUNCTION_LOG_RETURN(TLS_SESSION, result);
}

/**********************************************************************************************************************************/
String *
tlsClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (tlsClientStatLocal.object > 0)
    {
        result = strNewFmt(
            "tls statistics: objects %" PRIu64 ", sessions %" PRIu64 ", retries %" PRIu64, tlsClientStatLocal.object,
            tlsClientStatLocal.session, tlsClientStatLocal.retry);
    }

    FUNCTION_TEST_RETURN(result);
}
