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
Server cert with only a common name to test absence of alt names

To regenerate, run the following in a temp path:

openssl req -nodes -new -newkey rsa:4096 -sha256 -key ~/pgbackrest/test/certificate/pgbackrest-test-server.key \
    -out server-cn-only.csr -subj "/CN=127.0.0.1"
openssl x509 -extensions usr_cert -req -days 99999 -CA ~/pgbackrest/test/certificate/pgbackrest-test-ca.crt \
    -CAkey ~/pgbackrest/test/certificate/pgbackrest-test-ca.key -CAcreateserial -in server-cn-only.csr -out server-cn-only.crt

Then copy server-cn-only.crt into the variable below. Use a variable instead of a define so we know when the variable is not used.
***********************************************************************************************************************************/
static const char *const testServerCnOnlyCert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIE+TCCAuECFFtID1qGQ+Q6oyFiD9z5YmANCADjMA0GCSqGSIb3DQEBCwUAMFwx\n"
    "CzAJBgNVBAYTAlVTMQwwCgYDVQQIDANBbGwxDDAKBgNVBAcMA0FsbDETMBEGA1UE\n"
    "CgwKcGdCYWNrUmVzdDEcMBoGA1UEAwwTdGVzdC5wZ2JhY2tyZXN0Lm9yZzAgFw0y\n"
    "MTA4MjYxMjIxNTNaGA8yMjk1MDYxMDEyMjE1M1owFDESMBAGA1UEAwwJMTI3LjAu\n"
    "MC4xMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAwzNZDX/VhTA6lALX\n"
    "DZ4AOHv4OQH5wTZipa97XdTrI2TIfMGEffLmv5wzN85pku5HXBuHGJUaUENXt1Ss\n"
    "GwdfBx/gZZEA8oONqkrxOoJTrABWIAs5k6TTUd+f3Y39rlsyQj076f1sw6Mw9qoC\n"
    "h+JKXDDqw8kGwQHifXdtCrxL9OfV4eq+gYKrqdlyFM08WfKxe0Js8bB5cZ4Bt/GC\n"
    "2JhQzQ9bMjYJlxSXIXivP/FFunVT5hZ8gsUVAH+/sm8xlQ4sedW7mIBKkjT3tgL0\n"
    "FvchB3XyoZ6Sr0JKVaMOcQjIsTzOqdgawgArO541ZwUWHdJH+DODr/gBWXSnnzhH\n"
    "ED5DAvRMPdO/t353qS/ihpacTqQ91B4UKxK1pVNC84ch3spCLnQncl7kn7RhcdCc\n"
    "b5g4ZfahRmq79QSoMDvN4+7MtyERLXtSttSWiBzQVVj/jcFNDeGeDjKp6Z55xoso\n"
    "tMZ3yVajl4IbuQS1pfTLjp7WdJ58y5hQ+8O/ebjUYIxOo5kZhRZV/jxqoR7Ga9MG\n"
    "bAQ7BPcTuItpfGqiWcdYU+ZdyyFwvpXov6qNoCYt58nj7s+FAbed7EzRHa2Z3RVG\n"
    "kcqv2iX5EddydHmqKip+QUUR4cPLUXn+kvOHtJEOgAWDURh0DVfhrMD5dX1d+9de\n"
    "BUwZ89gYvxkkErPL1o8OPRxyiucCAwEAATANBgkqhkiG9w0BAQsFAAOCAgEAlwMZ\n"
    "tlqvggfXsJh/AQdl1XxqQKzwC+1OyPozqTUMaEiHLgswJw8eXaZB1/8g9ZODPO3N\n"
    "tLh6JfE4gJJ6gs89YmaZLR0oH3RkoFXSi4+t+WdyF0t3QrBuVx4uO3BeEdD1aLXm\n"
    "lxS7004mJAEMn9FTBBMwek/DGS1Ic/tHwFCRvvE73mFcPL2Qs03ZzRuYUEI1Ckef\n"
    "ONFu6/pydIS5MK0QCP/MfUlKP1D3u2aFEbdNHy4GjzGpfg+1DD/ebSswQG1YpjnN\n"
    "5XLEQZ9IKE2ULq9GnnhqPNUTdX6HFHxVvyZUe/iXasOCX7C9PBipj3tulLcPMbLn\n"
    "4tToEuLkvsLU2Z6I9mcS88Z30VyYu4BzM6tim7XvsOEObILjs2Qa0dJ2hSF6QJ9L\n"
    "NUrbWS591v/PvUdk68kC8UL7o7UVS3lsZoRIZD+X+xdEi+zy4DNIMKUBnWtKamUU\n"
    "1VOosL6vDSZYGg0InGfaBm3Bz3elTrWUHCapNQ5Zsxk+Sq+IQ4hauczrOsd6jugR\n"
    "m1JzWMUZROfSrcVfighZSencJwJEmyCQwnMUyovPs2v7S+1QQEY210ZZd5Fphoye\n"
    "1oA2FndLfr8BOG88+TzwdFilOiZ28lIpMFas38uybJBwlxVYN4/aLyIQGp6AyzGR\n"
    "XqmU0pBFqRYS8xENKxk7lPnxFKyEpb3NK3wk3mo=\n"
    "-----END CERTIFICATE-----";

