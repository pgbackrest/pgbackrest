/***********************************************************************************************************************************
Harness for Testing Using Fork

Sometimes it is useful to use a child process for testing.  This can be to test interaction with another process or to avoid
polluting memory in the main process with something that can't easily be undone.

The general form of the fork harness is:

HARNESS_FORK_BEGIN()
{
    // This block is required
    HARNESS_FORK_CHILD()
    {
        // Child test code goes here
    }

    // This block is optional
    HARNESS_FORK_PARENT()
    {
        // Parent test code goes here
    }

    // If the exit result from the child process is expected to be non-zero then that must be set with this optional statement
    HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS_SET(<non-zero exit status>);
}
HARNESS_FORK_END()

If the child process does not explicitly exit in HARNESS_FORK_CHILD() then it will exit with 0 at HARNESS_FORK_END().  This harness
is not intended for long-lived child processes.

There should not be any code outside the HARNESS_FORK_CHILD() and HARNESS_FORK_PARENT() blocks except the
HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS_SET() macro unless the code is intended to run in both the child and parent process which is
rare.
***********************************************************************************************************************************/
#include <sys/wait.h>
#include <unistd.h>

/***********************************************************************************************************************************
HARNESS_FORK_PROCESS_ID()

Return the id of the child process, 0 if in the child process.
***********************************************************************************************************************************/
#define HARNESS_FORK_PROCESS_ID()                                                                                                  \
    HARNESS_FORK_processId

/***********************************************************************************************************************************
HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS()

At the end of the HARNESS_FORK block the parent will wait for the child to exit.  By default an exit code of 0 is expected but that
can be modified with the _SET macro
***********************************************************************************************************************************/
#define HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS()                                                                                  \
    HARNESS_FORK_expectedExitCode

#define HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS_SET(expectedExitStatus)                                                            \
    HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS() = expectedExitStatus;

/***********************************************************************************************************************************
HARNESS_FORK_BEGIN()

Performs the fork and stores the process id.
***********************************************************************************************************************************/
#define HARNESS_FORK_BEGIN()                                                                                                       \
{                                                                                                                                  \
    pid_t HARNESS_FORK_PROCESS_ID() = fork();                                                                                      \
    int HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS() = 0;

/***********************************************************************************************************************************
HARNESS_FORK_CHILD()

Is this the child process?
***********************************************************************************************************************************/
#define HARNESS_FORK_CHILD()                                                                                                       \
    if (HARNESS_FORK_PROCESS_ID() == 0)

/***********************************************************************************************************************************
HARNESS_FORK_PARENT()

Is this the parent process?
***********************************************************************************************************************************/
#define HARNESS_FORK_PARENT()                                                                                                      \
    if (HARNESS_FORK_PROCESS_ID() != 0)

/***********************************************************************************************************************************
HARNESS_FORK_END()

Finish the fork block by waiting for the child to exit.
***********************************************************************************************************************************/
#define HARNESS_FORK_END()                                                                                                         \
    HARNESS_FORK_CHILD()                                                                                                           \
        exit(0);                                                                                                                   \
                                                                                                                                   \
    HARNESS_FORK_PARENT()                                                                                                          \
    {                                                                                                                              \
        int processStatus;                                                                                                         \
                                                                                                                                   \
        if (waitpid(HARNESS_FORK_PROCESS_ID(), &processStatus, 0) != HARNESS_FORK_PROCESS_ID())                                    \
            THROW_SYS_ERROR(AssertError, "unable to find child process");                                                          \
                                                                                                                                   \
        if (WEXITSTATUS(processStatus) != HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS())                                               \
            THROW_FMT(AssertError, "child exited with error %d", WEXITSTATUS(processStatus));                                      \
    }                                                                                                                              \
}
