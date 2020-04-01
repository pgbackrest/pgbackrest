/***********************************************************************************************************************************
Test Tls Client
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/time.h"

#include "common/harnessTls.h"

/***********************************************************************************************************************************
Test server with subject alternate names
***********************************************************************************************************************************/
static void
testTlsServerAltName(void)
{
    if (fork() == 0)
    {
        harnessTlsServerInit(
            harnessTlsTestPort(),
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-alt-name.crt", testRepoPath())),
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".key", testRepoPath())));

        // Certificate error on invalid ca path
        harnessTlsServerAccept();
        harnessTlsServerClose();

        if (testContainer())
        {
            // Success on valid ca file and match common name
            harnessTlsServerAccept();
            harnessTlsServerClose();

            // Success on valid ca file and match alt name
            harnessTlsServerAccept();
            harnessTlsServerClose();

            // Unable to find matching hostname in certificate
            harnessTlsServerAccept();
            harnessTlsServerClose();
        }

        // Certificate error
        harnessTlsServerAccept();
        harnessTlsServerClose();

        // Certificate ignored
        harnessTlsServerAccept();
        harnessTlsServerClose();

        exit(0);
    }
}

/***********************************************************************************************************************************
Test server
***********************************************************************************************************************************/
static void
testTlsServer(void)
{
    if (fork() == 0)
    {
        harnessTlsServerInitDefault();

        // First protocol exchange
        harnessTlsServerAccept();

        harnessTlsServerExpect("some protocol info");
        harnessTlsServerReply("something:0\n");

        sleepMSec(100);
        harnessTlsServerReply("some ");

        sleepMSec(100);
        harnessTlsServerReply("contentAND MORE");

        // This will cause the client to disconnect
        sleepMSec(500);

        // Second protocol exchange
        harnessTlsServerExpect("more protocol info");
        harnessTlsServerReply("0123456789AB");
        harnessTlsServerClose();

        // Need data in read buffer to test tlsWriteContinue()
        harnessTlsServerAccept();
        harnessTlsServerReply("0123456789AB");
        harnessTlsServerClose();

        exit(0);
    }
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("Socket Common"))
    {
        // Save socket settings
        struct SocketLocal socketLocalSave = socketLocal;

        struct addrinfo hints = (struct addrinfo)
        {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
        };

        struct addrinfo *hostAddress;
        int result;
        const char *host = "127.0.0.1";
        const char *port = "7777";

        if ((result = getaddrinfo(host, port, &hints, &hostAddress)) != 0)
            THROW_FMT(HostConnectError, "unable to get address for '%s': [%d] %s", host, result, gai_strerror(result));

        TRY_BEGIN()
        {
            int fd = socket(hostAddress->ai_family, hostAddress->ai_socktype, hostAddress->ai_protocol);
            THROW_ON_SYS_ERROR(fd == -1, HostConnectError, "unable to create socket");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("enable options");

            sckInit(true, 32, 3113, 818);
            sckOptionSet(fd);

            TEST_RESULT_INT(fcntl(fd, F_GETFD), FD_CLOEXEC, "check FD_CLOEXEC");

            socklen_t socketValueSize = sizeof(int);
            int noDelayValue = 0;

            THROW_ON_SYS_ERROR(
                getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &noDelayValue, &socketValueSize) == -1, ProtocolError,
                "unable get TCP_NO_DELAY");

            TEST_RESULT_INT(noDelayValue, 1, "check TCP_NODELAY");

            int keepAliveValue = 0;

            THROW_ON_SYS_ERROR(
                getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepAliveValue, &socketValueSize) == -1, ProtocolError,
                "unable get SO_KEEPALIVE");

            TEST_RESULT_INT(keepAliveValue, 1, "check SO_KEEPALIVE");

            int keepAliveCountValue = 0;

            THROW_ON_SYS_ERROR(
                getsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepAliveCountValue, &socketValueSize) == -1, ProtocolError,
                "unable get TCP_KEEPCNT");

            TEST_RESULT_INT(keepAliveCountValue, 32, "check TCP_KEEPCNT");

            int keepAliveIdleValue = 0;

            THROW_ON_SYS_ERROR(
                getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepAliveIdleValue, &socketValueSize) == -1, ProtocolError,
                "unable get TCP_KEEPIDLE");

            TEST_RESULT_INT(keepAliveIdleValue, 3113, "check TCP_KEEPIDLE");

            int keepAliveIntervalValue = 0;

            THROW_ON_SYS_ERROR(
                getsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveIntervalValue, &socketValueSize) == -1, ProtocolError,
                "unable get TCP_KEEPIDLE");

            TEST_RESULT_INT(keepAliveIntervalValue, 818, "check TCP_KEEPINTVL");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("disable keep-alive");

            sckInit(false, 0, 0, 0);
            sckOptionSet(fd);

            TEST_RESULT_INT(keepAliveValue, 1, "check SO_KEEPALIVE");
            TEST_RESULT_INT(keepAliveCountValue, 32, "check TCP_KEEPCNT");
            TEST_RESULT_INT(keepAliveIdleValue, 3113, "check TCP_KEEPIDLE");
            TEST_RESULT_INT(keepAliveIntervalValue, 818, "check TCP_KEEPINTVL");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("enable keep-alive but disable options");

            sckInit(true, 0, 0, 0);
            sckOptionSet(fd);

            TEST_RESULT_INT(keepAliveValue, 1, "check SO_KEEPALIVE");
            TEST_RESULT_INT(keepAliveCountValue, 32, "check TCP_KEEPCNT");
            TEST_RESULT_INT(keepAliveIdleValue, 3113, "check TCP_KEEPIDLE");
            TEST_RESULT_INT(keepAliveIntervalValue, 818, "check TCP_KEEPINTVL");
        }
        FINALLY()
        {
            // This needs to be freed or valgrind will complain
            freeaddrinfo(hostAddress);
        }
        TRY_END();

        // Restore socket settings
        socketLocal = socketLocalSave;
    }

    // Additional coverage not provided by testing with actual certificates
    // *****************************************************************************************************************************
    if (testBegin("asn1ToStr(), tlsClientHostVerify(), and tlsClientHostVerifyName()"))
    {
        TEST_ERROR(asn1ToStr(NULL), CryptoError, "TLS certificate name entry is missing");

        TEST_ERROR(
            tlsClientHostVerifyName(
                strNew("host"), strNewN("ab\0cd", 5)), CryptoError, "TLS certificate name contains embedded null");

        TEST_ERROR(tlsClientHostVerify(strNew("host"), NULL), CryptoError, "No certificate presented by the TLS server");

        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("host"), strNew("**")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("host"), strNew("*.")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(strNew("a.bogus.host.com"), strNew("*.host.com")), false, "invalid host");
    }

    // Additional coverage not provided by other tests
    // *****************************************************************************************************************************
    if (testBegin("tlsError()"))
    {
        TlsClient *client = NULL;

        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(strNew("99.99.99.99.99"), harnessTlsTestPort(), 0), 0, true, NULL, NULL),
            "new client");

        TEST_RESULT_BOOL(tlsError(client, SSL_ERROR_WANT_READ), true, "continue after want read");
        TEST_RESULT_BOOL(tlsError(client, SSL_ERROR_ZERO_RETURN), false, "check connection closed error");
        TEST_ERROR(tlsError(client, SSL_ERROR_WANT_X509_LOOKUP), ServiceError, "tls error [4]");
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient verification"))
    {
        TlsClient *client = NULL;

        // Connection errors
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(strNew("99.99.99.99.99"), harnessTlsTestPort(), 0), 0, true, NULL, NULL),
            "new client");
        TEST_ERROR(
            tlsClientOpen(client), HostConnectError, "unable to get address for '99.99.99.99.99': [-2] Name or service not known");

        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(strNew("localhost"), harnessTlsTestPort(), 100), 100, true, NULL, NULL),
            "new client");
        TEST_ERROR_FMT(
            tlsClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            harnessTlsTestPort());

        // Certificate location and validation errors
        // -------------------------------------------------------------------------------------------------------------------------
        // Add test hosts
        if (testContainer())
        {
            if (system(                                                                                 // {uncoverable_branch}
                    "echo \"127.0.0.1 test.pgbackrest.org host.test2.pgbackrest.org test3.pgbackrest.org\" |"
                        " sudo tee -a /etc/hosts > /dev/null") != 0)
            {
                THROW(AssertError, "unable to add test hosts to /etc/hosts");                           // {uncovered+}
            }
        }

        // Start server to test various certificate errors
        testTlsServerAltName();

        TEST_ERROR(
            tlsClientOpen(
                tlsClientNew(
                    sckClientNew(strNew("localhost"), harnessTlsTestPort(), 500), 500, true, strNew("bogus.crt"),
                    strNew("/bogus"))),
            CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");
        TEST_ERROR_FMT(
            tlsClientOpen(
                tlsClientNew(sckClientNew(strNew("localhost"), harnessTlsTestPort(), 500), 500, true, NULL, strNew("/bogus"))),
            CryptoError, "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
            harnessTlsTestPort());

        if (testContainer())
        {
            TEST_RESULT_VOID(
                tlsClientOpen(
                    tlsClientNew(
                        sckClientNew(strNew("test.pgbackrest.org"), harnessTlsTestPort(), 500), 500, true,
                        strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                "success on valid ca file and match common name");
            TEST_RESULT_VOID(
                tlsClientOpen(
                    tlsClientNew(
                        sckClientNew(strNew("host.test2.pgbackrest.org"), harnessTlsTestPort(), 500), 500, true,
                        strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                "success on valid ca file and match alt name");
            TEST_ERROR(
                tlsClientOpen(
                    tlsClientNew(
                        sckClientNew(strNew("test3.pgbackrest.org"), harnessTlsTestPort(), 500), 500, true,
                        strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                CryptoError,
                "unable to find hostname 'test3.pgbackrest.org' in certificate common name or subject alternative names");
        }

        TEST_ERROR_FMT(
            tlsClientOpen(
                tlsClientNew(
                    sckClientNew(strNew("localhost"), harnessTlsTestPort(), 500), 500, true,
                    strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".crt", testRepoPath()),
                NULL)),
            CryptoError, "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
            harnessTlsTestPort());

        TEST_RESULT_VOID(
            tlsClientOpen(
                tlsClientNew(sckClientNew(strNew("localhost"), harnessTlsTestPort(), 500), 500, false, NULL, NULL)),
                "success on no verify");
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient general usage"))
    {
        TlsClient *client = NULL;

        // Reset statistics
        sckClientStatLocal = (SocketClientStat){0};
        TEST_RESULT_PTR(sckClientStatStr(), NULL, "no stats yet");
        tlsClientStatLocal = (TlsClientStat){0};
        TEST_RESULT_PTR(tlsClientStatStr(), NULL, "no stats yet");

        testTlsServer();
        ioBufferSizeSet(12);

        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(harnessTlsTestHost(), harnessTlsTestPort(), 500), 500, testContainer(), NULL, NULL),
            "new client");
        TEST_RESULT_VOID(tlsClientOpen(client), "open client");

        const Buffer *input = BUFSTRDEF("some protocol info");
        TEST_RESULT_VOID(ioWrite(tlsClientIoWrite(client), input), "write input");
        ioWriteFlush(tlsClientIoWrite(client));

        TEST_RESULT_STR_Z(ioReadLine(tlsClientIoRead(client)), "something:0", "read line");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        Buffer *output = bufNew(12);
        TEST_RESULT_UINT(ioRead(tlsClientIoRead(client), output), 12, "read output");
        TEST_RESULT_STR_Z(strNewBuf(output), "some content", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(8);
        TEST_RESULT_UINT(ioRead(tlsClientIoRead(client), output), 8, "read output");
        TEST_RESULT_STR_Z(strNewBuf(output), "AND MORE", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(12);
        TEST_ERROR_FMT(
            ioRead(tlsClientIoRead(client), output), FileReadError,
            "timeout after 500ms waiting for read from '%s:%u'", strPtr(harnessTlsTestHost()), harnessTlsTestPort());

        // -------------------------------------------------------------------------------------------------------------------------
        input = BUFSTRDEF("more protocol info");
        TEST_RESULT_VOID(tlsClientOpen(client), "open client again (it is already open)");
        TEST_RESULT_VOID(ioWrite(tlsClientIoWrite(client), input), "write input");
        ioWriteFlush(tlsClientIoWrite(client));

        output = bufNew(12);
        TEST_RESULT_UINT(ioRead(tlsClientIoRead(client), output), 12, "read output");
        TEST_RESULT_STR_Z(strNewBuf(output), "0123456789AB", "    check output");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), false, "    check eof = false");

        output = bufNew(12);
        TEST_RESULT_UINT(ioRead(tlsClientIoRead(client), output), 0, "read no output after eof");
        TEST_RESULT_BOOL(ioReadEof(tlsClientIoRead(client)), true, "    check eof = true");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(tlsClientOpen(client), "open client again (was closed by server)");
        TEST_RESULT_BOOL(tlsWriteContinue(client, -1, SSL_ERROR_WANT_READ, 1), true, "continue on WANT_READ");
        TEST_RESULT_BOOL(tlsWriteContinue(client, 0, SSL_ERROR_NONE, 1), true, "continue on WANT_READ");
        TEST_ERROR(
            tlsWriteContinue(client, 77, 0, 88), FileWriteError,
            "unable to write to tls, write size 77 does not match expected size 88");
        TEST_ERROR(tlsWriteContinue(client, 0, SSL_ERROR_ZERO_RETURN, 1), FileWriteError, "unable to write to tls [6]");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(sckClientStatStr() != NULL, true, "check statistics exist");
        TEST_RESULT_BOOL(tlsClientStatStr() != NULL, true, "check statistics exist");

        TEST_RESULT_VOID(tlsClientFree(client), "free client");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
