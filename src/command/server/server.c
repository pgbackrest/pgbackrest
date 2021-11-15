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

    List *processList;                                              // List of child processes

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
                serverLocal.processList = lstNewP(sizeof(pid_t));
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
cmdServerSigHup(const int signalType)
{
    (void)signalType;
    serverLocal.sigHup = true;
}

static void
cmdServerSigTerm(const int signalType)
{
    (void)signalType;
    serverLocal.sigTerm = true;
}

/***********************************************************************************************************************************
Handler to reap child processes
***********************************************************************************************************************************/
static void
cmdServerSigChild(const int signalType, siginfo_t *signalInfo, void *context)
{
    (void)signalType;
    (void)context;

    ASSERT(signalInfo->si_code == CLD_EXITED);

    // Find the process and remove it
    for (unsigned int processIdx  = 0; processIdx < lstSize(serverLocal.processList); processIdx++)
    {
        if (*(int *)lstGet(serverLocal.processList, processIdx) == signalInfo->si_pid)
            lstRemoveIdx(serverLocal.processList, processIdx);
    }
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
        sigaction(
            SIGCHLD, &(struct sigaction){.sa_sigaction = cmdServerSigChild, .sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_SIGINFO},
            NULL);

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
                    // Reset SIGCHLD to default
                    sigaction(SIGCHLD, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);

                    // Set standard signal handlers
                    exitInit();

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
                // Add process to list
                else
                    lstAdd(serverLocal.processList, &pid);

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

    // Terminate any remaining children on SIGTERM. Disable the callback so it does not fire in the middle of the loop.
    if (serverLocal.sigTerm)
    {
        sigaction(SIGCHLD, &(struct sigaction){.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT}, NULL);

        for (unsigned int processIdx  = 0; processIdx < lstSize(serverLocal.processList); processIdx++)
        {
            pid_t pid = *(int *)lstGet(serverLocal.processList, processIdx);

            LOG_WARN_FMT("terminate child process %d", pid);
            kill(pid, SIGTERM);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}
