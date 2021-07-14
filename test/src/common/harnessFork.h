/***********************************************************************************************************************************
Harness for Testing Using Fork

Sometimes it is useful to use a child process for testing.  This can be to test interaction with another process or to avoid
polluting memory in the main process with something that can't easily be undone.

The general form of the fork harness is:

HRN_FORK_BEGIN()
{
    // This block is required and can be repeated up to HRN_FORK_CHILD_MAX times.
    //
    // The first parameter is the expected exit code.  If the child block does not have an explicit exit then it will automatically
    // exit on 0.
    //
    // The second parameter specifies whether pipes should be setup between the parent and child processes.  These can be accessed
    // with the HRN_FORK_*() macros;
    HRN_FORK_CHILD_BEGIN()
    {
        // Child test code goes here
    }
    HRN_FORK_CHILD_END();

    // This block is optional but generally useful
    HRN_FORK_PARENT_BEGIN()
    {
        // Parent test code goes here
    }
    HRN_FORK_PARENT_END();
}
HRN_FORK_END();

If the child process does not explicitly exit in HRN_FORK_CHILD_BEGIN/END() then it will exit with 0 at HRN_FORK_END(). This harness
is not intended for long-lived child processes.

There should not be any code outside the HRN_FORK_CHILD_BEGIN/END() and HRN_FORK_PARENT_BEGIN/END() blocks.
***********************************************************************************************************************************/
#include <sys/wait.h>
#include <unistd.h>

#include <common/harnessLog.h>

/***********************************************************************************************************************************
Define the max number of child processes allowed
***********************************************************************************************************************************/
#define HRN_FORK_CHILD_MAX                                          4

/***********************************************************************************************************************************
Total number of child processes forked
***********************************************************************************************************************************/
#define HRN_FORK_PROCESS_TOTAL()                                                                                                   \
    HRN_FORK_processTotal

/***********************************************************************************************************************************
Return the process index of the child (i.e. the index in the total)
***********************************************************************************************************************************/
#define HRN_FORK_PROCESS_IDX()                                                                                                     \
    HRN_FORK_processIdx

/***********************************************************************************************************************************
Return the id of the child process, 0 if in the child process
***********************************************************************************************************************************/
#define HRN_FORK_PROCESS_ID(processIdx)                                                                                            \
    HRN_FORK_processIdList[processIdx]

/***********************************************************************************************************************************
Return the pipe for the child process
***********************************************************************************************************************************/
#define HRN_FORK_PIPE(processIdx)                                                                                                  \
    HRN_FORK_pipe[processIdx]

/***********************************************************************************************************************************
Get read/write pipe file descriptors
***********************************************************************************************************************************/
#define HRN_FORK_CHILD_READ_FD()                                                                                                   \
    HRN_FORK_PIPE(HRN_FORK_PROCESS_IDX())[1][0]

#define HRN_FORK_CHILD_WRITE_FD()                                                                                                  \
    HRN_FORK_PIPE(HRN_FORK_PROCESS_IDX())[0][1]

#define HRN_FORK_PARENT_READ_FD(processIdx)                                                                                        \
    HRN_FORK_PIPE(processIdx)[0][0]

#define HRN_FORK_PARENT_WRITE_FD(processIdx)                                                                                       \
    HRN_FORK_PIPE(processIdx)[1][1]

/***********************************************************************************************************************************
At the end of the HRN_FORK block the parent will wait for the child to exit.  By default an exit code of 0 is expected but that can
be modified when the child begins.
***********************************************************************************************************************************/
#define HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(processIdx)                                                                            \
    HRN_FORK_expectedExitStatus[processIdx]

/***********************************************************************************************************************************
Begin the fork block
***********************************************************************************************************************************/
#define HRN_FORK_BEGIN()                                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        unsigned int HRN_FORK_PROCESS_TOTAL() = 0;                                                                                 \
        pid_t HRN_FORK_PROCESS_ID(HRN_FORK_CHILD_MAX) = {0};                                                                       \
        int HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(HRN_FORK_CHILD_MAX) = {0};                                                         \
        int HRN_FORK_PIPE(HRN_FORK_CHILD_MAX)[2][2] = {{{0}}};

/***********************************************************************************************************************************
Create a child process
***********************************************************************************************************************************/
typedef struct HrnForkChildParam
{
    VAR_PARAM_HEADER;
    int expectedExitStatus;
} HrnForkChildParam;

