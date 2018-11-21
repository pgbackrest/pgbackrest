/***********************************************************************************************************************************
TLS Client
***********************************************************************************************************************************/
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/io/tls/client.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/write.intern.h"
#include "common/memContext.h"
#include "common/time.h"
#include "common/type/keyValue.h"
#include "common/wait.h"
#include "crypto/crypto.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct TlsClient
{
    MemContext *memContext;                                         // Mem context
    String *host;                                                   // Hostname or IP address
    unsigned int port;                                              // Port to connect to host on
    TimeMSec timeout;                                               // Timeout for any i/o operation (connect, read, etc.)
    bool verifySsl;                                                 // Should SSL cert be verified?

    SSL_CTX *context;                                               // TLS/SSL context
    int socket;                                                     // Client socket
    SSL *session;                                                   // TLS/SSL session on the socket

    IoRead *read;                                                   // Read interface
    IoWrite *write;                                                 // Write interface
};

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
TlsClient *
tlsClientNew(
    const String *host, unsigned int port, TimeMSec timeout, bool verifySsl, const String *caFile, const String *caPath)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug)
        FUNCTION_DEBUG_PARAM(STRING, host);
        FUNCTION_DEBUG_PARAM(UINT, port);
        FUNCTION_DEBUG_PARAM(TIME_MSEC, timeout);
        FUNCTION_DEBUG_PARAM(BOOL, verifySsl);
        FUNCTION_DEBUG_PARAM(STRING, caFile);
        FUNCTION_DEBUG_PARAM(STRING, caPath);

        FUNCTION_TEST_ASSERT(host != NULL);
    FUNCTION_DEBUG_END();

    TlsClient *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("TlsClient")
    {
        this = memNew(sizeof(TlsClient));
        this->memContext = MEM_CONTEXT_NEW();

        this->host = strDup(host);
        this->port = port;
        this->timeout = timeout;
        this->verifySsl = verifySsl;

        // Initialize socket to -1 so we know when it is disconnected
        this->socket = -1;

        // Setup SSL context
        // -------------------------------------------------------------------------------------------------------------------------
        cryptoInit();

        // Select the TLS method to use.  To maintain compatability with older versions of OpenSSL we need to use an SSL method,
        // but SSL versions will be excluded in SSL_CTX_set_options().
        const SSL_METHOD *method = SSLv23_method();
        cryptoError(method == NULL, "unable to load TLS method");

        // Create the SSL context
        this->context = SSL_CTX_new(method);
        cryptoError(this->context == NULL, "unable to create TLS context");

        memContextCallback(this->memContext, (MemContextCallback)tlsClientFree, this);

        // Exclude SSL versions to only allow TLS and also disable compression
        SSL_CTX_set_options(this->context, (long)(SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION));

        // Set location of CA certificates if the server certificate will be verified
        // -------------------------------------------------------------------------------------------------------------------------
        if (this->verifySsl)
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
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(TLS_CLIENT, this);
}

