/***********************************************************************************************************************************
Test Tls Client
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"

#include "common/harnessFork.h"
#include "common/harnessServer.h"

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

            sckInit(false, true, 32, 3113, 818);
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

            sckInit(false, false, 0, 0, 0);
            sckOptionSet(fd);

            TEST_RESULT_INT(keepAliveValue, 1, "check SO_KEEPALIVE");
            TEST_RESULT_INT(keepAliveCountValue, 32, "check TCP_KEEPCNT");
            TEST_RESULT_INT(keepAliveIdleValue, 3113, "check TCP_KEEPIDLE");
            TEST_RESULT_INT(keepAliveIntervalValue, 818, "check TCP_KEEPINTVL");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("enable keep-alive but disable options");

            sckInit(false, true, 0, 0, 0);
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
            IoSession *session = NULL;
            TEST_ASSIGN(session, sckSessionNew(ioSessionRoleClient, fd, strNewZ(hostBad), 7777, 100), "new socket");

            TEST_ERROR(
                ioWriteReadyP(ioSessionIoWrite(session), .error = true), FileWriteError,
                "timeout after 100ms waiting for write to '172.31.255.255:7777'");

            TEST_RESULT_VOID(ioSessionClose(session), "close socket session");
            TEST_RESULT_VOID(ioSessionClose(session), "close socket session again");
            TEST_RESULT_VOID(ioSessionFree(session), "free socket session");

            // ---------------------------------------------------------------------------------------------------------------------
            TEST_TITLE("unable to connect to blocking socket");

            IoClient *socketClient = sckClientNew(STR(hostLocal), 7777, 0);
            TEST_RESULT_STR_Z(ioClientName(socketClient), "127.0.0.1:7777", " check name");

            socketLocal.block = true;
            TEST_ERROR(
                ioClientOpen(socketClient), HostConnectError, "unable to connect to '127.0.0.1:7777': [111] Connection refused");
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
        IoClient *client = NULL;

        TEST_ASSIGN(client, sckClientNew(STRDEF("localhost"), hrnServerPort(0), 100), "new client");
        TEST_ERROR_FMT(
            ioClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            hrnServerPort(0));

        // This address should not be in use in a test environment -- if it is the test will fail
        TEST_ASSIGN(client, sckClientNew(STRDEF("172.31.255.255"), hrnServerPort(0), 100), "new client");
        TEST_ERROR_FMT(ioClientOpen(client), HostConnectError, "timeout connecting to '172.31.255.255:%u'", hrnServerPort(0));
    }

    // Additional coverage not provided by testing with actual certificates
    // *****************************************************************************************************************************
    if (testBegin("asn1ToStr(), tlsClientHostVerify(), and tlsClientHostVerifyName()"))
    {
        TEST_ERROR(asn1ToStr(NULL), CryptoError, "TLS certificate name entry is missing");

        TEST_ERROR(
            tlsClientHostVerifyName(
                STRDEF("host"), strNewN("ab\0cd", 5)), CryptoError, "TLS certificate name contains embedded null");

        TEST_ERROR(tlsClientHostVerify(STRDEF("host"), NULL), CryptoError, "No certificate presented by the TLS server");

        TEST_RESULT_BOOL(tlsClientHostVerifyName(STRDEF("host"), STRDEF("**")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(STRDEF("host"), STRDEF("*.")), false, "invalid pattern");
        TEST_RESULT_BOOL(tlsClientHostVerifyName(STRDEF("a.bogus.host.com"), STRDEF("*.host.com")), false, "invalid host");
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient verification"))
    {
        IoClient *client = NULL;

        // Connection errors
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(STRDEF("99.99.99.99.99"), 7777, 0), STRDEF("X"), 0, true, NULL, NULL),
            "new client");
        TEST_RESULT_STR_Z(ioClientName(client), "99.99.99.99.99:7777", " check name");
        TEST_ERROR(
            ioClientOpen(client), HostConnectError, "unable to get address for '99.99.99.99.99': [-2] Name or service not known");

        TEST_ASSIGN(
            client, tlsClientNew(sckClientNew(STRDEF("localhost"), hrnServerPort(0), 100), STRDEF("X"), 100, true, NULL, NULL),
            "new client");
        TEST_ERROR_FMT(
            ioClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            hrnServerPort(0));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("bogus client cert/path");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(
                        STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, STRDEF("bogus.crt"), STRDEF("/bogus"))),
            CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");

        // Certificate location and validation errors
        // -------------------------------------------------------------------------------------------------------------------------
        // Add test hosts
#ifdef TEST_CONTAINER_REQUIRED
        HRN_SYSTEM(
            "echo \"127.0.0.1 test.pgbackrest.org host.test2.pgbackrest.org test3.pgbackrest.org\" | sudo tee -a /etc/hosts >"
                " /dev/null");

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "test server", .timeout = 5000)
            {
                // Start server to test various certificate errors
                TEST_RESULT_VOID(
                    hrnServerRunP(
                        HRN_FORK_CHILD_READ(), hrnServerProtocolTls,
                        .certificate = STRDEF(HRN_PATH_REPO "/" HRN_SERVER_CERT_PREFIX "alt-name.crt"),
                        .key = STRDEF(HRN_SERVER_KEY)),
                    "tls alt name server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "test client", .timeout = 1000)
            {
                IoWrite *tls = hrnServerScriptBegin(HRN_FORK_PARENT_WRITE(0));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("certificate error on invalid ca path");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR_FMT(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, NULL,
                            STRDEF("/bogus"))),
                    CryptoError,
                    "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
                    hrnServerPort(0));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("valid ca file and match common name");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_RESULT_VOID(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("test.pgbackrest.org"), hrnServerPort(0), 5000), STRDEF("test.pgbackrest.org"),
                            0, true, STRDEF(HRN_SERVER_CA), NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("valid ca file and match alt name");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_RESULT_VOID(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("host.test2.pgbackrest.org"), hrnServerPort(0), 5000),
                            STRDEF("host.test2.pgbackrest.org"), 0, true, STRDEF(HRN_SERVER_CA), NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("unable to find matching hostname in certificate");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("test3.pgbackrest.org"), hrnServerPort(0), 5000), STRDEF("test3.pgbackrest.org"),
                            0, true, STRDEF(HRN_SERVER_CA), NULL)),
                    CryptoError,
                    "unable to find hostname 'test3.pgbackrest.org' in certificate common name or subject alternative names");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("certificate error");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR_FMT(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true,
                            STRDEF(HRN_SERVER_CERT),
                        NULL)),
                    CryptoError,
                    "unable to verify certificate presented by 'localhost:%u': [20] unable to get local issuer certificate",
                    hrnServerPort(0));

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no certificate verify");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_RESULT_VOID(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, false, NULL, NULL)),
                        "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(tls);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
#endif // TEST_CONTAINER_REQUIRED
    }

    // *****************************************************************************************************************************
    if (testBegin("TlsClient general usage"))
    {
        IoClient *client = NULL;
        IoSession *session = NULL;

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "test server", .timeout = 5000)
            {
                TEST_RESULT_VOID(hrnServerRunP(HRN_FORK_CHILD_READ(), hrnServerProtocolTls), "tls server run");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "test client", .timeout = 1000)
            {
                IoWrite *tls = hrnServerScriptBegin(HRN_FORK_PARENT_WRITE(0));
                ioBufferSizeSet(12);

                TEST_ASSIGN(
                    client,
                    tlsClientNew(
                        sckClientNew(hrnServerHost(), hrnServerPort(0), 5000), hrnServerHost(), 0, TEST_IN_CONTAINER, NULL,
                        NULL),
                    "new client");

                hrnServerScriptAccept(tls);

                TEST_ASSIGN(session, ioClientOpen(client), "open client");
                TlsSession *tlsSession = (TlsSession *)session->pub.driver;

                TEST_RESULT_INT(ioSessionFd(session), -1, "no fd for tls session");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("uncovered errors");

                TEST_RESULT_INT(tlsSessionResultProcess(tlsSession, SSL_ERROR_WANT_WRITE, 0, 0, false), 0, "write ready");
                TEST_ERROR(
                    tlsSessionResultProcess(tlsSession, SSL_ERROR_WANT_X509_LOOKUP, 336031996, 0, false), ServiceError,
                    "TLS error [4:336031996] unknown protocol");
                TEST_ERROR(
                    tlsSessionResultProcess(tlsSession, SSL_ERROR_WANT_X509_LOOKUP, 0, 0, false), ServiceError,
                    "TLS error [4:0] no details available");
                TEST_ERROR(
                    tlsSessionResultProcess(tlsSession, SSL_ERROR_ZERO_RETURN, 0, 0, false), ProtocolError, "unexpected TLS eof");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("first protocol exchange");

                hrnServerScriptExpectZ(tls, "some protocol info");
                hrnServerScriptReplyZ(tls, "something:0\n");

                const Buffer *input = BUFSTRDEF("some protocol info");
                TEST_RESULT_VOID(ioWrite(ioSessionIoWrite(session), input), "write input");
                ioWriteFlush(ioSessionIoWrite(session));

                TEST_RESULT_STR_Z(ioReadLine(ioSessionIoRead(session)), "something:0", "read line");
                TEST_RESULT_BOOL(ioReadEof(ioSessionIoRead(session)), false, "check eof = false");

                hrnServerScriptSleep(tls, 100);
                hrnServerScriptReplyZ(tls, "some ");

                hrnServerScriptSleep(tls, 100);
                hrnServerScriptReplyZ(tls, "contentAND MORE");

                Buffer *output = bufNew(12);
                TEST_RESULT_UINT(ioRead(ioSessionIoRead(session), output), 12, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "some content", "check output");
                TEST_RESULT_BOOL(ioReadEof(ioSessionIoRead(session)), false, "check eof = false");

                output = bufNew(8);
                TEST_RESULT_UINT(ioRead(ioSessionIoRead(session), output), 8, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "AND MORE", "check output");
                TEST_RESULT_BOOL(ioReadEof(ioSessionIoRead(session)), false, "check eof = false");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("read eof");

                hrnServerScriptSleep(tls, 500);

                output = bufNew(12);
                ((IoFdRead *)((SocketSession *)tlsSession->ioSession->pub.driver)->read->pub.driver)->timeout = 100;
                TEST_ERROR_FMT(
                    ioRead(ioSessionIoRead(session), output), FileReadError,
                    "timeout after 100ms waiting for read from '%s:%u'", strZ(hrnServerHost()), hrnServerPort(0));
                ((IoFdRead *)((SocketSession *)tlsSession->ioSession->pub.driver)->read->pub.driver)->timeout = 5000;

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("second protocol exchange");

                hrnServerScriptExpectZ(tls, "more protocol info");
                hrnServerScriptReplyZ(tls, "0123456789AB");

                hrnServerScriptClose(tls);

                input = BUFSTRDEF("more protocol info");
                TEST_RESULT_VOID(ioWrite(ioSessionIoWrite(session), input), "write input");
                ioWriteFlush(ioSessionIoWrite(session));

                output = bufNew(12);
                TEST_RESULT_UINT(ioRead(ioSessionIoRead(session), output), 12, "read output");
                TEST_RESULT_STR_Z(strNewBuf(output), "0123456789AB", "check output");
                TEST_RESULT_BOOL(ioReadEof(ioSessionIoRead(session)), false, "check eof = false");

                output = bufNew(12);
                TEST_RESULT_UINT(ioRead(ioSessionIoRead(session), output), 0, "read no output after eof");
                TEST_RESULT_BOOL(ioReadEof(ioSessionIoRead(session)), true, "check eof = true");

                TEST_RESULT_VOID(ioSessionClose(session), "close again");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("aborted connection before read complete (blocking socket)");

                hrnServerScriptAccept(tls);
                hrnServerScriptReplyZ(tls, "0123456789AB");
                hrnServerScriptAbort(tls);

                socketLocal.block = true;
                TEST_ASSIGN(session, ioClientOpen(client), "open client again (was closed by server)");
                socketLocal.block = false;

                output = bufNew(13);
                TEST_ERROR(ioRead(ioSessionIoRead(session), output), KernelError, "TLS syscall error");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("close connection");

                TEST_RESULT_VOID(ioClientFree(client), "free client");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(tls);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("stastistics exist");

        TEST_RESULT_BOOL(varLstEmpty(kvKeyList(statToKv())), false, "check");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