#define HRN_FORK_CHILD_BEGIN(...)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnForkChildParam param = {VAR_PARAM_INIT, __VA_ARGS__};                                                             \
        HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(HRN_FORK_PROCESS_TOTAL()) = param.expectedExitStatus;                                  \
                                                                                                                                   \
        /* Create pipe for parent/child communication */                                                                           \
        THROW_ON_SYS_ERROR_FMT(                                                                                                    \
            pipe(HRN_FORK_PIPE(HRN_FORK_PROCESS_TOTAL())[0]) == -1, KernelError,                                                   \
            "unable to create read pipe for child process %u", HRN_FORK_PROCESS_TOTAL());                                          \
        THROW_ON_SYS_ERROR_FMT(                                                                                                    \
            pipe(HRN_FORK_PIPE(HRN_FORK_PROCESS_TOTAL())[1]) == -1, KernelError,                                                   \
            "unable to create write pipe for child process %u", HRN_FORK_PROCESS_TOTAL());                                         \
                                                                                                                                   \
        /* Fork child process */                                                                                                   \
        HRN_FORK_PROCESS_ID(HRN_FORK_PROCESS_TOTAL()) = fork();                                                                    \
                                                                                                                                   \
        if (HRN_FORK_PROCESS_ID(HRN_FORK_PROCESS_TOTAL()) == 0)                                                                    \
        {                                                                                                                          \
            unsigned int HRN_FORK_PROCESS_IDX() = HRN_FORK_PROCESS_TOTAL();                                                        \
                                                                                                                                   \
            /* Change log process id to aid in debugging */                                                                        \
            hrnLogProcessIdSet(HRN_FORK_PROCESS_IDX() + 1);                                                                        \
                                                                                                                                   \
            /* Close parent side of pipe */                                                                                        \
            close(HRN_FORK_PARENT_READ_FD(HRN_FORK_PROCESS_IDX()));                                                                \
            close(HRN_FORK_PARENT_WRITE_FD(HRN_FORK_PROCESS_IDX()));                                                               \

#define HRN_FORK_CHILD_END()                                                                                                       \
            /* Close child side of pipe */                                                                                         \
            close(HRN_FORK_CHILD_READ_FD());                                                                                       \
            close(HRN_FORK_CHILD_WRITE_FD());                                                                                      \
                                                                                                                                   \
            exit(0);                                                                                                               \
        }                                                                                                                          \
                                                                                                                                   \
        HRN_FORK_PROCESS_TOTAL()++;                                                                                                \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
Process in the parent
***********************************************************************************************************************************/
#define HRN_FORK_PARENT_BEGIN()                                                                                                    \
    do                                                                                                                             \
    {                                                                                                                              \
        for (unsigned int processIdx = 0; processIdx < HRN_FORK_PROCESS_TOTAL(); processIdx++)                                     \
        {                                                                                                                          \
            /* Close child side of pipe */                                                                                         \
            close(HRN_FORK_PIPE(processIdx)[1][0]);                                                                                \
            close(HRN_FORK_PIPE(processIdx)[0][1]);                                                                                \
        }

#define HRN_FORK_PARENT_END()                                                                                                      \
        for (unsigned int processIdx = 0; processIdx < HRN_FORK_PROCESS_TOTAL(); processIdx++)                                     \
        {                                                                                                                          \
            /* Close parent side of pipe */                                                                                        \
            close(HRN_FORK_PARENT_READ_FD(processIdx));                                                                            \
            close(HRN_FORK_PARENT_WRITE_FD(processIdx));                                                                           \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

/***********************************************************************************************************************************
End the fork block and check exit status for all child processes
***********************************************************************************************************************************/
#define HRN_FORK_END()                                                                                                             \
        for (unsigned int processIdx = 0; processIdx < HRN_FORK_PROCESS_TOTAL(); processIdx++)                                     \
        {                                                                                                                          \
            int processStatus;                                                                                                     \
                                                                                                                                   \
            if (waitpid(HRN_FORK_PROCESS_ID(processIdx), &processStatus, 0) != HRN_FORK_PROCESS_ID(processIdx))                    \
                THROW_SYS_ERROR_FMT(AssertError, "unable to find child process %u", processIdx);                                   \
                                                                                                                                   \
            if (WEXITSTATUS(processStatus) != HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(processIdx))                                     \
            {                                                                                                                      \
                THROW_FMT(                                                                                                         \
                    AssertError, "child %u exited with error %d but expected %d", processIdx, WEXITSTATUS(processStatus),          \
                    HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(processIdx));                                                              \
            }                                                                                                                      \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)