/***********************************************************************************************************************************
Client key with a password

To regenerate, run the following in a temp path:

openssl genrsa -aes256 -passout pass:xxxx -out client-pwd.key 1024

Then copy server-pwd.key into the variable below. Use a variable instead of a define so we know when the variable is not used.
***********************************************************************************************************************************/
static const char *const testClientPwdKey =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "Proc-Type: 4,ENCRYPTED\n"
    "DEK-Info: AES-256-CBC,86BD081993E54559E92BFF24921DCD00\n"
    "\n"
    "MglGBz4qfnUUs7IuueInkDrn5GRp6V7ZOs/S8rkrOp0i7MTfFJk1cYByu0FCcLqo\n"
    "O3UX1dLPOzu74hJGOmOUHEJzrzA09wFIshTZo96z60+gQGcPEWkkkG9eRadJG5EN\n"
    "TiAq1xopmAYK+srSBsPD7Xf9KeYpFM4Fa98pWbEhzMBiozgh2fH8+6OK8izDFJ57\n"
    "aW8p/9lwZtkO2nd6G4meU+vyVDsL4GpdHcryi+MGGXfhkrr6mFAQ3PLpHTwr6xNI\n"
    "wp6IjuLpNwlkNadq8Wgi8qy4YhpdqmSVt/oFJ25HMH+0UT+EAk0f/WMqOgkSmi3y\n"
    "HgmG7YYAHel5tVeY59/ovxMvc290KQkthVgYBIT/Sy3O4pTRgu+xBkB5VfxjI9gj\n"
    "yVVHsvJHwWdNyUf093Qvroul6Ulob7DOXmPvRWuu6YIBASYIGrmtI2cZ0SQBoySp\n"
    "V65yTAwoi7bqsmwCo4sEjKE6FSeVINY/EvwYLbfyGUOmaunWkWkNPsg9fvwTZNOc\n"
    "3G1IAypM4++02wVVeLdc0+n9FdE6QX3MWpUeZ5YaOzjBjCisk+jJ46L79bi6Z/Xc\n"
    "H1XXBsMnmFhkhd3lraMQ8QYpWus890OmCnimB59SM1W2LgROwv/fXt/8rwgJY1v5\n"
    "6VP5KAZkXpCq20gG6C7GW3jL5/prnPoe+uXku4m4iAReUmTewqht8WUyQaiAWy+e\n"
    "nH5HoTaB3+9ZLu2RivU9l6y1YwYSAbSWBPbnN4HmjP4rmLG5t1ky9igYfJ3NL3LV\n"
    "gJPetwzWuiONyshwMzcg0bE/NjzTXcCaFVKSJ/M++Kd+abDcixUQ3u6htVFHW/L/\n"
    "-----END RSA PRIVATE KEY-----";

