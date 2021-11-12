/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/wait.h>

#include "command/remote/remote.h"
#include "command/server/server.h"
#include "common/debug.h"
#include "common/exit.h"
#include "common/fork.h"
#include "common/io/socket/server.h"
#include "common/io/tls/server.h"
#include "config/config.h"
#include "config/load.h"
#include "protocol/helper.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct ServerLocal
{
    MemContext *memContext;                                         // Mem context for server

    unsigned int argListSize;                                       // Argument list size
    const char **argList;                                           // Argument list

    bool sigHup;                                                    // SIGHUP was caught
    bool sigTerm;                                                   // SIGTERM was caught

    IoServer *socketServer;                                         // Socket server
    IoServer *tlsServer;                                            // TLS server
} serverLocal;

/***********************************************************************************************************************************
Initialization can be redone when options change
***********************************************************************************************************************************/
static void
cmdServerInit(void)
{
    // Initialize mem context
    if (serverLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("Server")
            {
                serverLocal.memContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    MEM_CONTEXT_BEGIN(serverLocal.memContext)
    {
        // Free old servers
        ioServerFree(serverLocal.socketServer);
        ioServerFree(serverLocal.tlsServer);

        // Create new servers
        serverLocal.socketServer = sckServerNew(
            cfgOptionStr(cfgOptTlsServerAddress), cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptProtocolTimeout));
        serverLocal.tlsServer = tlsServerNew(
            cfgOptionStr(cfgOptTlsServerAddress), cfgOptionStr(cfgOptTlsServerCaFile), cfgOptionStr(cfgOptTlsServerKeyFile),
            cfgOptionStr(cfgOptTlsServerCertFile), cfgOptionStrNull(cfgOptTlsServerCrlFile),
            cfgOptionUInt64(cfgOptProtocolTimeout));
    }
    MEM_CONTEXT_END();
}

/***********************************************************************************************************************************
Handlers to set flags on signals
***********************************************************************************************************************************/
static void
cmdServerSigHup(int signalType)
{
    (void)signalType;
    serverLocal.sigHup = true;
}

static void
cmdServerSigTerm(int signalType)
{
    (void)signalType;
    serverLocal.sigTerm = true;
}

/**********************************************************************************************************************************/
void
cmdServer(const unsigned int argListSize, const char *argList[])
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, argListSize);
        FUNCTION_LOG_PARAM(CHARPY, argList);
    FUNCTION_LOG_END();

    // Initialize server
    cmdServerInit();

    // Set arguments used for reload
    serverLocal.argListSize = argListSize;
    serverLocal.argList = argList;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set signal handlers
        sigaction(SIGHUP, &(struct sigaction){.sa_handler = cmdServerSigHup}, NULL);
        sigaction(SIGTERM, &(struct sigaction){.sa_handler = cmdServerSigTerm}, NULL);
        sigaction(SIGCHLD, &(struct sigaction){.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT}, NULL);

        // Accept connections indefinitely. The only way to exit this loop is for the process to receive a signal.
        do
        {
            // Accept a new connection
            IoSession *const socketSession = ioServerAccept(serverLocal.socketServer, NULL);

            if (socketSession != NULL)
            {
                // Fork off the child process
                pid_t pid = forkSafe();

                if (pid == 0)
                {
                    // Close the server socket so we don't hold the port open if the parent exits first
                    ioServerFree(serverLocal.socketServer);

                    // Disable logging and close log file
                    logClose();

                    // Start standard remote processing if a server is returned
                    ProtocolServer *server = protocolServer(serverLocal.tlsServer, socketSession);

                    if (server != NULL)
                        cmdRemote(server);

                    break;
                }

                // Free the socket since the child is now using it
                ioSessionFree(socketSession);
            }

            // Reload configuration
            if (serverLocal.sigHup)
            {
                LOG_DETAIL("configuration reload begin");

                // Reload configuration
                cfgLoad(serverLocal.argListSize, serverLocal.argList);

                // Reinitialize server
                cmdServerInit();

                LOG_DETAIL("configuration reload end");

                // Reset flag
                serverLocal.sigHup = false;
            }
        }
        while (!serverLocal.sigTerm);
    }
    MEM_CONTEXT_TEMP_END();

    // !!! TERMINATE REMAINING CHILD PROCESSES

    FUNCTION_LOG_RETURN_VOID();
}
