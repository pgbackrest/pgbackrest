/***********************************************************************************************************************************
Test Tls Client
***********************************************************************************************************************************/
#include <fcntl.h>
#include <unistd.h>

#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "storage/posix/storage.h"

#include "common/harnessFork.h"
#include "common/harnessServer.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Client key/cert signed by another CA used to generate invalid client cert error

Run the following in a temp path:

openssl genrsa -out bogus-ca.key 4096
openssl req -new -x509 -sha256 -days 99999 -key bogus-ca.key -out bogus-ca.crt -subj "/CN=bogus"
openssl req -nodes -new -newkey rsa:4096 -sha256 -keyout bogus-client.key -out bogus-client.csr -subj "/CN=bogus"
openssl x509 -extensions usr_cert -req -days 99999 -CA bogus-ca.crt -CAkey bogus-ca.key -CAcreateserial -in bogus-client.csr \
    -out bogus-client.crt

Then copy bogus-client.crt/bogus-client.key into the variables below. Use variables instead of defines so we know when the variable
is no longer used.
***********************************************************************************************************************************/
static const char *const testClientBogusCert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEqTCCApECFB9rIW8PSz2iH9LoV4DuNQf+e5s3MA0GCSqGSIb3DQEBCwUAMBAx\n"
    "DjAMBgNVBAMMBWJvZ3VzMCAXDTIxMDgyNTE5MzgyNVoYDzIyOTUwNjA5MTkzODI1\n"
    "WjAQMQ4wDAYDVQQDDAVib2d1czCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoC\n"
    "ggIBANZuk8q8jOzDir2EFzvMCIECHDkL2DF+qXbcooPua+K6LKUJeskFIFiGVQBA\n"
    "X5QgshIcKl7cUcM11KYylt62Nbkzj0MbupWhynVCM2P8ZAxy3zcqLyWy9D0DMoMe\n"
    "YPJrx7MxhOkymUb3ogPmBzlhcVsZayh+b8ywM7q9NAi5EWCqIbeFXB9jJu2qpZXY\n"
    "l8bHnBIbqmyh45l+ADY5gTW/qx4SU1QnEgctqmrMTN7bdCP9Jch/gCyqB17RTwL2\n"
    "L1SpeIlDG5FUdjOfOUJf6GnX2kQh7niWYqsa9C/ByY0+YrjmIrHSDK51AaIGovnw\n"
    "dn9GZvpwY6wWgBoyt/17VcEqPHl/Bey6K37QGgd2o8Ez3aBJp7sgnxszSn5Zddu5\n"
    "XBBrdCNlhhF/qdqezevVDWQpr+fq0eebK7VxR4OMI2jwwc5yUd+ZdJoxNQevV1pC\n"
    "JXRFbwZ2Um5ZBw1CoZxRep1cZDy1zvZhhn6Hen7aiqoFkD61JjfgCXyeWfdi6Ytg\n"
    "e3cnJpo5I8PpHHjRABmWWyu/vF9M25QUAT+lVCIJ+/yvwRvrFnN3VHF/JN7uuu0R\n"
    "hEf4rXBWpTc1z8JzUzFGyMmeLQFXCGlHWY0IBbsA952bk1kgMwKgdBa0RbJJrQpx\n"
    "NMQqX1tfsDHJw86+uO8NTd/ekfs7j/HiHBDZF8g1su8J9Em3AgMBAAEwDQYJKoZI\n"
    "hvcNAQELBQADggIBAL1/+/7zFcWhdOPtzvghuRMvcxUAKvWUNaELpkAmtV6cB9CP\n"
    "9AlJwhCAuvjdRofdfigy94J9MXkvULMqHsSE56Ei6la+mXawenN2OyuwChvagFOl\n"
    "tm+wEgutJ8c32g9QMxvC9OEZlXEgyWyo2rqV0KQ9GKEr2kBhvCOitEJhfQVMwCAR\n"
    "mQyeyMRdFxfvI7IG/3AWrzNJlr8+w1WtWmkefcy5TG7HzLwIXmFde9W7HkOJnch4\n"
    "m+5Cvw+KNgivWXp4vhr7uDRAr/feJ+GeoermQb/apM7PlJvHP9u8+H9OUu4WrB6v\n"
    "4Y8z/g6qV/br60wxNnsM1ESm+k2LkiSKe6Hdpl9FNV9NwtTnEsvUC5wZNAbHwsch\n"
    "BhFw354/ut/zLwSXaL70PD8jsOxClnJv3KEAc8+eL7bdgfPS9lJKY3Xy87XatnoX\n"
    "DUbokGaJEOaheRFpEzbtin5b2RBqCtqsnPv6ftdO0kvL/GKoATeg0khwr///Ur2n\n"
    "LmgFQGlIz5T5PzBXsQNjVzWlBSnoOQp6M4pRcC+R0cbInyG2YTDhYydpJSwAxc2L\n"
    "EofZUlo7XE6Nzn/RZeaRDZ7Y4+5EIe42i/5A5ydXuPzKFc9MoNSl5ddNGWt/OjDJ\n"
    "A+CAKwGi30m0Lv9yu4fb2yi+Al3+qmkCtIwa0XSMJj8SluReKmKGVVdh32Qb\n"
    "-----END CERTIFICATE-----";

