/***********************************************************************************************************************************
TLS Client
***********************************************************************************************************************************/
#include "build.auto.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#include <openssl/conf.h>
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
#include "common/object.h"
#include "common/time.h"
#include "common/type/keyValue.h"
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
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifyPeer;                                                // Should the peer (server) certificate be verified?

    SSL_CTX *context;                                               // TLS context
    int socket;                                                     // Client socket
    SSL *session;                                                   // TLS session on the socket

    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
};

OBJECT_DEFINE_FREE(TLS_CLIENT);

/***********************************************************************************************************************************
Free connection
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(TLS_CLIENT, LOG, logLevelTrace)
{
    tlsClientClose(this);
    SSL_CTX_free(this->context);
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/***********************************************************************************************************************************
Report TLS errors.  Returns true if the command should continue and false if it should exit.
***********************************************************************************************************************************/
static bool
tlsError(TlsClient *this, int code)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
        FUNCTION_LOG_PARAM(INT, code);
    FUNCTION_LOG_END();

    bool result = false;

    switch (code)
    {
        // The connection was closed
        case SSL_ERROR_ZERO_RETURN:
        {
            tlsClientClose(this);
            break;
        }

        // Try the read/write again
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
        {
            result = true;
            break;
        }

        // A syscall failed (this usually indicates eof)
        case SSL_ERROR_SYSCALL:
        {
            // Get the error before closing so it is not cleared
            int errNo = errno;
            tlsClientClose(this);

            // Throw the sys error if there is one
            THROW_ON_SYS_ERROR(errNo, KernelError, "tls failed syscall");

            break;
        }

        // Some other tls error that cannot be handled
        default:
            THROW_FMT(ServiceError, "tls error [%d]", code);
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
TlsClient *
tlsClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifyPeer, const String *caFile, const String *caPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(UINT, port);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
        FUNCTION_LOG_PARAM(BOOL, verifyPeer);
        FUNCTION_LOG_PARAM(STRING, caFile);
        FUNCTION_LOG_PARAM(STRING, caPath);
    FUNCTION_LOG_END();

    ASSERT(host != NULL);

    TlsClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsClient")
    {
        this = memNew(sizeof(TlsClient));

        *this = (TlsClient)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .host = strDup(host),
            .port = port,
            .timeout = timeout,
            .verifyPeer = verifyPeer,

            // Initialize socket to -1 so we know when it is disconnected
            .socket = -1,
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
Wait for the socket to be readable
***********************************************************************************************************************************/
static void
tlsClientReadWait(TlsClient *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->session != NULL);

    // Initialize the file descriptor set used for select
    fd_set selectSet;
    FD_ZERO(&selectSet);

    // We know the socket is not negative because it passed error handling, so it is safe to cast to unsigned
    FD_SET((unsigned int)this->socket, &selectSet);

    // Initialize timeout struct used for select.  Recreate this structure each time since Linux (at least) will modify it.
    struct timeval timeoutSelect;
    timeoutSelect.tv_sec = (time_t)(this->timeout / MSEC_PER_SEC);
    timeoutSelect.tv_usec = (time_t)(this->timeout % MSEC_PER_SEC * 1000);

    // Determine if there is data to be read
    int result = select(this->socket + 1, &selectSet, NULL, NULL, &timeoutSelect);
    THROW_ON_SYS_ERROR_FMT(result == -1, AssertError, "unable to select from '%s:%u'", strPtr(this->host), this->port);

    // If no data read after time allotted then error
    if (!result)
    {
        THROW_FMT(
            FileReadError, "timeout after %" PRIu64 "ms waiting for read from '%s:%u'", this->timeout, strPtr(this->host),
            this->port);
    }

    FUNCTION_LOG_RETURN_VOID();
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

    // If blocking read keep reading until buffer is full
    do
    {
        // If no tls data pending then check the socket
        if (!SSL_pending(this->session))
            tlsClientReadWait(this);

        // Read and handle errors
        size_t expectedBytes = bufRemains(buffer);
        result = SSL_read(this->session, bufRemainsPtr(buffer), (int)expectedBytes);

        if (result <= 0)
        {
            // Break if the error indicates that we should not continue trying
            if (!tlsError(this, SSL_get_error(this->session, (int)result)))
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
tlsWriteContinue(TlsClient *this, int writeResult, int writeError, size_t writeSize)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(TLS_CLIENT, this);
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
            if (!tlsError(this, writeError))
                THROW_FMT(FileWriteError, "unable to write to tls [%d]", writeError);

            // Wait for the socket to be readable for tls renegotiation
            tlsClientReadWait(this);
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
    int error = SSL_ERROR_NONE;

    while (tlsWriteContinue(this, result, error, bufUsed(buffer)))
    {
        result = SSL_write(this->session, bufPtr(buffer), (int)bufUsed(buffer));
        error = SSL_get_error(this->session, result);
    }

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

    // Close the socket
    if (this->socket != -1)
    {
        close(this->socket);
        this->socket = -1;
    }

    // Free the TLS session
    if (this->session != NULL)
    {
        SSL_free(this->session);
        this->session = NULL;
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
            Wait *wait = this->timeout > 0 ? waitNew(this->timeout) : NULL;

            do
            {
                // Assume there will be no retry
                retry = false;

                TRY_BEGIN()
                {
                    // Set hits that narrow the type of address we are looking for -- we'll take ipv4 or ipv6
                    struct addrinfo hints = (struct addrinfo)
                    {
                        .ai_family = AF_UNSPEC,
                        .ai_socktype = SOCK_STREAM,
                        .ai_protocol = IPPROTO_TCP,
                    };

                    // Convert the port to a zero-terminated string for use with getaddrinfo()
                    char port[CVT_BASE10_BUFFER_SIZE];
                    cvtUIntToZ(this->port, port, sizeof(port));

                    // Get an address for the host.  We are only going to try the first address returned.
                    struct addrinfo *hostAddress;
                    int result;

                    if ((result = getaddrinfo(strPtr(this->host), port, &hints, &hostAddress)) != 0)
                    {
                        THROW_FMT(
                            HostConnectError, "unable to get address for '%s': [%d] %s", strPtr(this->host), result,
                            gai_strerror(result));
                    }

                    // Connect to the host
                    TRY_BEGIN()
                    {
                        this->socket = socket(hostAddress->ai_family, hostAddress->ai_socktype, hostAddress->ai_protocol);
                        THROW_ON_SYS_ERROR(this->socket == -1, HostConnectError, "unable to create socket");

                        if (connect(this->socket, hostAddress->ai_addr, hostAddress->ai_addrlen) == -1)
                            THROW_SYS_ERROR_FMT(HostConnectError, "unable to connect to '%s:%u'", strPtr(this->host), this->port);
                    }
                    FINALLY()
                    {
                        freeaddrinfo(hostAddress);
                    }
                    TRY_END();

                    // Enable TCP keepalives
                    int socketValue = 1;

                    THROW_ON_SYS_ERROR(
                        setsockopt(this->socket, SOL_SOCKET, SO_KEEPALIVE, &socketValue, sizeof(int)) == -1, ProtocolError,
                         "unable set SO_KEEPALIVE");

                    // Set per-connection keepalive options if they are available
#ifdef TCP_KEEPIDLE
                    socketValue = 3;

                    THROW_ON_SYS_ERROR(
                        setsockopt(this->socket, SOL_SOCKET, TCP_KEEPIDLE, &socketValue, sizeof(int)) == -1, ProtocolError,
                         "unable set SO_KEEPIDLE");

                    THROW_ON_SYS_ERROR(
                        setsockopt(this->socket, SOL_SOCKET, TCP_KEEPINTVL, &socketValue, sizeof(int)) == -1, ProtocolError,
                         "unable set SO_KEEPINTVL");

                    socketValue = (int)this->timeout / socketValue;

                    THROW_ON_SYS_ERROR(
                        setsockopt(this->socket, SOL_SOCKET, TCP_KEEPCNT, &socketValue, sizeof(int)) == -1, ProtocolError,
                         "unable set SO_KEEPCNT");
#endif

                    // Negotiate TLS
                    cryptoError((this->session = SSL_new(this->context)) == NULL, "unable to create TLS context");

                    cryptoError(SSL_set_tlsext_host_name(this->session, strPtr(this->host)) != 1, "unable to set TLS host name");
                    cryptoError(SSL_set_fd(this->session, this->socket) != 1, "unable to add socket to TLS context");
                    cryptoError(SSL_connect(this->session) != 1, "unable to negotiate TLS connection");

                    // Connection was successful
                    connected = true;
                }
                CATCH_ANY()
                {
                    // Retry if wait time has not expired
                    if (wait != NULL && waitMore(wait))
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
                    CryptoError, "unable to verify certificate presented by '%s:%u': [%ld] %s", strPtr(this->host),
                    this->port, verifyResult, X509_verify_cert_error_string(verifyResult));
            }

            // Verify that the hostname appears in the certificate
            X509 *certificate = SSL_get_peer_certificate(this->session);
            bool nameResult = tlsClientHostVerify(this->host, certificate);
            X509_free(certificate);

            if (!nameResult)
            {
                THROW_FMT(
                    CryptoError,
                    "unable to find hostname '%s' in certificate common name or subject alternative names", strPtr(this->host));
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

    tlsClientStatLocal.request++;

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Format statistics to a string
***********************************************************************************************************************************/
String *
tlsClientStatStr(void)
{
    FUNCTION_TEST_VOID();

    String *result = NULL;

    if (tlsClientStatLocal.object > 0)
    {
        result = strNewFmt(
            "tls statistics: objects %" PRIu64 ", sessions %" PRIu64 ", requests %" PRIu64 ", retries %" PRIu64,
            tlsClientStatLocal.object, tlsClientStatLocal.session, tlsClientStatLocal.request, tlsClientStatLocal.retry);
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
tlsClientIoRead(const TlsClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->read);
}

/***********************************************************************************************************************************
Get write interface
***********************************************************************************************************************************/
IoWrite *
tlsClientIoWrite(const TlsClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->write);
}
