/***********************************************************************************************************************************
Tls Test Harness
***********************************************************************************************************************************/
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/crypto/common.h"
#include "common/error.h"
#include "common/io/socket/client.h"
#include "common/io/socket/common.h"
#include "common/io/tls/client.h"
#include "common/type/buffer.h"
#include "common/wait.h"

#include "common/harnessTest.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Test defaults
***********************************************************************************************************************************/
#define TLS_TEST_HOST                                               "tls.test.pgbackrest.org"

static TlsClient *tlsServer;
static int testServerSocket = 0;

/***********************************************************************************************************************************
Initialize TLS and listen on the specified port for TLS connections
***********************************************************************************************************************************/
void
harnessTlsServerInit(unsigned int port, const char *serverCert, const char *serverKey)
{
    // Add test hosts
    if (testContainer())
    {
        if (system("echo \"127.0.0.1 " TLS_TEST_HOST "\" | sudo tee -a /etc/hosts > /dev/null") != 0)
            THROW(AssertError, "unable to add test host to /etc/hosts");
    }

    tlsServer = tlsClientNewServer(10000, STR(serverCert), STR(serverKey));

    // Create the socket
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((testServerSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        THROW_SYS_ERROR(AssertError, "unable to create socket");

    // Set the address as reusable so we can bind again in the same process for testing
    int reuseAddr = 1;
    setsockopt(testServerSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    // Bind the address.  It might take a bit to bind if another process was recently using it so retry a few times.
    Wait *wait = waitNew(2000);
    int result;

    do
    {
        result = bind(testServerSocket, (struct sockaddr *)&address, sizeof(address));
    }
    while (result < 0 && waitMore(wait));

    if (result < 0)
        THROW_SYS_ERROR(AssertError, "unable to bind socket");

    // Listen for client connections
    if (listen(testServerSocket, 1) < 0)
        THROW_SYS_ERROR(AssertError, "unable to listen on socket");
}

/**********************************************************************************************************************************/
void
harnessTlsServerInitDefault(void)
{
    if (testContainer())
        harnessTlsServerInit(harnessTlsTestPort(), TLS_CERT_TEST_CERT, TLS_CERT_TEST_KEY);
    else
    {
        harnessTlsServerInit(
            harnessTlsTestPort(),
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".crt", testRepoPath())),
            strPtr(strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".key", testRepoPath())));
    }
}

/***********************************************************************************************************************************
Expect an exact string from the client
***********************************************************************************************************************************/
void
harnessTlsServerExpect(const char *expected)
{
    Buffer *buffer = bufNew(strlen(expected));

    ioRead(tlsClientIoRead(tlsServer), buffer);

    // Treat and ? characters as wildcards so variable elements (e.g. auth hashes) can be ignored
    String *actual = strNewBuf(buffer);

    for (unsigned int actualIdx = 0; actualIdx < strSize(actual); actualIdx++)
    {
        if (expected[actualIdx] == '?')
            ((char *)strPtr(actual))[actualIdx] = '?';
    }

    // Error if actual does not match expected
    if (!strEqZ(actual, expected))
        THROW_FMT(AssertError, "server expected '%s' but got '%s'", expected, strPtr(actual));
}

/***********************************************************************************************************************************
Send a reply to the client
***********************************************************************************************************************************/
void
harnessTlsServerReply(const char *reply)
{
    ioWrite(tlsClientIoWrite(tlsServer), BUF((unsigned char *)reply, strlen(reply)));
    ioWriteFlush(tlsClientIoWrite(tlsServer));
}

/***********************************************************************************************************************************
Accept a TLS connection from the client
***********************************************************************************************************************************/
void
harnessTlsServerAccept(void)
{
    struct sockaddr_in addr;
    unsigned int len = sizeof(addr);

    int testClientSocket = accept(testServerSocket, (struct sockaddr *)&addr, &len);

    if (testClientSocket < 0)
        THROW_SYS_ERROR(AssertError, "unable to accept socket");

    sckOptionSet(testClientSocket);

    tlsClientAccept(tlsServer, sckSessionNew(sckSessionTypeServer, testClientSocket, STRDEF("127.0.0.1"), 0, 10000));
}

/***********************************************************************************************************************************
Close the connection
***********************************************************************************************************************************/
void
harnessTlsServerClose(void)
{
    tlsClientClose(tlsServer);
}

/**********************************************************************************************************************************/
const String *harnessTlsTestHost(void)
{
    return strNew(testContainer() ? TLS_TEST_HOST : "localhost");
}

/**********************************************************************************************************************************/
unsigned int harnessTlsTestPort(void)
{
    return 44443 + testIdx();
}
