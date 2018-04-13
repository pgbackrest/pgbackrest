/***********************************************************************************************************************************
Fork Handler
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/error.h"

/***********************************************************************************************************************************
Fork a new process and detach it so it can continue running after the parent process has exited.  This is not a typical daemon
startup because the parent process may continue to run and perform work for some time.
***********************************************************************************************************************************/
void
forkDetach()
{
    if (chdir("/") == -1)                                                                       // {uncoverable - should never fail}
        THROW_SYS_ERROR(PathMissingError, "unable to change directory to '/'");                 // {uncoverable+}

    if (setsid() == -1)                                                                         // {uncoverable - should never fail}
        THROW_SYS_ERROR(AssertError, "unable to create new session group");                     // {uncoverable+}

    if (close(STDIN_FILENO) == -1)                                                              // {uncoverable - should never fail}
        THROW_SYS_ERROR(FileCloseError, "unable to close stdin");                               // {uncoverable+}

    if (close(STDOUT_FILENO) == -1)                                                             // {uncoverable - should never fail}
        THROW_SYS_ERROR(FileCloseError, "unable to close stdout");                              // {uncoverable+}

    if (close(STDERR_FILENO) == -1)                                                             // {uncoverable - should never fail}
        THROW_SYS_ERROR(FileCloseError, "unable to close stderr");                              // {uncoverable+}
}