/***********************************************************************************************************************************
Client cert signed by another CA used to generate invalid client cert error

To regenerate, run the following in a temp path:

openssl genrsa -out bogus-ca.key 4096
openssl req -new -x509 -sha256 -days 99999 -key bogus-ca.key -out bogus-ca.crt -subj "/CN=bogus"
openssl req -nodes -new -newkey rsa:4096 -sha256 -key ~/pgbackrest/test/certificate/pgbackrest-test-client.key \
    -out client-bad-ca.csr -subj "/CN=bogus"
openssl x509 -extensions usr_cert -req -days 99999 -CA bogus-ca.crt -CAkey bogus-ca.key -CAcreateserial -in client-bad-ca.csr \
    -out client-bad-ca.crt

Then copy client-bad-ca.crt into the variable below. Use a variable instead of a define so we know when the variable is not used.
***********************************************************************************************************************************/
static const char *const testClientBadCa =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEqTCCApECFAzHjCL/QJZZRmBhloX298J4V4HbMA0GCSqGSIb3DQEBCwUAMBAx\n"
    "DjAMBgNVBAMMBWJvZ3VzMCAXDTIxMDgyNjE0MzYyMloYDzIyOTUwNjEwMTQzNjIy\n"
    "WjAQMQ4wDAYDVQQDDAVib2d1czCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoC\n"
    "ggIBALK5ahiXzFqvha28yqe3SGdezs4IREHUDgbn6Tem9k1GazE/IsdIQ9wj9KHd\n"
    "lXwb/2fsdQk1hkPXa4qRBR/AyeBPRL3d0aZzU+pTjV6n9dJ+KIaivuAxoyyY59XM\n"
    "36CqTZxe3VqXweRWPn40tzDcUxSVIfipJpFuK2vxpwEHdl/cFQ38/sRoHTjx61nx\n"
    "obt4RFiEMAFCxxCk/qDyYISHJmH67dIUEw7ujB4vn/gpk5f1WBY5msZMgT3pJbFv\n"
    "xD8dRgUvMpIM5poVFfhHgRq+L2dxQ2jHD5AnlpY6n8XrWy4QQa3AFsWgnD0w0Wn6\n"
    "cyU0g5AkIlP+0mMNC9LIPVc9LGKUTrqciBgx8Rysy3mskg8pElEe1ouQOi/Zx8UA\n"
    "G3RXqvjxXLkMp3S7PKgrr48uZHAso59+k33EkF/ceLsr3r3VY1WWsiszDfK+vbj6\n"
    "Bbxvtv/S2ZYXMA7nM2Ysu20BpHm9LLo4y8HqDeixqw5enOwuDKSeKD1pJgJ5CUYq\n"
    "RbA/cUYxHJ36NuPDxec+bhqiJq8RMR4pGGcJ7BvirJxYPJX0LqIfTHL1t2dVm//A\n"
    "meMDNiM2quAzpBosjvaWaRUcankYE1dL59eVugoDKCNCPg72LjXBB7bPWZT2Xhkc\n"
    "co0etruIYYJmQ3LO0vGe9pOYBu2FHx5FY72b8gpshm+umdL5AgMBAAEwDQYJKoZI\n"
    "hvcNAQELBQADggIBAEN3778acjJ46yKzYoM+wiyyiMtmOrf+zvJsF0oK4KcWgec2\n"
    "O2grmhxxDdF5c/P6XasDQFl8feQfGeDY76DaLrcmIiOYrtnqg11IZcPOHx5nbpu2\n"
    "ZVV5LiMS8nHhQIyxMF/WYYKGzBQ5AY2+t6dozyDo3R4O7CCmsFKc8NaB4maC7Q16\n"
    "7MxKXxtAH9I1PigjRMDpi1xQJbXJxFKhZrKBODtreL6cmv6yB4JJezI5ngIdODpI\n"
    "MaIS0reRGN4QUpzDaXwYBTaOHaIDShPDOfiA5ai4xK/dEWG2rDu+yk7g5SEKMAxU\n"
    "mfUCO1MGY6NwQupLUyfO2VjvfYeB+ipJq6F8tYMGrQJU/PCQT6nxaZdSoZZQF72y\n"
    "OuYVfKjnj7MWapGKC3ea1oTUvkwDePe8xg3DBuXImp5mO4MG5K/oVv5SnNVmcUGq\n"
    "L9WBrvypJK+3x3vbdyH02DR10TcMRSbDODmW59nx2PQEDUM7ddNZ60dRn8Hdgoz2\n"
    "s/Sk3I1gXvZLQ/shS4Aa7XKz/TqhPNrBnMvSnp5/PtjjeBwxIBimuuM1ALFfwz91\n"
    "KpzwqfTswuGIO8TWKJZzNTsdwScqmbZTtiVs6GaEZ3FQX5qnrbybX53S2R9fNKm+\n"
    "qGj7FtRiSdjkZ7pmNpma6ycPR0RBZyL3aHnig+DDfRRt8TgrZzY3aXBReONb\n"
    "-----END CERTIFICATE-----";

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    THROW_ON_SYS_ERROR_FMT(chmod(HRN_SERVER_KEY, 0600) == -1, FileModeError, "unable to set mode on " HRN_SERVER_KEY);
    THROW_ON_SYS_ERROR_FMT(
        chmod(HRN_SERVER_CLIENT_KEY, 0600) == -1, FileModeError, "unable to set mode on " HRN_SERVER_CLIENT_KEY);

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

            IoClient *socketClient = sckClientNew(STR(hostLocal), 7777, 0, 0);
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

        TEST_ASSIGN(client, sckClientNew(STRDEF("localhost"), hrnServerPort(0), 100, 100), "new client");
        TEST_ERROR_FMT(
            ioClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            hrnServerPort(0));

        // This address should not be in use in a test environment -- if it is the test will fail
        TEST_ASSIGN(client, sckClientNew(STRDEF("172.31.255.255"), hrnServerPort(0), 100, 100), "new client");
        TEST_ERROR_FMT(ioClientOpen(client), HostConnectError, "timeout connecting to '172.31.255.255:%u'", hrnServerPort(0));
    }

    // Additional coverage not provided by testing with actual certificates
    // *****************************************************************************************************************************
    if (testBegin("tlsAsn1ToStr(), tlsClientHostVerify(), and tlsClientHostVerifyName()"))
    {
        TEST_ERROR(tlsAsn1ToStr(NULL), CryptoError, "TLS certificate name entry is missing");

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
            client,
            tlsClientNew(sckClientNew(STRDEF("99.99.99.99.99"), 7777, 0, 0), STRDEF("X"), 0, 0, true, NULL, NULL, NULL, NULL, NULL),
            "new client");
        TEST_RESULT_STR_Z(ioClientName(client), "99.99.99.99.99:7777", " check name");
        TEST_ERROR(
            ioClientOpen(client), HostConnectError, "unable to get address for '99.99.99.99.99': [-2] Name or service not known");

        TEST_ASSIGN(
            client,
            tlsClientNew(
                sckClientNew(STRDEF("localhost"), hrnServerPort(0), 100, 100), STRDEF("X"), 100, 100, true, NULL, NULL, NULL, NULL,
                NULL),
            "new client");
        TEST_ERROR_FMT(
            ioClientOpen(client), HostConnectError, "unable to connect to 'localhost:%u': [111] Connection refused",
            hrnServerPort(0));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing ca cert/path");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, STRDEF("bogus.crt"),
                    STRDEF("/bogus"), NULL, NULL, NULL)),
            CryptoError, "unable to set user-defined CA certificate location: [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing client cert");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF("/bogus"), STRDEF("/bogus"), NULL)),
            CryptoError, "unable to load cert file '/bogus': [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("missing client key");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF("/bogus"), NULL)),
            CryptoError, "unable to load key file '/bogus': [33558530] No such file or directory");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("client cert and key do not match");

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(HRN_SERVER_KEY), NULL)),
            CryptoError,
            "unable to load key file '" HRN_PATH_REPO "/test/certificate/pgbackrest-test-server.key': [185073780] key values"
                " mismatch");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("client cert with passphrase");

        storagePutP(storageNewWriteP(storageTest, STRDEF("client-pwd.key"), .modeFile = 0600), BUFSTRZ(testClientPwdKey));

        TRY_BEGIN()
        {
            TEST_ERROR(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(TEST_PATH "/client-pwd.key"), NULL),
                CryptoError, "unable to load key file '" TEST_PATH "/client-pwd.key': [101077092] bad decrypt");
        }
        CATCH(TestError)
        {
            TEST_ERROR(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(TEST_PATH "/client-pwd.key"), NULL),
                CryptoError, "unable to load key file '" TEST_PATH "/client-pwd.key': [151429224] bad password read");
        }
        TRY_END();

        storageRemoveP(storageTest, STRDEF("client-pwd.key"));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("key with bad permissions");

        storagePutP(storageNewWriteP(storageTest, STRDEF("client-bad-perm.key")), BUFSTRDEF("bogus"));

        TEST_ERROR(
            ioClientOpen(
                tlsClientNew(
                    sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL, NULL,
                    STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(TEST_PATH "/client-bad-perm.key"), NULL)),
            FileReadError,
            "key file '" TEST_PATH "/client-bad-perm.key' must not have group or other permissions\n"
            "HINT: key file permissions should be 0600 or 0400.");

        storageRemoveP(storageTest, STRDEF("client-bad-perm.key"));

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
                        .certificate = STRDEF(HRN_SERVER_CERT),
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
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true, NULL,
                            STRDEF("/bogus"), NULL, NULL, NULL)),
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
                            sckClientNew(STRDEF("test.pgbackrest.org"), hrnServerPort(0), 5000, 5000),
                            STRDEF("test.pgbackrest.org"), 0, 0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL, NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("valid ca file and match alt name");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_RESULT_VOID(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("host.test2.pgbackrest.org"), hrnServerPort(0), 5000, 5000),
                            STRDEF("host.test2.pgbackrest.org"), 0, 0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL, NULL)),
                    "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("unable to find matching hostname in certificate");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("test3.pgbackrest.org"), hrnServerPort(0), 5000, 5000),
                            STRDEF("test3.pgbackrest.org"), 0, 0, true, STRDEF(HRN_SERVER_CA), NULL, NULL, NULL, NULL)),
                    CryptoError,
                    "unable to find hostname 'test3.pgbackrest.org' in certificate common name or subject alternative names");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("certificate error");

                hrnServerScriptAccept(tls);
                hrnServerScriptClose(tls);

                TEST_ERROR_FMT(
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, true,
                            STRDEF(HRN_SERVER_CERT), NULL, NULL, NULL, NULL)),
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
                            sckClientNew(STRDEF("localhost"), hrnServerPort(0), 5000, 5000), STRDEF("X"), 0, 0, false, NULL, NULL,
                            NULL, NULL, NULL)),
                        "open connection");

                // -----------------------------------------------------------------------------------------------------------------
                hrnServerScriptEnd(tls);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        // Put CN only server cert
        storagePutP(storageNewWriteP(storageTest, STRDEF("server-cn-only.crt")), BUFSTRZ(testServerCnOnlyCert));

        // Put bad CA client cert
        storagePutP(storageNewWriteP(storageTest, STRDEF("client-bad-ca.crt")), BUFSTRZ(testClientBadCa));

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "test server", .timeout = 5000)
            {
                // TLS server to accept connections
                IoServer *socketServer = sckServerNew(STRDEF("localhost"), hrnServerPort(0), 5000);
                IoServer *tlsServer = tlsServerNew(
                    STRDEF("localhost"), STRDEF(HRN_SERVER_CA), STRDEF(HRN_SERVER_KEY), STRDEF(TEST_PATH "/server-cn-only.crt"),
                    NULL, 5000);
                IoSession *socketSession = NULL;

                TEST_RESULT_STR(ioServerName(socketServer), strNewFmt("localhost:%u", hrnServerPort(0)), "socket server name");
                TEST_RESULT_STR_Z(ioServerName(tlsServer), "localhost", "tls server name");

                // Invalid client cert
                if (TEST_64BIT())                                   // !!! ONLY RUN ON 64-BIT
                {
                    socketSession = ioServerAccept(socketServer, NULL);

                    TEST_ERROR(
                        ioServerAccept(tlsServer, socketSession), ServiceError,
                        "TLS error [1:337100934] certificate verify failed");

                    // !!! NEEDED FOR 32-BIT SINCE ERROR MESSAGES VARY
                    // TRY_BEGIN()
                    // {
                    //     TEST_RESULT_VOID(ioServerAccept(tlsServer, socketSession), "open server session");
                    // }
                    // CATCH_ANY()
                    // {
                    //     const char *const errorMessageExpected1 = "TLS error [1:337100934] certificate verify failed";
                    //     const char *const errorMessageExpected2 = "TLS error [1:336105606] certificate verify failed";
                    //
                    //     if (errorType() != &ServiceError ||
                    //         (strcmp(errorMessage(), errorMessageExpected1) != 0 && strcmp(errorMessage(), errorMessageExpected2) != 0))
                    //     {
                    //         THROW_FMT(
                    //             TestError, "EXPECTED %s: %s (or %s)\n\n BUT GOT %s: %s\n\nTHROWN AT:\n%s",
                    //             errorTypeName(&ServiceError), errorMessageExpected1, errorMessageExpected2, errorName(), errorMessage(),
                    //             errorStackTrace());
                    //     }
                    //
                    //     hrnTestResultEnd();
                    // }
                    // TRY_END();
                    //
                    // HRN_FORK_CHILD_NOTIFY_GET();
                }

                // Valid client cert
                socketSession = ioServerAccept(socketServer, NULL);
                IoSession *tlsSession = NULL;
                TEST_ASSIGN(tlsSession, ioServerAccept(tlsServer, socketSession), "open server session");

                TEST_RESULT_BOOL(ioSessionAuthenticated(tlsSession), true, "server session authenticated");
                TEST_RESULT_STR_Z(ioSessionPeerName(tlsSession), "pgbackrest-client", "check peer name");
                TEST_RESULT_VOID(ioWrite(ioSessionIoWrite(tlsSession), BUFSTRDEF("message")), "server write");
                TEST_RESULT_VOID(ioWriteFlush(ioSessionIoWrite(tlsSession)), "server write flush");

                TEST_RESULT_VOID(ioSessionFree(tlsSession), "free server session");

                // No client cert
                socketSession = ioServerAccept(socketServer, NULL);
                TEST_ASSIGN(tlsSession, ioServerAccept(tlsServer, socketSession), "open server session");

                TEST_RESULT_BOOL(ioSessionAuthenticated(tlsSession), false, "server session not authenticated");
                TEST_RESULT_VOID(ioWrite(ioSessionIoWrite(tlsSession), BUFSTRDEF("message2")), "server write");
                TEST_RESULT_VOID(ioWriteFlush(ioSessionIoWrite(tlsSession)), "server write flush");

                TEST_RESULT_VOID(ioSessionFree(tlsSession), "free server session");

                // Client crl rejects server !!! NOT WORKING
                socketSession = ioServerAccept(socketServer, NULL);
                TEST_ASSIGN(tlsSession, ioServerAccept(tlsServer, socketSession), "open server session");

                TEST_RESULT_VOID(ioWrite(ioSessionIoWrite(tlsSession), BUFSTRDEF("message3")), "server write");
                TEST_RESULT_VOID(ioWriteFlush(ioSessionIoWrite(tlsSession)), "server write flush");

                TEST_RESULT_VOID(ioSessionFree(tlsSession), "free server session");

                // Free socket
                ioServerFree(socketServer);
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "test client")
            {
                IoSession *clientSession = NULL;

                if (TEST_64BIT())                                   // !!! ONLY RUN ON 64-BIT
                {
                    TEST_TITLE("client cert is invalid (signed by another CA)");

                    TEST_ASSIGN(
                        clientSession,
                        ioClientOpen(
                            tlsClientNew(
                                sckClientNew(STRDEF("127.0.0.1"), hrnServerPort(0), 5000, 5000), STRDEF("127.0.0.1"), 5000, 5000,
                                true, NULL, NULL, STRDEF(TEST_PATH "/client-bad-ca.crt"), STRDEF(HRN_SERVER_CLIENT_KEY), NULL)),
                        "client open");

                    TEST_ERROR(
                        ioRead(ioSessionIoRead(clientSession), bufNew(1)), ServiceError,
                        "TLS error [1:336151576] tlsv1 alert unknown ca");

                    // !!! NEEDED FOR 32-BIT SINCE ERROR MESSAGES VARY
                    // TRY_BEGIN()
                    // {
                    //     TEST_ERROR(
                    //         ioRead(ioSessionIoRead(clientSession), bufNew(1)), ServiceError,
                    //         "TLS error [1:336151576] tlsv1 alert unknown ca");
                    // }
                    // CATCH(TestError)
                    // {
                    //     TEST_ERROR(
                    //         ioRead(ioSessionIoRead(clientSession), bufNew(1)), ServiceError,
                    //         "TLS error [1:336105606] tlsv1 alert unknown ca");
                    // }
                    // TRY_END();

                    // HRN_FORK_PARENT_NOTIFY_PUT(0);

                    TEST_RESULT_VOID(ioSessionFree(clientSession), "free client session");
                }

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("client cert is valid");

                TEST_ASSIGN(
                    clientSession,
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("127.0.0.1"), hrnServerPort(0), 5000, 5000), STRDEF("127.0.0.1"), 5000, 5000, true,
                            NULL, NULL, STRDEF(HRN_SERVER_CLIENT_CERT), STRDEF(HRN_SERVER_CLIENT_KEY), NULL)),
                    "client open");

                Buffer *buffer = bufNew(7);
                TEST_RESULT_VOID(ioRead(ioSessionIoRead(clientSession), buffer), "client read");
                TEST_RESULT_STR_Z(strNewBuf(buffer), "message", "check read");

                TEST_RESULT_VOID(ioSessionFree(clientSession), "free client session");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("no client cert");

                TEST_ASSIGN(
                    clientSession,
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("127.0.0.1"), hrnServerPort(0), 5000, 5000), STRDEF("127.0.0.1"), 5000, 5000, true,
                            NULL, NULL, NULL, NULL, NULL)),
                    "client open");

                buffer = bufNew(8);
                TEST_RESULT_VOID(ioRead(ioSessionIoRead(clientSession), buffer), "client read");
                TEST_RESULT_STR_Z(strNewBuf(buffer), "message2", "check read");

                TEST_RESULT_VOID(ioSessionFree(clientSession), "free client session");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("crl reject"); // !!! NOT WORKING

                TEST_ASSIGN(
                    clientSession,
                    ioClientOpen(
                        tlsClientNew(
                            sckClientNew(STRDEF("127.0.0.1"), hrnServerPort(0), 5000, 5000), STRDEF("127.0.0.1"), 5000, 5000, true,
                            NULL, NULL, NULL, NULL, STRDEF(HRN_SERVER_CRL))),
                    "client open");

                buffer = bufNew(8);
                TEST_RESULT_VOID(ioRead(ioSessionIoRead(clientSession), buffer), "client read");
                TEST_RESULT_STR_Z(strNewBuf(buffer), "message3", "check read");

                TEST_RESULT_VOID(ioSessionFree(clientSession), "free client session");
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
                        sckClientNew(hrnServerHost(), hrnServerPort(0), 5000, 5000), hrnServerHost(), 0, 0, TEST_IN_CONTAINER, NULL,
                        NULL, NULL, NULL, NULL),
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
