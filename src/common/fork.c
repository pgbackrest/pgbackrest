/***********************************************************************************************************************************
Fork Handler
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/debug.h"
#include "common/error.h"
#include "common/log.h"

/***********************************************************************************************************************************
Fork a new process and detach it so it can continue running after the parent process has exited.  This is not a typical daemon
startup because the parent process may continue to run and perform work for some time.
***********************************************************************************************************************************/
void
forkDetach(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    // Change the working directory to / so we won't error if the old working directory goes away
    THROW_ON_SYS_ERROR(chdir("/") == -1, PathMissingError, "unable to change directory to '/'");

    // Make this process a group leader so the parent process won't block waiting for it to finish
    THROW_ON_SYS_ERROR(setsid() == -1, KernelError, "unable to create new session group");

    // Close standard file handles
    THROW_ON_SYS_ERROR(close(STDIN_FILENO) == -1, FileCloseError, "unable to close stdin");
    THROW_ON_SYS_ERROR(close(STDOUT_FILENO) == -1, FileCloseError, "unable to close stdout");
    THROW_ON_SYS_ERROR(close(STDERR_FILENO) == -1, FileCloseError, "unable to close stderr");

    FUNCTION_LOG_RETURN_VOID();
}
