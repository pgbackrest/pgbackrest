/***********************************************************************************************************************************
Test Tls Client
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/time.h"

#include "common/harnessFork.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Test server with subject alternate names
***********************************************************************************************************************************/
static void
testTlsServerAltName(void)
{
    FUNCTION_HARNESS_VOID();

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

    FUNCTION_HARNESS_RESULT_VOID();
}

/***********************************************************************************************************************************
Test server
***********************************************************************************************************************************/
static void
testTlsServer(void)
{
    FUNCTION_HARNESS_VOID();

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

    // Test aborted connection before read complete
    harnessTlsServerAccept();
    harnessTlsServerReply("0123456789AB");
    harnessTlsServerAbort();

    FUNCTION_HARNESS_RESULT_VOID();
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

        int result;

        const char *port = "7777";

        const char *hostLocal = "127.0.0.1";
        struct addrinfo *hostLocalAddress;

        if ((result = getaddrinfo(hostLocal, port, &hints, &hostLocalAddress)) != 0)
        {
            THROW_FMT(                                              // {uncoverable - lookup on IP should never fail}
                HostConnectError, "unable to get address for '%s': [%d] %s", hostLocal, result, gai_strerror(result));
        }

        const char *hostBad = "172.31.255.255";
        struct addrinfo *hostBadAddress;

        if ((result = getaddrinfo(hostBad, port, &hints, &hostBadAddress)) != 0)
        {
            THROW_FMT(                                              // {uncoverable - lookup on IP should never fail}
                HostConnectError, "unable to get address for '%s': [%d] %s", hostBad, result, gai_strerror(result));
        }

        TRY_BEGIN()
        {
            int fd = socket(hostBadAddress->ai_family, hostBadAddress->ai_socktype, hostBadAddress->ai_protocol);
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

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("connect to non-blocking socket to test write ready");

            // Attempt connection
            CHECK(connect(fd, hostBadAddress->ai_addr, hostBadAddress->ai_addrlen) == -1);

            // Create socket session and wait for timeout
            SocketSession *session = NULL;
            TEST_ASSIGN(session, sckSessionNew(sckSessionTypeClient, fd, strNew(hostBad), 7777, 100), "new socket");

            TEST_ERROR(
                sckSessionReadyWrite(session), ProtocolError, "timeout after 100ms waiting for write to '172.31.255.255:7777'");

            TEST_RESULT_VOID(sckSessionFree(session), "free socket session");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("unable to connect to blocking socket");

            socketLocal.block = true;
            TEST_ERROR(
                sckClientOpen(sckClientNew(STR(hostLocal), 7777, 0)), HostConnectError,
                "unable to connect to '127.0.0.1:7777': [111] Connection refused");
            socketLocal.block = false;

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("uncovered conditions for sckConnect()");

            TEST_RESULT_BOOL(sckConnectInProgress(EINTR), true, "connection in progress (EINTR)");
        }
        FINALLY()
        {
            // These need to be freed or valgrind will complain
            freeaddrinfo(hostLocalAddress);
            freeaddrinfo(hostBadAddress);
        }
        TRY_END();

        // Restore socket settings
        socketLocal = socketLocalSave;
    }

    // *****************************************************************************************************************************
    if (testBegin("SocketClient"))
    {
        SocketClient *client = NULL;

        TEST_ASSIGN(client, sckClientNew(strNew("localhost"), harnessTlsTestPort(), 100), "new client");
        TEST_ERROR_FMT(
            sckClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            harnessTlsTestPort());

        // This address should not be in use in a test environment -- if it is the test will fail
        TEST_ASSIGN(client, sckClientNew(strNew("172.31.255.255"), harnessTlsTestPort(), 100), "new client");
        TEST_ERROR_FMT(sckClientOpen(client), HostConnectError, "timeout connecting to '172.31.255.255:%u'", harnessTlsTestPort());
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

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                // Start server to test various certificate errors
                TEST_RESULT_VOID(testTlsServerAltName(), "tls alt name server begin");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                TEST_ERROR(
                    tlsClientOpen(
                        tlsClientNew(
                            sckClientNew(strNew("localhost"), harnessTlsTestPort(), 5000), 0, true, strNew("bogus.crt"),
                            strNew("/bogus"))),
                    CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");
                TEST_ERROR_FMT(
                    tlsClientOpen(
                        tlsClientNew(
                            sckClientNew(strNew("localhost"), harnessTlsTestPort(), 5000), 0, true, NULL, strNew("/bogus"))),
                    CryptoError,
                    "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
                    harnessTlsTestPort());

                if (testContainer())
                {
                    TEST_RESULT_VOID(
                        tlsClientOpen(
                            tlsClientNew(
                                sckClientNew(strNew("test.pgbackrest.org"), harnessTlsTestPort(), 5000), 0, true,
                                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                        "success on valid ca file and match common name");
                    TEST_RESULT_VOID(
                        tlsClientOpen(
                            tlsClientNew(
                                sckClientNew(strNew("host.test2.pgbackrest.org"), harnessTlsTestPort(), 5000), 0, true,
                                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                        "success on valid ca file and match alt name");
                    TEST_ERROR(
                        tlsClientOpen(
                            tlsClientNew(
                                sckClientNew(strNew("test3.pgbackrest.org"), harnessTlsTestPort(), 5000), 0, true,
                                strNewFmt("%s/" TEST_CERTIFICATE_PREFIX "-ca.crt", testRepoPath()), NULL)),
                        CryptoError,
                        "unable to find hostname 'test3.pgbackrest.org' in certificate common name or subject alternative names");
                }

                TEST_ERROR_FMT(
                    tlsClientOpen(
                        tlsClientNew(
                            sckClientNew(strNew("localhost"), harnessTlsTestPort(), 5000), 0, true,
                            strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".crt", testRepoPath()),
                        NULL)),
                    CryptoError,
                    "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
                    harnessTlsTestPort());

                TEST_RESULT_VOID(
                    tlsClientOpen(
                        tlsClientNew(sckClientNew(strNew("localhost"), harnessTlsTestPort(), 5000), 0, false, NULL, NULL)),
                        "success on no verify");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient general usage"))
    {
        TlsClient *client = NULL;
        TlsSession *session = NULL;

        // Reset statistics
        sckClientStatLocal = (SocketClientStat){0};
        TEST_RESULT_PTR(sckClientStatStr(), NULL, "no stats yet");
        tlsClientStatLocal = (TlsClientStat){0};
        TEST_RESULT_PTR(tlsClientStatStr(), NULL, "no stats yet");

        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                TEST_RESULT_VOID(testTlsServer(), "tls server begin");
            }
            HARNESS_FORK_CHILD_END();

            HARNESS_FORK_PARENT_BEGIN()
            {
                ioBufferSizeSet(12);

                TEST_ASSIGN(
                    client,
                    tlsClientNew(sckClientNew(harnessTlsTestHost(), harnessTlsTestPort(), 5000), 0, testContainer(), NULL, NULL),
                    "new client");
                TEST_ASSIGN(session, tlsClientOpen(client), "open client");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("socket read/write ready");

                TEST_RESULT_BOOL(sckReadyRetry(-1, EINTR, true, waitNew(1000)), true, "retry after interrupt");
                TEST_RESULT_BOOL(sckReadyRetry(-1, EINTR, false, waitNew(100)), true, "retry before timeout");
                TEST_RESULT_BOOL(sckReadyRetry(-1, EINTR, false, waitNew(0)), false, "no retry after timeout");
                TEST_ERROR(
                    sckReadyRetry(-1, EINVAL, true, waitNew(1000)), KernelError, "unable to poll socket: [22] Invalid argument");

                TEST_RESULT_BOOL(sckReadyRead(session->socketSession->fd, 0), false, "socket is not read ready");
                TEST_RESULT_BOOL(sckReadyWrite(session->socketSession->fd, 100), true, "socket is write ready");
                TEST_RESULT_VOID(sckSessionReadyWrite(session->socketSession), "socket session is write ready");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("uncovered errors");

                TEST_RESULT_INT(tlsSessionResultProcess(session, SSL_ERROR_WANT_WRITE, 0, false), 0, "write ready");
                TEST_ERROR(tlsSessionResultProcess(session, SSL_ERROR_WANT_X509_LOOKUP, 0, false), ServiceError, "tls error [4]");
                TEST_ERROR(tlsSessionResultProcess(session, SSL_ERROR_ZERO_RETURN, 0, false), ProtocolError, "unexpected eof");

                // -----------------------------------------------------------------------------------------------------------------
                const Buffer *input = BUFSTRDEF("some protocol info");
                TEST_RESULT_VOID(ioWrite(tlsSessionIoWrite(session), input), "write input");
                ioWriteFlush(tlsSessionIoWrite(session));

                TEST_RESULT_STR_Z(ioReadLine(tlsSessionIoRead(session)), "something:0", "read line");
                TEST_RESULT_BOOL(ioReadEof(tlsSessionIoRead(session)), false, "    check eof = false");

                Buffer *output = bufNew(12);
                TEST_RESULT_UINT(ioRead(tlsSessionIoRead(session), output), 12, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "some content", "    check output");
                TEST_RESULT_BOOL(ioReadEof(tlsSessionIoRead(session)), false, "    check eof = false");

                output = bufNew(8);
                TEST_RESULT_UINT(ioRead(tlsSessionIoRead(session), output), 8, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "AND MORE", "    check output");
                TEST_RESULT_BOOL(ioReadEof(tlsSessionIoRead(session)), false, "    check eof = false");

                output = bufNew(12);
                session->socketSession->timeout = 100;
                TEST_ERROR_FMT(
                    ioRead(tlsSessionIoRead(session), output), ProtocolError,
                    "timeout after 100ms waiting for read from '%s:%u'", strPtr(harnessTlsTestHost()), harnessTlsTestPort());
                session->socketSession->timeout = 5000;

                // -----------------------------------------------------------------------------------------------------------------
                input = BUFSTRDEF("more protocol info");
                TEST_RESULT_VOID(ioWrite(tlsSessionIoWrite(session), input), "write input");
                ioWriteFlush(tlsSessionIoWrite(session));

                output = bufNew(12);
                TEST_RESULT_UINT(ioRead(tlsSessionIoRead(session), output), 12, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "0123456789AB", "    check output");
                TEST_RESULT_BOOL(ioReadEof(tlsSessionIoRead(session)), false, "    check eof = false");

                output = bufNew(12);
                TEST_RESULT_UINT(ioRead(tlsSessionIoRead(session), output), 0, "read no output after eof");
                TEST_RESULT_BOOL(ioReadEof(tlsSessionIoRead(session)), true, "    check eof = true");

                TEST_RESULT_VOID(tlsSessionClose(session, false), "close again");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("aborted connection before read complete (blocking socket)");

                socketLocal.block = true;
                TEST_ASSIGN(session, tlsClientOpen(client), "open client again (was closed by server)");
                socketLocal.block = false;

                output = bufNew(13);
                TEST_ERROR(ioRead(tlsSessionIoRead(session), output), KernelError, "tls failed syscall");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_RESULT_BOOL(sckClientStatStr() != NULL, true, "check statistics exist");
                TEST_RESULT_BOOL(tlsClientStatStr() != NULL, true, "check statistics exist");

                TEST_RESULT_VOID(tlsClientFree(client), "free client");
            }
            HARNESS_FORK_PARENT_END();
        }
        HARNESS_FORK_END();
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