static const char *const testClientBogusKey =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIJQwIBADANBgkqhkiG9w0BAQEFAASCCS0wggkpAgEAAoICAQDWbpPKvIzsw4q9\n"
    "hBc7zAiBAhw5C9gxfql23KKD7mviuiylCXrJBSBYhlUAQF+UILISHCpe3FHDNdSm\n"
    "MpbetjW5M49DG7qVocp1QjNj/GQMct83Ki8lsvQ9AzKDHmDya8ezMYTpMplG96ID\n"
    "5gc5YXFbGWsofm/MsDO6vTQIuRFgqiG3hVwfYybtqqWV2JfGx5wSG6psoeOZfgA2\n"
    "OYE1v6seElNUJxIHLapqzEze23Qj/SXIf4Asqgde0U8C9i9UqXiJQxuRVHYznzlC\n"
    "X+hp19pEIe54lmKrGvQvwcmNPmK45iKx0gyudQGiBqL58HZ/Rmb6cGOsFoAaMrf9\n"
    "e1XBKjx5fwXsuit+0BoHdqPBM92gSae7IJ8bM0p+WXXbuVwQa3QjZYYRf6nans3r\n"
    "1Q1kKa/n6tHnmyu1cUeDjCNo8MHOclHfmXSaMTUHr1daQiV0RW8GdlJuWQcNQqGc\n"
    "UXqdXGQ8tc72YYZ+h3p+2oqqBZA+tSY34Al8nln3YumLYHt3JyaaOSPD6Rx40QAZ\n"
    "llsrv7xfTNuUFAE/pVQiCfv8r8Eb6xZzd1RxfyTe7rrtEYRH+K1wVqU3Nc/Cc1Mx\n"
    "RsjJni0BVwhpR1mNCAW7APedm5NZIDMCoHQWtEWySa0KcTTEKl9bX7AxycPOvrjv\n"
    "DU3f3pH7O4/x4hwQ2RfINbLvCfRJtwIDAQABAoICABAUBnzjGmX+W37OUrenGtQh\n"
    "hmA4pSNA7g/9hyoBTJGZiBNv3IcKHVzF5cW5DfGbaf61oe+u8WqDtMgpbuqQGwMh\n"
    "/JH5mEnz8axNJHFQ0WeljVsjjJl1C58viDAQrRBASJ8FDYQ2yQtrMfi83LnOtqMw\n"
    "CrrkkBl29MoBuc8VoVnwJ8sM8tVfp+GWNAhCT08WVHt/G449rUUrD3UBZtDS6E++\n"
    "7ASZUV68a9TKMNFc/x5bsuOPu9qdfSP86cG9F9tvQZx5La39+Ubxn2d8rX6SMsl9\n"
    "CdZ84DUYNksGashubxSSHPPcXhsOpuqxOLMo9pmge8Q3fSHAJibQur8E6m8rbZFD\n"
    "WWZ7F/v9ZNKpHZ2AY1QX81dgvvegPA9r2Nk3ulk1kja5ggHJHFAyK7pxAxpCsCle\n"
    "fcgNlZgokPTZatzw8xgbqbtfv3YQeCHnqpe9ccM5Az19DFzFlCjU+oYEwA1ZhrfE\n"
    "tvCgv/KsZCLi9KjmV+x5Fpzq2BXN7IkEZTtNZgsc5AGyDS59bZ4zFMq0xPLjGRoJ\n"
    "85jmcTyJ9F34AatU+5B19wBnSCOQ/6Yfy5fgrkgezhRAfvxmlPnbfvPqZxUD5AjS\n"
    "TaEGmrLLZkQ073E9y+uGW8stUbMm6aGwJadgC6La0rTLBYB8pCEyxafMfMNPkw3D\n"
    "g2ZUF64XsIe1AC8qjrNhAoIBAQD9Gci+df0/A8SAIRclbB0vsvfP5t03lXIYZhHB\n"
    "SpyhzRL45LTWV2zlpbXqV0nbr6W+n6l9XnGb3ZN4b87Ea/dZnKenyz76uUIOqrzZ\n"
    "DSpyS5BUVEzkr6ZiYaKdknMzQlHXysxwHTkLVXr6vUM+Sl7fBzaa5ABozRbyMDX/\n"
    "eelqYQdo1K4BNQHx9sKysCMFCWVRB1+IE5H2h36OWZNr+vWUQ58WFIhnKp3sHKCj\n"
    "yX6s1lbjlw6pv7XYg5v1kjgH4AinCnQIw/xKNqZJANpSUeYu0v3OqgboseH88dyR\n"
    "L8CJW3B4iXAQ7Hou8sCI4KyIVCDUgTE2lw+2l1+cjQEIk+XpAoIBAQDY42Wz5Xtz\n"
    "qZYchrIUPCQC9Yb7yP9nxVHb+zUnFNeA9OFYEP0PHM5IvNxgk4yCmJWYtFmn+poY\n"
    "qfE0HswXSWLiA+8JX7PHnOK1LJf9mDOXaY5/q06tSTXYrW3qL/Kl83wJUsJJvHJf\n"
    "wxy4IPiYV/YUrxozRwBTBNtj/+Mmy/jnA/Q69LkJ+CeLbDqSao6dG/FilM/lHBjV\n"
    "nabm3fB43YSNqBd2/18VhJAETNZdXPjAUQ2lUMtsAQXOwrGw6enkLoKexuAU0Ux9\n"
    "kkDVXUdWUse9oIm17KEB0hbZbL9Xya6Vk7kCVIXN2xbv5iOR1oycqA8yy/lF095D\n"
    "ZNRAzIYw/86fAoIBAQCoihf0RGusH50lWWOpZtIUpk+A4RIkZl8Awk9GcKHW2NGu\n"
    "bdXB+ZupXOzDrPag1NlBE97wfgiXKzh9da6xe9fNk5TNFnnMybqkO6vfuXWvgIQO\n"
    "s8g0bIcWcj+wQAp4csw/L2ttqPgIhRaMi6WQgEOmro39HKDtKM0D33jFs+/sB8rA\n"
    "UwfABAVUk+ZYyRO40eXmzEsgOS/0g4uRzTJvMEGCRnlUYb3nPSjGRtXt20qAW4am\n"
    "rTt1bBTypckgAQtQqy331e0ovSFuZe/bIzc+pAzs11Ft4ikRoQqEvqYLBEpo7Tv6\n"
    "+EJo8p/2TW5Kd5pMegEWoSUdXgB3rVtcy0SJ6rqpAoIBAGNuVKjVkvQikhP/2FIY\n"
    "hDXrE/gIXLbZKj8cenCxSF7xZQG3wBwWi6ejFbEc07TneOWqANRWuiCGgHLxj4U5\n"
    "eqC9Ru/YNRZVIUYH7KIxDa3jkZWMFqSwxIPSdmp/ktFrv7iSfUnKn/CxBVCQpQdK\n"
    "hCFVaUCK02Y7+sxselnF9xUJpgUFPnOIlbCAbJXFTh5OuioEqQ6TA/uiq+p5Yw42\n"
    "F9fNcPx39MJrpI6kHz5sKgoY3pWkZa3dBimU7lt50WVvwShDamWA0n1a+GgYvGSh\n"
    "zLpth9SkZ+fqxdjl1w7LAkPGlnGwCCuovmo66qGoZ4xGK7mQ83WEvQfOiNQwL3D1\n"
    "RWcCggEBAKIQz5FoDbt0FeoBpxus8SAjOL1oWbs4i8GpOklBGg65Q9DY+hialA4p\n"
    "YzvU1Gc2CO4Ly+6gEn+EOU0U5G9X9fA/jg7OWjRe1IxETNT2h7ZNFHHGS8Hw8ffD\n"
    "a5lER+FoxWnkr+Ha52SqviEncVy+1QIo2CZf3cnMp7turrLCSWi16jObcXuSxPbR\n"
    "imcderkJABQ5nlXFkQqesluYSjVNFBVFEEnrbDMq3hxCj9O681aDyLMyVId5hTSh\n"
    "0sov2zy5gXVCUbhtdc+DjorqUXha4gs0uM+4xeaer7DYyt5Fgc9aAcmBguwu3FGp\n"
    "fSPKoHpKuloAXgj43pWZtJ3WrJUJcpk=\n"
    "-----END PRIVATE KEY-----\n";

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

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
            client, tlsClientNew(sckClientNew(STRDEF("99.99.99.99.99"), 7777, 0), STRDEF("X"), 0, true, NULL, NULL, NULL, NULL),
            "new client");
        TEST_RESULT_STR_Z(ioClientName(client), "99.99.99.99.99:7777", " check name");
        TEST_ERROR(
            ioClientOpen(client), HostConnectError, "unable to get address for '99.99.99.99.99': [-2] Name or service not known");

        TEST_ASSIGN(
            client,
            tlsClientNew(sckClientNew(STRDEF("localhost"), hrnServerPort(0), 100), STRDEF("X"), 100, true, NULL, NULL, NULL, NULL),
            "new client");
        TEST_ERROR_FMT(
            ioClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            hrnServerPort(0));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing ca cert/path");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, STRDEF("bogus.crt"),
                    STRDEF("/bogus"), NULL, NULL)),
            CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing client cert");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, NULL, NULL, STRDEF("/bogus"),
                    STRDEF("/bogus"))),
            CryptoError, "unable to load cert '/bogus': [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing client key");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF("/bogus"))),
            CryptoError, "unable to load key '/bogus': [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("client cert and key do not match");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(HRN_SERVER_KEY))),
            CryptoError,
            "unable to load key '" HRN_PATH_REPO "/test/certificate/pgbackrest-test-server.key': [185073780] key values mismatch");

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
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, true, NULL, STRDEF("/bogus"),
                            NULL, NULL)),
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
                            0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("valid ca file and match alt name");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_RESULT_VOID(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("host.test2.pgbackrest.org"), hrnServerPort(0), 5000),
                            STRDEF("host.test2.pgbackrest.org"), 0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("unable to find matching hostname in certificate");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("test3.pgbackrest.org"), hrnServerPort(0), 5000), STRDEF("test3.pgbackrest.org"),
                            0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL)),
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
                            STRDEF(HRN_SERVER_CERT), NULL, NULL, NULL)),
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
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000), STRDEF("X"), 0, false, NULL, NULL, NULL,
                            NULL)),
                        "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(tls);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("client cert is invalid (signed by another CA)");

        // Socket server to accept connections
        IoServer *socketServer = sckServerNew(STRDEF("localhost"), hrnServerPort(0), 5000);

        // Add host name
        HRN_SYSTEM_FMT("echo \"127.0.0.1 %s\" | sudo tee -a /etc/hosts > /dev/null", strZ(hrnServerHost()));

        // Put bogus cert and key
        storagePutP(storageNewWriteP(storageTest, STRDEF("bogus-client.crt")), BUFSTRZ(testClientBogusCert));
        storagePutP(storageNewWriteP(storageTest, STRDEF("bogus-client.key")), BUFSTRZ(testClientBogusKey));

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "test server", .timeout = 5000)
            {
                IoServer *tlsServer = tlsServerNew(
                    STRDEF("localhost"), STRDEF(HRN_SERVER_CA), STRDEF(HRN_SERVER_KEY), STRDEF(HRN_SERVER_CERT), NULL, 5000);
                IoSession *socketSession = ioServerAccept(socketServer, NULL);

                TEST_ERROR(
                    ioServerAccept(tlsServer, socketSession), ServiceError, "TLS error [1:337100934] certificate verify failed");
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "test client")
            {
                IoSession *clientSession = NULL;

                TEST_ASSIGN(
                    clientSession,
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(hrnServerHost(), hrnServerPort(0), 5000), hrnServerHost(), 5000, true, NULL, NULL,
                            STRDEF(TEST_PATH "/bogus-client.crt"), STRDEF(TEST_PATH "/bogus-client.key"))),
                    "client open");

                TEST_ERROR(
                    ioRead(ioSessionIoRead(clientSession), bufNew(1)), ServiceError,
                    "TLS error [1:336151576] tlsv1 alert unknown ca");
                TEST_RESULT_VOID(ioSessionFree(clientSession), "free client session");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        ioServerFree(socketServer);
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
                        NULL, NULL, NULL),
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
