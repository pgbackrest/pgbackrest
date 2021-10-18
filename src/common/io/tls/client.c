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
#include "common/io/client.h"
#include "common/io/io.h"
#include "common/io/tls/client.h"
#include "common/io/tls/session.h"
#include "common/stat.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Statistics constants
***********************************************************************************************************************************/
STRING_EXTERN(TLS_STAT_CLIENT_STR,                                  TLS_STAT_CLIENT);
STRING_EXTERN(TLS_STAT_RETRY_STR,                                   TLS_STAT_RETRY);
STRING_EXTERN(TLS_STAT_SESSION_STR,                                 TLS_STAT_SESSION);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TlsClient
{
    const String *host;                                             // Host to use for peer verification
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifyPeer;                                                // Should the peer (server) certificate be verified?
    IoClient *ioClient;                                             // Underlying client (usually a SocketClient)

    SSL_CTX *context;                                               // TLS context
} TlsClient;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
static String *
tlsClientToLog(const THIS_VOID)
{
    THIS(const TlsClient);

    return strNewFmt(
        "{ioClient: %s, timeout: %" PRIu64", verifyPeer: %s}",
        objMemContextFreeing(this) ? NULL_Z : strZ(ioClientToLog(this->ioClient)), this->timeout,
        cvtBoolToConstZ(this->verifyPeer));
}

#define FUNCTION_LOG_TLS_CLIENT_TYPE                                                                                               \
    TlsClient *
