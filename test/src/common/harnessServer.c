/***********************************************************************************************************************************
Server Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/conf.h>
#include <openssl/ssl.h>

#include "common/crypto/common.h"
#include "common/io/socket/server.h"
#include "common/io/tls/server.h"
#include "common/io/tls/session.h"
#include "common/log.h"
#include "common/type/buffer.h"
#include "common/type/json.h"
#include "common/wait.h"

#include "common/harnessDebug.h"
#include "common/harnessServer.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Command enum
***********************************************************************************************************************************/
typedef enum
{
    hrnServerCmdAbort,
    hrnServerCmdAccept,
    hrnServerCmdClose,
    hrnServerCmdDone,
    hrnServerCmdExpect,
    hrnServerCmdReply,
    hrnServerCmdSleep,
} HrnServerCmd;

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define HRN_SERVER_HOST                                             "tls.test.pgbackrest.org"

/***********************************************************************************************************************************
Local data
***********************************************************************************************************************************/
static struct HrnServerLocal
{
    unsigned int portOffsetNext;                                    // Next server port offset to be returned
} hrnServerLocal;

/**********************************************************************************************************************************/
void
hrnServerInit(void)
{
    FUNCTION_HARNESS_VOID();

    // Set correct permissions on private keys
    THROW_ON_SYS_ERROR(
        chmod(zNewFmt("%s/" HRN_SERVER_CERT_PREFIX "server.key", hrnPathRepo()), 0600) == -1, FileModeError,
        "unable to set mode on server key");
    THROW_ON_SYS_ERROR(
        chmod(zNewFmt("%s/" HRN_SERVER_CERT_PREFIX "client.key", hrnPathRepo()), 0600) == -1, FileModeError,
        "unable to set mode on client key");

    // Add hostname when running in a container
    if (testContainer())
    {
        if (system("echo \"127.0.0.1 " HRN_SERVER_HOST "\" | sudo tee -a /etc/hosts > /dev/null") != 0)
            THROW(AssertError, "unable to add test host to /etc/hosts");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Send commands to the server
***********************************************************************************************************************************/
static void
hrnServerScriptCommand(IoWrite *write, HrnServerCmd cmd, const Variant *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(ENUM, cmd);
        FUNCTION_HARNESS_PARAM(VARIANT, data);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    ioWriteStrLine(write, jsonFromVar(VARUINT(cmd)));
    ioWriteStrLine(write, jsonFromVar(data));
    ioWriteFlush(write);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoWrite *
hrnServerScriptBegin(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    FUNCTION_HARNESS_RETURN(IO_WRITE, write);
}

void
hrnServerScriptEnd(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    ASSERT(write != NULL);

    hrnServerScriptCommand(write, hrnServerCmdDone, NULL);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnServerScriptAbort(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnServerCmdAbort, NULL);

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptAccept(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnServerCmdAccept, NULL);

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptClose(IoWrite *write)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
    FUNCTION_HARNESS_END();

    hrnServerScriptCommand(write, hrnServerCmdClose, NULL);

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptExpect(IoWrite *write, const String *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRING, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnServerCmdExpect, VARSTR(data));

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptExpectZ(IoWrite *write, const char *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRINGZ, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnServerCmdExpect, VARSTRZ(data));

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptReply(IoWrite *write, const String *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRING, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnServerCmdReply, VARSTR(data));

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptReplyZ(IoWrite *write, const char *data)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(STRINGZ, data);
    FUNCTION_HARNESS_END();

    ASSERT(data != NULL);

    hrnServerScriptCommand(write, hrnServerCmdReply, VARSTRZ(data));

    FUNCTION_HARNESS_RETURN_VOID();
}

void
hrnServerScriptSleep(IoWrite *write, TimeMSec sleepMs)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_WRITE, write);
        FUNCTION_HARNESS_PARAM(TIME_MSEC, sleepMs);
    FUNCTION_HARNESS_END();

    ASSERT(sleepMs > 0);

    hrnServerScriptCommand(write, hrnServerCmdSleep, VARUINT64(sleepMs));

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnServerRun(IoRead *const read, const HrnServerProtocol protocol, const unsigned int port, HrnServerRunParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(IO_READ, read);
        FUNCTION_HARNESS_PARAM(ENUM, protocol);
        FUNCTION_HARNESS_PARAM(UINT, port);
        FUNCTION_HARNESS_PARAM(STRING, param.ca);
        FUNCTION_HARNESS_PARAM(STRING, param.certificate);
        FUNCTION_HARNESS_PARAM(STRING, param.key);
        FUNCTION_HARNESS_PARAM(STRING, param.address);
    FUNCTION_HARNESS_END();

    ASSERT(read != NULL);
    ASSERT(port >= HRN_SERVER_PORT_MIN);

    // Initialize ssl and create a context
    IoServer *tlsServer = NULL;

    if (protocol == hrnServerProtocolTls)
    {
        ASSERT((param.certificate != NULL && param.key != NULL) || (param.certificate == NULL && param.key == NULL));

        // If certificate and key are not set then use defaults
        if (param.certificate == NULL)
        {
            param.certificate = strNewFmt("%s/" HRN_SERVER_CERT_PREFIX "server.crt", hrnPathRepo());
            param.key = strNewFmt("%s/" HRN_SERVER_CERT_PREFIX "server.key", hrnPathRepo());
        }

        tlsServer = tlsServerNew(STRDEF(HRN_SERVER_HOST), param.ca, param.key, param.certificate, 5000);
    }

    IoServer *socketServer = sckServerNew(param.address == NULL ? STRDEF("127.0.0.1") : param.address, port, 5000);

    // Loop until no more commands
    IoSession *serverSession = NULL;
    bool done = false;

    do
    {
        HrnServerCmd cmd = varUIntForce(jsonToVar(ioReadLine(read)));
        const Variant *data = jsonToVar(ioReadLine(read));

        switch (cmd)
        {
            case hrnServerCmdAbort:
            {
                // Only makes since to abort in TLS, otherwise it is just a close
                ASSERT(protocol == hrnServerProtocolTls);

                ioSessionFree(serverSession);
                serverSession = NULL;

                break;
            }

            case hrnServerCmdAccept:
            {
                serverSession = ioServerAccept(socketServer, NULL);

                // Start TLS if requested
                if (protocol == hrnServerProtocolTls)
                    serverSession = ioServerAccept(tlsServer, serverSession);

                break;
            }

            case hrnServerCmdClose:
            {
                if (serverSession == NULL)
                    THROW(AssertError, "session is already closed");

                ioSessionClose(serverSession);
                ioSessionFree(serverSession);
                serverSession = NULL;

                break;
            }

            case hrnServerCmdDone:
                done = true;
                break;

            case hrnServerCmdExpect:
            {
                const String *expected = varStr(data);

                // Read as much as possible
                Buffer *buffer = bufNew(strSize(expected));

                TRY_BEGIN()
                {
                    // Read one byte at a time so we can error with partial data
                    Buffer *const bufferOne = bufNew(1);

                    for (size_t size = 1; size <= bufSize(buffer); size++)
                    {
                        ioReadSmall(ioSessionIoReadP(serverSession), bufferOne);

                        bufCat(buffer, bufferOne);
                        bufUsedZero(bufferOne);
                    }
                }
                CATCH(FileReadError)
                {
                    THROW_FMT(
                        AssertError, "server expected:\n'%s' but got short read:\n'%s'", strZ(expected), strZ(strNewBuf(buffer)));
                }
                TRY_END();

                // Treat any ? characters as wildcards so variable elements (e.g. auth hashes) can be ignored
                for (unsigned int bufferIdx = 0; bufferIdx < bufUsed(buffer); bufferIdx++)
                {
                    if (strZ(expected)[bufferIdx] == '?')
                        bufPtr(buffer)[bufferIdx] = '?';
                }

                const String *actual = strNewBuf(buffer);

                // Error if actual does not match expected
                if (!strEq(actual, expected))
                    THROW_FMT(AssertError, "server expected:\n'%s' but got:\n'%s'", strZ(expected), strZ(actual));

                break;
            }

            case hrnServerCmdReply:
                ioWrite(ioSessionIoWrite(serverSession), BUFSTR(varStr(data)));
                ioWriteFlush(ioSessionIoWrite(serverSession));
                break;

            case hrnServerCmdSleep:
                sleepMSec(varUInt64Force(data));
                break;
        }
    }
    while (!done);

    // Free servers
    ioServerFree(tlsServer);
    ioServerFree(socketServer);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
hrnServerHost(void)
{
    return strNewZ(testContainer() ? HRN_SERVER_HOST : "127.0.0.1");
}

/**********************************************************************************************************************************/
unsigned int
hrnServerPortNext(void)
{
    CHECK(AssertError, testIdx() < 32, "test max exceeds limit of 32");
    CHECK(
        AssertError, hrnServerLocal.portOffsetNext < HRN_SERVER_PORT_MAX,
        "requested port exceeds limit of " STRINGIFY(HRN_SERVER_PORT_MAX));

    return HRN_SERVER_PORT_MIN + (HRN_SERVER_PORT_MAX * testIdx()) + hrnServerLocal.portOffsetNext++;
}
