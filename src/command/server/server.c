/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/wait.h>

#include "command/remote/remote.h"
#include "command/server/server.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/io/tls/server.h"
#include "common/io/socket/server.h"
#include "config/config.h"
#include "config/load.h"
#include "config/protocol.h"
#include "db/protocol.h"
#include "protocol/helper.h"
#include "protocol/server.h"
#include "storage/remote/protocol.h"

/**********************************************************************************************************************************/
static bool
cmdServerFork(IoServer *const tlsServer, IoSession *const socketSession, const String *const host)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_SERVER, tlsServer);
        FUNCTION_LOG_PARAM(IO_SESSION, socketSession);
        FUNCTION_LOG_PARAM(STRING, host);
    FUNCTION_LOG_END();

    // Fork off the server process
    pid_t pid = forkSafe();

    if (pid == 0)
    {
        // !!! DO WE NEED TO FREE THE SOCKET SERVER HERE?

        // Start protocol handshake on the bare socket
        ProtocolServer *const socketServer = protocolServerNew(
            PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(socketSession),
            ioSessionIoWrite(socketSession));

        // Negotiate TLS if requested
        ProtocolServerCommandGetResult command = protocolServerCommandGet(socketServer);

        if (command.id == PROTOCOL_COMMAND_TLS)
        {
            // Acknowledge TLS request. It is very important that the client and server are synchronized here because we need to
            // hand the bare socket off the TLS and there should not be any data held in the IoRead/IoWrite buffers.
            protocolServerDataEndPut(socketServer);

            // Start TLS
            IoSession *const tlsSession = ioServerAccept(tlsServer, socketSession);

            ProtocolServer *const tlsServer = protocolServerNew(
                PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(tlsSession),
                ioSessionIoWrite(tlsSession));

            // Get parameter list from the client and load it
            command = protocolServerCommandGet(tlsServer);
            CHECK(command.id == PROTOCOL_COMMAND_CONFIG);

            StringList *const paramList = pckReadStrLstP(pckReadNewBuf(command.param));
            strLstInsert(paramList, 0, cfgExe());
            cfgLoad(strLstSize(paramList), strLstPtr(paramList));

            protocolServerDataEndPut(tlsServer);

            // !!! NEED TO SET READ TIMEOUT TO PROTOCOL-TIMEOUT HERE

            // Detach from parent process
            forkDetach();

            // Start standard remote processing
            cmdRemote(tlsServer);

            ioSessionFree(tlsSession);
        }
        // Else a noop used to ping the server
        else
        {
            // Process noop
            CHECK(command.id == PROTOCOL_COMMAND_NOOP);
            protocolServerDataEndPut(socketServer);

            ioSessionFree(socketSession);
        }
    }
    else
    {
        // The process that was just forked should return immediately
        int processStatus;

        THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

        // The first fork should exit with success. If not, something went wrong during the second fork.
        CHECK(WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0);
    }

    FUNCTION_LOG_RETURN(BOOL, pid != 0);
}

void
cmdServer(uint64_t connectionMax)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(connectionMax > 0);

    const String *const host = STRDEF("localhost");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *const tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert), cfgOptionUInt64(cfgOptIoTimeout));
        IoServer *const socketServer = sckServerNew(host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptIoTimeout));

        // Accept connections until connection max is reached. !!! THIS IS A HACK TO LIMIT THE LOOP AND ALLOW TESTING. IT SHOULD BE
        // REPLACED WITH A STOP REQUEST FROM AN AUTHENTICATED CLIENT.
        do
        {
            // Accept a new connection
            IoSession *const socketSession = ioServerAccept(socketServer, NULL);

            // Fork the child and break out of the loop when the child returns
            if (!cmdServerFork(tlsServer, socketSession, host))
                break;

            // Free the socket since the child is now using it
            ioSessionFree(socketSession);
        }
        while (--connectionMax > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