#define FUNCTION_LOG_TLS_CLIENT_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, tlsClientToLog, buffer, bufferSize)

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
static void
tlsClientFreeResource(THIS_VOID)
{
    THIS(TlsClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    SSL_CTX_free(this->context);

    FUNCTION_LOG_RETURN_VOID();
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
    if (nameAsn1 == NULL)                                                                                           // {vm_covered}
        THROW(CryptoError, "TLS certificate name entry is missing");

    FUNCTION_TEST_RETURN(                                                                                           // {vm_covered}
        strNewZN(
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
    if (strlen(strZ(name)) != strSize(name))
        THROW(CryptoError, "TLS certificate name contains embedded null");

    bool result = false;

    // Try an exact match
    if (strcasecmp(strZ(name), strZ(host)) == 0)                                                                    // {vm_covered}
    {
        result = true;                                                                                              // {vm_covered}
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
    else if (strZ(name)[0] == '*' && strZ(name)[1] == '.' && strSize(name) > 2 &&                                   // {vm_covered}
             strSize(name) < strSize(host) &&                                                                       // {vm_covered}
             strcasecmp(strZ(name) + 1, strZ(host) + strSize(host) - strSize(name) + 1) == 0 &&                     // {vm_covered}
             strChr(host, '.') >= (int)(strSize(host) - strSize(name)))                                             // {vm_covered}
    {
        result = true;                                                                                              // {vm_covered}
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
    if (certificate == NULL)                                                                                        // {vm_covered}
        THROW(CryptoError, "No certificate presented by the TLS server");                                           // {vm_covered}

    MEM_CONTEXT_TEMP_BEGIN()                                                                                        // {vm_covered}
    {
        // First get the subject alternative names from the certificate and compare them against the hostname
        STACK_OF(GENERAL_NAME) *altNameStack = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(                          // {vm_covered}
            certificate, NID_subject_alt_name, NULL, NULL);                                                         // {vm_covered}
        bool altNameFound = false;                                                                                  // {vm_covered}

        if (altNameStack)                                                                                           // {vm_covered}
        {
            for (int altNameIdx = 0; altNameIdx < sk_GENERAL_NAME_num(altNameStack); altNameIdx++)                  // {vm_covered}
            {
                const GENERAL_NAME *name = sk_GENERAL_NAME_value(altNameStack, altNameIdx);                         // {vm_covered}
                altNameFound = true;                                                                                // {vm_covered}

                if (name->type == GEN_DNS)                                                                          // {vm_covered}
                    result = tlsClientHostVerifyName(host, asn1ToStr(name->d.dNSName));                             // {vm_covered}

                if (result != false)                                                                                // {vm_covered}
                    break;                                                                                          // {vm_covered}
            }

            sk_GENERAL_NAME_pop_free(altNameStack, GENERAL_NAME_free);                                              // {vm_covered}
        }

        // If no subject alternative name was found then check the common name. Per RFC 2818 and RFC 6125, if the subjectAltName
        // extension of type dNSName is present the CN must be ignored.
        if (!altNameFound)                                                                                          // {vm_covered}
        {
            X509_NAME *subjectName = X509_get_subject_name(certificate);                                            // {vm_covered}
            CHECK(subjectName != NULL);                                                                             // {vm_covered}

            int commonNameIndex = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);                      // {vm_covered}
            CHECK(commonNameIndex >= 0);                                                                            // {vm_covered}

            result = tlsClientHostVerifyName(                                                                       // {vm_covered}
                host,                                                                                               // {vm_covered}
                asn1ToStr(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, commonNameIndex))));            // {vm_covered}
        }
    }
    MEM_CONTEXT_TEMP_END();                                                                                         // {vm_covered}

    FUNCTION_LOG_RETURN(BOOL, result);                                                                              // {vm_covered}
}

/***********************************************************************************************************************************
Open connection if this is a new client or if the connection was closed by the server
***********************************************************************************************************************************/
static IoSession *
tlsClientOpen(THIS_VOID)
{
    THIS(TlsClient);

    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    IoSession *result = NULL;
    SSL *session = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        bool retry;
        Wait *wait = waitNew(this->timeout);

        do
        {
            // Assume there will be no retry
            retry = false;

            TRY_BEGIN()
            {
                // Open the underlying session first since this is mostly likely to fail
                IoSession *ioSession = ioClientOpen(this->ioClient);

                // Create internal TLS session. If there is a failure before the TlsSession object is created there may be a leak
                // of the TLS session but this is likely to result in program termination so it doesn't seem worth coding for.
                cryptoError((session = SSL_new(this->context)) == NULL, "unable to create TLS session");

                // Set server host name used for validation
                cryptoError(SSL_set_tlsext_host_name(session, strZ(this->host)) != 1, "unable to set TLS host name");

                // Create the TLS session
                result = tlsSessionNew(session, ioSession, this->timeout);
            }
            CATCH_ANY()
            {
                result = NULL;

                // Retry if wait time has not expired
                if (waitMore(wait))
                {
                    LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                    retry = true;

                    statInc(TLS_STAT_RETRY_STR);
                }
                else
                    RETHROW();
            }
            TRY_END();
        }
        while (retry);

        ioSessionMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    statInc(TLS_STAT_SESSION_STR);

    // Verify that the certificate presented by the server is valid
    if (this->verifyPeer)                                                                                           // {vm_covered}
    {
        // Verify that the chain of trust leads to a valid CA
        long int verifyResult = SSL_get_verify_result(session);                                                     // {vm_covered}

        if (verifyResult != X509_V_OK)                                                                              // {vm_covered}
        {
            THROW_FMT(                                                                                              // {vm_covered}
                CryptoError, "unable to verify certificate presented by '%s': [%ld] %s",                            // {vm_covered}
                strZ(ioClientName(this->ioClient)), verifyResult, X509_verify_cert_error_string(verifyResult));     // {vm_covered}
        }

        // Verify that the hostname appears in the certificate
        X509 *certificate = SSL_get_peer_certificate(session);                                                      // {vm_covered}
        bool nameResult = tlsClientHostVerify(this->host, certificate);                                             // {vm_covered}
        X509_free(certificate);                                                                                     // {vm_covered}

        if (!nameResult)                                                                                            // {vm_covered}
        {
            THROW_FMT(                                                                                              // {vm_covered}
                CryptoError,                                                                                        // {vm_covered}
                "unable to find hostname '%s' in certificate common name or subject alternative names",             // {vm_covered}
                strZ(this->host));                                                                                  // {vm_covered}
        }
    }

    FUNCTION_LOG_RETURN(IO_SESSION, result);
}

/**********************************************************************************************************************************/
static const String *
tlsClientName(THIS_VOID)
{
    THIS(TlsClient);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(ioClientName(this->ioClient));
}

/**********************************************************************************************************************************/
static const IoClientInterface tlsClientInterface =
{
    .type = IO_CLIENT_TLS_TYPE,
    .name = tlsClientName,
    .open = tlsClientOpen,
    .toLog = tlsClientToLog,
};

IoClient *
tlsClientNew(IoClient *ioClient, const String *host, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_CLIENT, ioClient);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(ioClient != NULL);

    IoClient *this = NULL;

    OBJ_NEW_BEGIN(TlsClient)
    {
        TlsClient *driver = OBJ_NEW_ALLOC();

        *driver = (TlsClient)
        {
            .ioClient = ioClientMove(ioClient, MEM_CONTEXT_NEW()),
            .host = strDup(host),
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
        driver->context = SSL_CTX_new(method);
        cryptoError(driver->context == NULL, "unable to create TLS context");

        memContextCallbackSet(objMemContext(driver), tlsClientFreeResource, driver);

        // Exclude SSL versions to only allow TLS and also disable compression
        SSL_CTX_set_options(driver->context, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);

        // Disable auto-retry to prevent SSL_read() from hanging
        SSL_CTX_clear_mode(driver->context, SSL_MODE_AUTO_RETRY);

        // Set location of CA certificates if the server certificate will be verified
        // -------------------------------------------------------------------------------------------------------------------------
        if (driver->verifyPeer)
        {
            // If the user specified a location
            if (caFile != NULL || caPath != NULL)                                                                   // {vm_covered}
            {
                cryptoError(                                                                                        // {vm_covered}
                    SSL_CTX_load_verify_locations(driver->context, strZNull(caFile), strZNull(caPath)) != 1,        // {vm_covered}
                    "unable to set user-defined CA certificate location");                                          // {vm_covered}
            }
            // Else use the defaults
            else
            {
                cryptoError(
                    SSL_CTX_set_default_verify_paths(driver->context) != 1, "unable to set default CA certificate location");
            }
        }

        statInc(TLS_STAT_CLIENT_STR);

        // Create client interface
        this = ioClientNew(driver, &tlsClientInterface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(IO_CLIENT, this);
}
