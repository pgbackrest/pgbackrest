/***********************************************************************************************************************************
TLS Test Harness
***********************************************************************************************************************************/
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/ssl.h>

#include "common/crypto/common.h"
#include "common/error.h"
#include "common/io/socket/session.h"
#include "common/io/tls/session.h"
#include "common/log.h"
#include "common/type/buffer.h"
#include "common/type/json.h"
#include "common/wait.h"

#include "common/harnessDebug.h"
#include "common/harnessTest.h"
#include "common/harnessTls.h"

/***********************************************************************************************************************************
Command enum
***********************************************************************************************************************************/
typedef enum
{
    hrnTlsCmdAbort,
    hrnTlsCmdAccept,
    hrnTlsCmdClose,
    hrnTlsCmdDone,
    hrnTlsCmdExpect,
    hrnTlsCmdReply,
    hrnTlsCmdSleep,
} HrnTlsCmd;

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define TLS_TEST_HOST                                               "tls.test.pgbackrest.org"
#define TLS_CERT_TEST_KEY                                           TLS_CERT_FAKE_PATH "/pgbackrest-test.key"

/***********************************************************************************************************************************
Send commands to the server
***********************************************************************************************************************************/
static void
hrnServerScriptCommand(IoWrite *write, HrnTlsCmd cmd, const Variant *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(ENUM, cmd);
        FUNCTION_HARNESS_PARAM(VARIANT, data);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    ioWriteStrLine(write, jsonFromUInt(cmd));
    ioWriteStrLine(write, jsonFromVar(data));
    ioWriteFlush(write);

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
IoWrite *hrnServerScriptBegin(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    write = write;
    ioWriteOpen(write);

    FUNCTION_HARNESS_RESULT(IO_WRITE, write);
}

void hrnServerScriptEnd(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    hrnServerScriptCommand(write, hrnTlsCmdDone, NULL);

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
void
hrnServerScriptAbort(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnTlsCmdAbort, NULL);

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptAccept(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnTlsCmdAccept, NULL);

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptClose(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnTlsCmdClose, NULL);

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptExpect(IoWrite *write, const String *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRING, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnTlsCmdExpect, VARSTR(data));

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptExpectZ(IoWrite *write, const char *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRINGZ, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnTlsCmdExpect, VARSTRZ(data));

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptReply(IoWrite *write, const String *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRING, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnTlsCmdReply, VARSTR(data));

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptReplyZ(IoWrite *write, const char *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRINGZ, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnTlsCmdReply, VARSTRZ(data));

    FUNCTION_HARNESS_RESULT_VOID();
}

void
hrnServerScriptSleep(IoWrite *write, TimeMSec sleepMs)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(TIME_MSEC, sleepMs);
    FUNCTION_HARNESS_END();

    ASSERT(sleepMs > 0);

    hrnServerScriptCommand(write, hrnTlsCmdSleep, VARUINT64(sleepMs));

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
void hrnTlsServerRunParam(IoRead *read, HrnServerProtocol protocol, unsigned int port, const String *certificate, const String *key)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_READ, read);
        FUNCTION_HARNESS_PARAM(ENUM, protocol);
        FUNCTION_HARNESS_PARAM(UINT, port);
        FUNCTION_HARNESS_PARAM(STRING, certificate);
        FUNCTION_HARNESS_PARAM(STRING, key);
    FUNCTION_HARNESS_END();

    ASSERT(read != NULL);
    ASSERT(certificate != NULL);
    ASSERT(key != NULL);

    // Open read connection to client
    ioReadOpen(read);

    // Add test hosts
    if (testContainer())
    {
        if (system("echo \"127.0.0.1 " TLS_TEST_HOST "\" | sudo tee -a /etc/hosts > /dev/null") != 0)
            THROW(AssertError, "unable to add test host to /etc/hosts");
    }

    // Initialize ssl and create a context
    SSL_CTX *serverContext = NULL;

    if (protocol == hrnServerProtocolTls)
    {
        cryptoInit();

        const SSL_METHOD *method = SSLv23_method();
        cryptoError(method == NULL, "unable to load TLS method");

        serverContext = SSL_CTX_new(method);
        cryptoError(serverContext == NULL, "unable to create TLS context");

        // Configure the context by setting key and cert
        cryptoError(
            SSL_CTX_use_certificate_file(serverContext, strZ(certificate), SSL_FILETYPE_PEM) <= 0,
            "unable to load server certificate");
        cryptoError(
            SSL_CTX_use_PrivateKey_file(serverContext, strZ(key), SSL_FILETYPE_PEM) <= 0, "unable to load server private key");
    }

    // Create the socket
    int serverSocket;

    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        THROW_SYS_ERROR(AssertError, "unable to create socket");

    // Set the address as reusable so we can bind again in the same process for testing
    int reuseAddr = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    // Bind the address.  It might take a bit to bind if another process was recently using it so retry a few times.
    Wait *wait = waitNew(2000);
    int result;

    do
    {
        result = bind(serverSocket, (struct sockaddr *)&address, sizeof(address));
    }
    while (result < 0 && waitMore(wait));

    if (result < 0)
        THROW_SYS_ERROR(AssertError, "unable to bind socket");

    // Listen for client connections
    if (listen(serverSocket, 1) < 0)
        THROW_SYS_ERROR(AssertError, "unable to listen on socket");

    // Loop until no more commands
    IoSession *serverSession = NULL;
    bool done = false;

    do
    {
        HrnTlsCmd cmd = jsonToUInt(ioReadLine(read));
        const Variant *data = jsonToVar(ioReadLine(read));

        switch (cmd)
        {
            case hrnTlsCmdAbort:
            {
                // Only makes since to abort in TLS, otherwise it is just a close
                ASSERT(protocol == hrnServerProtocolTls);

                ioSessionFree(serverSession);
                serverSession = NULL;

                break;
            }

            case hrnTlsCmdAccept:
            {
                struct sockaddr_in addr;
                unsigned int len = sizeof(addr);

                int testClientSocket = accept(serverSocket, (struct sockaddr *)&addr, &len);

                if (testClientSocket < 0)
                    THROW_SYS_ERROR(AssertError, "unable to accept socket");

                serverSession = sckSessionNew(ioSessionRoleServer, testClientSocket, STRDEF("client"), port, 5000);

                if (protocol == hrnServerProtocolTls)
                {
                    SSL *testClientSSL = SSL_new(serverContext);
                    serverSession = tlsSessionNew(testClientSSL, serverSession, 5000);
                }

                break;
            }

            case hrnTlsCmdClose:
            {
                if (serverSession == NULL)
                    THROW(AssertError, "session is already closed");

                ioSessionClose(serverSession);
                ioSessionFree(serverSession);
                serverSession = NULL;

                break;
            }

            case hrnTlsCmdDone:
            {
                done = true;
                break;
            }

            case hrnTlsCmdExpect:
            {
                const String *expected = varStr(data);
                Buffer *buffer = bufNew(strSize(expected));

                ioRead(ioSessionIoRead(serverSession), buffer);

                // Treat any ? characters as wildcards so variable elements (e.g. auth hashes) can be ignored
                String *actual = strNewBuf(buffer);

                for (unsigned int actualIdx = 0; actualIdx < strSize(actual); actualIdx++)
                {
                    if (strZ(expected)[actualIdx] == '?')
                        ((char *)strZ(actual))[actualIdx] = '?';
                }

                // Error if actual does not match expected
                if (!strEq(actual, expected))
                    THROW_FMT(AssertError, "server expected '%s' but got '%s'", strZ(expected), strZ(actual));

                break;
            }

            case hrnTlsCmdReply:
            {
                ioWrite(ioSessionIoWrite(serverSession), BUFSTR(varStr(data)));
                ioWriteFlush(ioSessionIoWrite(serverSession));

                break;
            }

            case hrnTlsCmdSleep:
            {
                sleepMSec(varUInt64Force(data));

                break;
            }
        }
    }
    while (!done);

    // Free TLS context
    if (protocol == hrnServerProtocolTls)
        SSL_CTX_free(serverContext);

    FUNCTION_HARNESS_RESULT_VOID();
}

void hrnTlsServerRun(IoRead *read, HrnServerProtocol protocol, unsigned int port)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_READ, read);
        FUNCTION_HARNESS_PARAM(ENUM, protocol);
        FUNCTION_HARNESS_PARAM(UINT, port);
    FUNCTION_HARNESS_END();

    if (testContainer())
        hrnTlsServerRunParam(read, protocol, port, STRDEF(TLS_CERT_TEST_CERT), STRDEF(TLS_CERT_TEST_KEY));
    else
    {
        hrnTlsServerRunParam(
            read, protocol, port, strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".crt", testRepoPath()),
            strNewFmt("%s/" TEST_CERTIFICATE_PREFIX ".key", testRepoPath()));
    }

    FUNCTION_HARNESS_RESULT_VOID();
}

/**********************************************************************************************************************************/
const String *hrnTlsServerHost(void)
{
    return strNew(testContainer() ? TLS_TEST_HOST : "127.0.0.1");
}

/**********************************************************************************************************************************/
unsigned int hrnTlsServerPort(unsigned int portIdx)
{
    ASSERT(portIdx < HRN_SERVER_PORT_MAX);

    return 44443 + (HRN_SERVER_PORT_MAX * testIdx()) + portIdx;
}
