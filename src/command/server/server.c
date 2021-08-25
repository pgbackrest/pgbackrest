/***********************************************************************************************************************************
Server Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/wait.h>

#include "command/remote/remote.h"
#include "command/server/server.h"
#include "common/debug.h"
#include "common/fork.h"
#include "common/io/socket/server.h"
#include "common/io/tls/server.h"
#include "config/config.h"
#include "protocol/helper.h"

/**********************************************************************************************************************************/
void
cmdServer(uint64_t connectionMax)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    ASSERT(connectionMax > 0);

    const String *const host = STRDEF("localhost");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoServer *const tlsServer = tlsServerNew(
           host, cfgOptionStr(cfgOptTlsServerCa), cfgOptionStr(cfgOptTlsServerKey), cfgOptionStr(cfgOptTlsServerCert),
           cfgOptionStrNull(cfgOptTlsServerCrl), cfgOptionUInt64(cfgOptIoTimeout));
        IoServer *const socketServer = sckServerNew(host, cfgOptionUInt(cfgOptTlsServerPort), cfgOptionUInt64(cfgOptIoTimeout));

        // Accept connections until connection max is reached. !!! THIS IS A HACK TO LIMIT THE LOOP AND ALLOW TESTING. IT SHOULD BE
        // REPLACED WITH A STOP REQUEST FROM AN AUTHENTICATED CLIENT OR SIGQUIT.
        do
        {
            // Accept a new connection
            IoSession *const socketSession = ioServerAccept(socketServer, NULL);

            // Fork off the child process
            pid_t pid = forkSafe();

            if (pid == 0)
            {
                // Close the server socket so we don't hold the port open if the parent exits first
                ioServerFree(socketServer);

                // Disable logging and close log file !!! REOPEN LOG FILE HERE TO CATCH ERRORS? MAYBE SOMETHING LIKE
                // logCloseConsole()
                logClose();

                // Detach from parent process
                forkDetach();

                // Start standard remote processing if a server is returned
                ProtocolServer *server = protocolServer(tlsServer, socketSession);

                if (server != NULL)
                    cmdRemote(server);

                break;
            }
            // Wait for first fork to exit
            else
            {
                // The process that was just forked should return immediately
                int processStatus;

                THROW_ON_SYS_ERROR(waitpid(pid, &processStatus, 0) == -1, ExecuteError, "unable to wait for forked process");

                // The first fork should exit with success. If not, something went wrong during the second fork.
                CHECK(WIFEXITED(processStatus) && WEXITSTATUS(processStatus) == 0);
            }

            // Free the socket since the child is now using it
            ioSessionFree(socketSession);
        }
        while (--connectionMax > 0);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