/***********************************************************************************************************************************
Convert an ASN1 string used in certificates to a String
***********************************************************************************************************************************/
static String *
asn1ToStr(ASN1_STRING *nameAsn1)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VOIDP, nameAsn1);
    FUNCTION_TEST_END();

    // The name should not be null
    if (nameAsn1 == NULL)
        THROW(CryptoError, "TLS certificate name entry is missing");

    FUNCTION_TEST_RESULT(
        STRING, strNewN(
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
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, host);
        FUNCTION_DEBUG_PARAM(STRING, name);

        FUNCTION_TEST_ASSERT(host != NULL);
        FUNCTION_TEST_ASSERT(name != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Verify that the server certificate matches the hostname we connected to

The certificate's Common Name and Subject Alternative Names are considered.
***********************************************************************************************************************************/
static bool
tlsClientHostVerify(const String *host, X509 *certificate)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, host);
        FUNCTION_DEBUG_PARAM(VOIDP, certificate);

        FUNCTION_TEST_ASSERT(host != NULL);
    FUNCTION_DEBUG_END();

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

            if (subjectName != NULL)                            // {uncovered - not sure how to create cert with null common name}
            {
                int commonNameIndex = X509_NAME_get_index_by_NID(subjectName, NID_commonName, -1);

                if (commonNameIndex >= 0)                       // {uncovered - it seems this must be >= 0 if CN is not null}
                {
                    result = tlsClientHostVerifyName(
                        host, asn1ToStr(X509_NAME_ENTRY_get_data(X509_NAME_get_entry(subjectName, commonNameIndex))));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Open connection if this is a new client or if the connection was closed by the server
***********************************************************************************************************************************/
void
tlsClientOpen(TlsClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace)
        FUNCTION_DEBUG_PARAM(TLS_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

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
                    // Resolve the ip address.  We'll just blindly take the first address that comes up.
                    struct hostent *host_entry = NULL;

                    if ((host_entry = gethostbyname(strPtr(this->host))) == NULL)
                        THROW_FMT(FileOpenError, "unable to resolve host '%s'", strPtr(this->host));

                    // Connect to the server
                    this->socket = socket(AF_INET, SOCK_STREAM, 0);
                    THROW_ON_SYS_ERROR(this->socket == -1, FileOpenError, "unable to create socket");

                    struct sockaddr_in socketAddr;
                    memset(&socketAddr, 0, sizeof(socketAddr));
                    socketAddr.sin_family = AF_INET;
                    socketAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
                    socketAddr.sin_port = htons((uint16_t)this->port);

                    if (connect(this->socket, (struct sockaddr *)&socketAddr, sizeof(socketAddr)) == -1)
                        THROW_SYS_ERROR_FMT(FileOpenError, "unable to connect to '%s:%u'", strPtr(this->host), this->port);

                    // Negotiate SSL
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
                        LOG_DEBUG("retry %s: %s", errorTypeName(errorType()), errorMessage());
                        retry = true;
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
        if (this->verifySsl)
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
                THROW(CryptoError, "unable to find matching hostname in certificate");
        }

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            // Create read and write interfaces
            this->write = ioWriteNewP(this, .write = (IoWriteInterfaceWrite)tlsClientWrite);
            ioWriteOpen(this->write);
            this->read = ioReadNewP(this, .eof = (IoReadInterfaceEof)tlsClientEof, .read = (IoReadInterfaceRead)tlsClientRead);
            ioReadOpen(this->read);
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Read from the TLS session
***********************************************************************************************************************************/
size_t
tlsClientRead(TlsClient *this, Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(TLS_CLIENT, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->session != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(!bufFull(buffer));
    FUNCTION_DEBUG_END();

    ssize_t actualBytes = 0;

    // If no tls data pending then check the socket
    if (!SSL_pending(this->session))
    {
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
                FileReadError, "unable to read data from '%s:%u' after %" PRIu64 "ms",
                strPtr(this->host), this->port, this->timeout);
        }
    }

    // Read and handle errors
    size_t expectedBytes = bufRemains(buffer);
    actualBytes = SSL_read(this->session, bufRemainsPtr(buffer), (int)expectedBytes);

    cryptoError(actualBytes < 0, "unable to read from TLS");

    // Update amount of buffer used
    bufUsedInc(buffer, (size_t)actualBytes);

    // If zero bytes were returned then the connection was closed
    if (actualBytes == 0)
        tlsClientClose(this);

    FUNCTION_DEBUG_RESULT(SIZE, (size_t)actualBytes);
}

/***********************************************************************************************************************************
Write to the tls session
***********************************************************************************************************************************/
void
tlsClientWrite(TlsClient *this, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(TLS_CLIENT, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->session != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
    FUNCTION_DEBUG_END();

    cryptoError(SSL_write(this->session, bufPtr(buffer), (int)bufUsed(buffer)) != (int)bufUsed(buffer), "unable to write");

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Close the connection
***********************************************************************************************************************************/
void
tlsClientClose(TlsClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(TLS_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

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

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Has session been closed by the server?
***********************************************************************************************************************************/
bool
tlsClientEof(const TlsClient *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(TLS_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    FUNCTION_DEBUG_RESULT(BOOL, this->session == NULL);
}

/***********************************************************************************************************************************
Get read interface
***********************************************************************************************************************************/
IoRead *
tlsClientIoRead(const TlsClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_READ, this->read);
}

/***********************************************************************************************************************************
Get write interface
***********************************************************************************************************************************/
IoWrite *
tlsClientIoWrite(const TlsClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->write);
}

/***********************************************************************************************************************************
Free the object
***********************************************************************************************************************************/
void
tlsClientFree(TlsClient *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TLS_CLIENT, this);
    FUNCTION_TEST_END();

    if (this != NULL)
    {
        tlsClientClose(this);
        SSL_CTX_free(this->context);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_TEST_RESULT_VOID();
}
