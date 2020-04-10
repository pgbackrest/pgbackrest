/***********************************************************************************************************************************
TLS Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <strings.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "common/crypto/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/io/tls/client.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/write.intern.h"
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

    SocketSession *socketSession;                                   // Socket session
    SSL_CTX *context;                                               // TLS context
    SSL *session;                                                   // TLS session on the socket
    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
};

OBJECT_DEFINE_GET(IoRead, , TLS_CLIENT, IoRead *, read);
OBJECT_DEFINE_GET(IoWrite, , TLS_CLIENT, IoWrite *, write);

OBJECT_DEFINE_FREE(TLS_CLIENT);

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TLS_CLIENT, LOG, logLevelTrace)
{
    SSL_free(this->session);
    SSL_CTX_free(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Process results and report errors. Returns true if the command should continue and false if it should exit.
***********************************************************************************************************************************/
static int
tlsResult(TlsClient *this, int result, bool closeOk)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
        FUNCTION_LOG_PARAM(INT, result);
        FUNCTION_LOG_PARAM(BOOL, closeOk);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);

    if (result <= 0)
    {
        // Get tls error and store errno in case of syscall error
        int error = SSL_get_error(this->session, result);
        int errNo = errno;

        LOG_DEBUG_FMT("tls error %d, sys error %d", error, errNo);

        ERR_clear_error();

        switch (error)
        {
            // The connection was closed
            case SSL_ERROR_ZERO_RETURN:
            {
                tlsClientClose(this);

                if (!closeOk)
                    THROW(ProtocolError, "unexpected eof");

                result = -1;
                break;
            }

            // Try the read/write again
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
            {
                sckSessionReady(this->socketSession, error == SSL_ERROR_WANT_READ, error == SSL_ERROR_WANT_WRITE);

                result = 0;
                break;
            }

            // A syscall failed (this usually indicates eof)
            case SSL_ERROR_SYSCALL:
            {
                // Get the error before closing so it is not cleared
                tlsClientClose(this);

                // Throw the sys error if there is one
                THROW_SYS_ERROR_CODE(errNo, KernelError, "tls failed syscall");
                break;
            }

            // Some other tls error that cannot be handled
            default:
                THROW_FMT(ServiceError, "tls error [%d]", error);
        }
    }

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
TlsClient *
tlsClientNewServer(TimeMSec timeout, const String *certificateFile, const String *privateKeyFile)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(STRING, certificateFile);
        FUNCTION_LOG_PARAM(STRING, privateKeyFile);
    FUNCTION_LOG_END();

    ASSERT(certificateFile != NULL);
    ASSERT(privateKeyFile != NULL);

    TlsClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsClient")
    {
        this = memNew(sizeof(TlsClient));

        *this = (TlsClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .timeout = timeout,
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

        // Set certificate and private key
        cryptoError(
            SSL_CTX_use_certificate_file(this->context, strPtr(certificateFile), SSL_FILETYPE_PEM) <= 0,
            "unable to load certificate");
        cryptoError(
            SSL_CTX_use_PrivateKey_file(this->context, strPtr(privateKeyFile), SSL_FILETYPE_PEM) <= 0,
            "unable to load private key");

        tlsClientStatLocal.object++;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(TLS_CLIENT, this);
}

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

            sk_GENERAL_NAME_free(altNameStack);
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
Read from the TLS session
***********************************************************************************************************************************/
size_t
tlsClientRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(TlsClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
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

    // If blocking read keep reading until buffer is full or eof
    do
    {
        // Read and handle errors
        size_t expectedBytes = bufRemains(buffer);
        result = tlsResult(this, SSL_read(this->session, bufRemainsPtr(buffer), (int)expectedBytes), true);

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

void
tlsClientWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(TlsClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);
    ASSERT(buffer != NULL);

    int result = 0;

    do
    {
        result = tlsResult(this, SSL_write(this->session, bufPtrConst(buffer), (int)bufUsed(buffer)), false);

        CHECK(result == 0 || (size_t)result == bufUsed(buffer));
    }
    while (result == 0);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Close the connection
***********************************************************************************************************************************/
void
tlsClientClose(TlsClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Free the TLS session
    if (this->session != NULL)
    {
        SSL_shutdown(this->session);
        SSL_free(this->session);
        this->session = NULL;

        // Free the socket
        sckSessionFree(this->socketSession);
        this->socketSession = NULL;
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Has session been closed by the server?
***********************************************************************************************************************************/
bool
tlsClientEof(THIS_VOID)
{
    THIS(TlsClient);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    FUNCTION_LOG_RETURN(BOOL, this->session == NULL);
}

/***********************************************************************************************************************************
Accept a connection
***********************************************************************************************************************************/
void
tlsClientAccept(TlsClient *this, SocketSession *socketSession)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
        FUNCTION_LOG_PARAM(SOCKET_SESSION, socketSession);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(socketSession != NULL);

    // !!! THIS SHOULD BE A MOVE, BUT THIS IS TEMP CODE
    this->socketSession = socketSession;

    // Free the read/write interfaces
    ioReadFree(this->read);
    this->read = NULL;
    ioWriteFree(this->write);
    this->write = NULL;

    // Negotiate TLS
    cryptoError((this->session = SSL_new(this->context)) == NULL, "unable to create TLS context");

    cryptoError(
        SSL_set_fd(this->session, sckSessionFd(this->socketSession)) != 1, "unable to add socket to TLS context");

    // Keep trying the accept until success or error
    while (tlsResult(this, SSL_accept(this->session), false) == 0);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Create read and write interfaces
        this->write = ioWriteNewP(this, .write = tlsClientWrite);
        ioWriteOpen(this->write);
        this->read = ioReadNewP(this, .block = true, .eof = tlsClientEof, .read = tlsClientRead);
        ioReadOpen(this->read);
    }
    MEM_CONTEXT_END();

    tlsClientStatLocal.session++;

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open connection if this is a new client or if the connection was closed by the server
***********************************************************************************************************************************/
bool
tlsClientOpen(TlsClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace)
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    if (this->session == NULL)
    {
        // Free the read/write interfaces
        ioReadFree(this->read);
        this->read = NULL;
        ioWriteFree(this->write);
        this->write = NULL;

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
                    // Open the socket
                    MEM_CONTEXT_BEGIN(this->memContext)
                    {
                        this->socketSession = sckClientOpen(this->socketClient);
                    }
                    MEM_CONTEXT_END();

                    // Negotiate TLS
                    cryptoError((this->session = SSL_new(this->context)) == NULL, "unable to create TLS context");

                    cryptoError(
                        SSL_set_tlsext_host_name(this->session, strPtr(sckSessionHost(this->socketSession))) != 1,
                        "unable to set TLS host name");
                    cryptoError(
                        SSL_set_fd(this->session, sckSessionFd(this->socketSession)) != 1, "unable to add socket to TLS context");

                    // Keep trying the connection until success or error
                    while (tlsResult(this, SSL_connect(this->session), false) == 0);

                    // Connection was successful
                    connected = true;
                }
                CATCH_ANY()
                {
                    // Retry if wait time has not expired
                    if (waitMore(wait))
                    {
                        LOG_DEBUG_FMT("retry %s: %s", errorTypeName(errorType()), errorMessage());
                        retry = true;

                        tlsClientStatLocal.retry++;
                    }

                    tlsClientClose(this);
                }
                TRY_END();
            }
            while (!connected && retry);

            if (!connected)
                RETHROW();
        }
        MEM_CONTEXT_TEMP_END();

        // Verify that the certificate presented by the server is valid
        if (this->verifyPeer)
        {
            // Verify that the chain of trust leads to a valid CA
            long int verifyResult = SSL_get_verify_result(this->session);

            if (verifyResult != X509_V_OK)
            {
                THROW_FMT(
                    CryptoError, "unable to verify certificate presented by '%s:%u': [%ld] %s",
                    strPtr(sckSessionHost(this->socketSession)), sckSessionPort(this->socketSession), verifyResult,
                    X509_verify_cert_error_string(verifyResult));
            }

            // Verify that the hostname appears in the certificate
            X509 *certificate = SSL_get_peer_certificate(this->session);
            bool nameResult = tlsClientHostVerify(sckSessionHost(this->socketSession), certificate);
            X509_free(certificate);

            if (!nameResult)
            {
                THROW_FMT(
                    CryptoError,
                    "unable to find hostname '%s' in certificate common name or subject alternative names",
                    strPtr(sckSessionHost(this->socketSession)));
            }
        }

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // Create read and write interfaces
            this->write = ioWriteNewP(this, .write = tlsClientWrite);
            ioWriteOpen(this->write);
            this->read = ioReadNewP(this, .block = true, .eof = tlsClientEof, .read = tlsClientRead);
            ioReadOpen(this->read);
        }
        MEM_CONTEXT_END();

        tlsClientStatLocal.session++;
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
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
