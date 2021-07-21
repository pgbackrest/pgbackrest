/***********************************************************************************************************************************
Fork Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"

/**********************************************************************************************************************************/
int
forkSafe(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    int result = fork();

    THROW_ON_SYS_ERROR(result == -1, KernelError, "unable to fork");

    FUNCTION_LOG_RETURN(INT, result);
}

/**********************************************************************************************************************************/
void
forkDetach(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    // Make this process a group leader so the parent process won't block waiting for it to finish
    THROW_ON_SYS_ERROR(setsid() == -1, KernelError, "unable to create new session group");

    // The process should never receive a SIGHUP but ignore it just in case
    signal(SIGHUP, SIG_IGN);

    // There should be no way the child process can exit first (after the next fork) but just in case ignore SIGCHLD.  This means
    // that the child process will automatically be reaped by the kernel should it finish first rather than becoming defunct.
    signal(SIGCHLD, SIG_IGN);

    // Fork again and let the parent process terminate to ensure that we get rid of the session leading process. Only session
    // leaders may get a TTY again.
    if (forkSafe() != 0)
    {
        LOG_DEBUG_FMT("!!!FORK2 IS %d", getpid());
        exit(0);
    }

    // Reset SIGCHLD to the default handler so waitpid() calls in the process will work as expected
    signal(SIGCHLD, SIG_DFL);

    // Change the working directory to / so there is no dependency on the original working directory
    THROW_ON_SYS_ERROR(chdir("/") == -1, PathMissingError, "unable to change directory to '/'");

    // Close standard file descriptors
    THROW_ON_SYS_ERROR(close(STDIN_FILENO) == -1, FileCloseError, "unable to close stdin");
    THROW_ON_SYS_ERROR(close(STDOUT_FILENO) == -1, FileCloseError, "unable to close stdout");
    THROW_ON_SYS_ERROR(close(STDERR_FILENO) == -1, FileCloseError, "unable to close stderr");

    FUNCTION_LOG_RETURN_VOID();
}
