/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/wait.h>

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

/***********************************************************************************************************************************
Command handlers
***********************************************************************************************************************************/
static const ProtocolServerHandler commandRemoteHandlerList[] =
{
    PROTOCOL_SERVER_HANDLER_DB_LIST
    PROTOCOL_SERVER_HANDLER_OPTION_LIST
    PROTOCOL_SERVER_HANDLER_STORAGE_REMOTE_LIST
};

/**********************************************************************************************************************************/
static bool
cmdServerFork(IoSession *const socketSession, const String *const host)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_SESSION, socketSession);
        FUNCTION_LOG_PARAM(STRING, host);
    FUNCTION_LOG_END();

    // Fork off the async process
    pid_t pid = forkSafe();

    if (pid == 0)
    {
        IoServer *const tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert), cfgOptionUInt64(cfgOptProtocolTimeout));
        IoSession *const tlsSession = ioServerAccept(tlsServer, socketSession);

        ProtocolServer *const server = protocolServerNew(
            PROTOCOL_SERVICE_REMOTE_STR, PROTOCOL_SERVICE_REMOTE_STR, ioSessionIoRead(tlsSession),
            ioSessionIoWrite(tlsSession));

        // Get the command and put data end. No need to check parameters since we know this is the first noop.
        CHECK(protocolServerCommandGet(server).id == PROTOCOL_COMMAND_NOOP);
        protocolServerDataEndPut(server);

        // Get parameter list from the client and load it
        ProtocolServerCommandGetResult command = protocolServerCommandGet(server);
        CHECK(command.id == PROTOCOL_COMMAND_CONFIG);

        StringList *const paramList = pckReadStrLstP(pckReadNewBuf(command.param));
        strLstInsert(paramList, 0, cfgExe());
        cfgLoad(strLstSize(paramList), strLstPtr(paramList));

        protocolServerDataEndPut(server);

        // Detach from parent process
        forkDetach();

        protocolServerProcess(
            server, NULL, commandRemoteHandlerList, PROTOCOL_SERVER_HANDLER_LIST_SIZE(commandRemoteHandlerList));

        ioSessionFree(tlsSession);
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
        IoServer *const socketServer = sckServerNew(
            host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptProtocolTimeout));

        // Accept connections until connection max is reached. !!! THIS IS A HACK TO LIMIT THE LOOP AND ALLOW TESTING. IT SHOULD BE
        // REPLACED WITH A STOP REQUEST FROM AN AUTHENTICATED CLIENT.
        do
        {
            IoSession *const socketSession = ioServerAccept(socketServer, NULL);

            if (!cmdServerFork(socketSession, host))
                break;

            ioSessionFree(socketSession);
        }
        while (--connectionMax > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
