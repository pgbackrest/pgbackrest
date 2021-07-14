/***********************************************************************************************************************************
Harness for Testing Using Fork

Sometimes it is useful to use a child process for testing.  This can be to test interaction with another process or to avoid
polluting memory in the main process with something that can't easily be undone.

The general form of the fork harness is:

// Parameters may be passed, see HrnForkParam.
HRN_FORK_BEGIN()
{
    // This block is required and can be repeated up to HRN_FORK_CHILD_MAX times. Parameters may be passed, see HrnForkChildParam.
    HRN_FORK_CHILD_BEGIN()
    {
        // Child test code goes here
    }
    HRN_FORK_CHILD_END();

    // This block is optional but generally useful. Parameters may be passed, see HrnForkParentParam.
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
Default timeout for IoRead/IoWrite interfaces
***********************************************************************************************************************************/
#define HRN_FORK_TIMEOUT                                            2000

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
Get IoRead/IoWrite interfaces. These are automatically created at fork time and are available via these macros(). Since
IoRead/IoWrite buffer internally using both the interfaces and the file descriptors will be unpredictable unless the IoRead/IoWrite
buffers are known to be empty, e.g. ioWriteFlush() has been called.
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_IO
    #define HRN_FORK_CHILD_READ()                                                                                                  \
        HRN_FORK_ioReadChild

    #define HRN_FORK_CHILD_WRITE()                                                                                                 \
        HRN_FORK_ioWriteChild

    #define HRN_FORK_PARENT_READ(processIdx)                                                                                       \
        HRN_FORK_ioReadParent[processIdx]

    #define HRN_FORK_PARENT_WRITE(processIdx)                                                                                      \
        HRN_FORK_ioWriteParent[processIdx]
#endif

/***********************************************************************************************************************************
Get/put notify messages. These macros allow the parent and child process to synchronize which is useful, e.g. releasing locks.
***********************************************************************************************************************************/
#ifdef HRN_FEATURE_IO
    // General notify get macro used by parent/child
    #define HRN_FORK_NOTIFY_GET(read)                                                                                              \
        ioReadLine(read)

    // General notify put macro used by parent/child
    #define HRN_FORK_NOTIFY_PUT(write)                                                                                             \
        do                                                                                                                         \
        {                                                                                                                          \
            ioWriteStrLine(write, EMPTY_STR);                                                                                      \
            ioWriteFlush(write);                                                                                                   \
        }                                                                                                                          \
        while (0)

    // Put notification to parent from child
    #define HRN_FORK_CHILD_NOTIFY_GET()                                                                                            \
        HRN_FORK_NOTIFY_GET(HRN_FORK_CHILD_READ())

    // Get notification from parent to child
    #define HRN_FORK_CHILD_NOTIFY_PUT()                                                                                            \
        HRN_FORK_NOTIFY_PUT(HRN_FORK_CHILD_WRITE())

    // Put notification to child from parent
    #define HRN_FORK_PARENT_NOTIFY_GET(processIdx)                                                                                 \
        HRN_FORK_NOTIFY_GET(HRN_FORK_PARENT_READ(processIdx))

    // Get notification from child to parent
    #define HRN_FORK_PARENT_NOTIFY_PUT(processIdx)                                                                                 \
        HRN_FORK_NOTIFY_PUT(HRN_FORK_PARENT_WRITE(processIdx))
#endif

/***********************************************************************************************************************************
At the end of the HRN_FORK block the parent will wait for the child to exit.  By default an exit code of 0 is expected but that can
be modified when the child begins.
***********************************************************************************************************************************/
#define HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(processIdx)                                                                            \
    HRN_FORK_expectedExitStatus[processIdx]

/***********************************************************************************************************************************
Begin the fork block
***********************************************************************************************************************************/
typedef struct HrnForkParam
{
    VAR_PARAM_HEADER;

    // Timeout in ms for IoRead/IoWrite interfaces (defaults to HRN_FORK_TIMEOUT). May be overridden in HRN_FORK_CHILD_BEGIN() or
    // HRN_FORK_PARENT_BEGIN().
    uint64_t timeout;
} HrnForkParam;

#define HRN_FORK_BEGIN(...)                                                                                                        \
    do                                                                                                                             \
    {                                                                                                                              \
        HrnForkParam param = {VAR_PARAM_INIT, __VA_ARGS__};                                                                        \
                                                                                                                                   \
        /* Set timeout default */                                                                                                  \
        if (param.timeout == 0)                                                                                                    \
            param.timeout = HRN_FORK_TIMEOUT;                                                                                      \
                                                                                                                                   \
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

    // Expected exit status. Defaults to 0. This does not need to be changed unless the child is expected to exit with a non-zero
    // code for testing purposes.
    int expectedExitStatus;

    // Prefix used to name IoRead/IoWrite interfaces. Defaults to "child" so the default name is "child [idx] read/write".
    const char *prefix;

    // Timeout in ms for IoRead/IoWrite interfaces. Defaults to the value passed in HRN_FORK_BEGIN().
    uint64_t timeout;
} HrnForkChildParam;

// Declare/assign IoRead/IoWrite
#ifdef HRN_FEATURE_IO
    #include "common/io/fdRead.h"
    #include "common/io/fdWrite.h"
    #include "common/type/string.h"

    #define HRN_FORK_CHILD_IO()                                                                                                    \
        IoRead *HRN_FORK_CHILD_READ() = ioFdReadNewOpen(                                                                           \
            strNewFmt("%s %u read", paramChild.prefix, HRN_FORK_PROCESS_IDX()), HRN_FORK_CHILD_READ_FD(), paramChild.timeout);     \
        (void)HRN_FORK_CHILD_READ();                                                                                               \
        IoWrite *HRN_FORK_CHILD_WRITE() = ioFdWriteNewOpen(                                                                        \
            strNewFmt("%s %u write", paramChild.prefix, HRN_FORK_PROCESS_IDX()), HRN_FORK_CHILD_WRITE_FD(), paramChild.timeout);   \
        (void)HRN_FORK_CHILD_WRITE()
#else
    #define HRN_FORK_CHILD_IO()
#endif

#define HRN_FORK_CHILD_BEGIN(...)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        HrnForkChildParam paramChild = {VAR_PARAM_INIT, __VA_ARGS__};                                                              \
                                                                                                                                   \
        /* Set prefix default */                                                                                                   \
        if (paramChild.prefix == NULL)                                                                                             \
            paramChild.prefix = "child";                                                                                           \
                                                                                                                                   \
        /* Set timeout default */                                                                                                  \
        if (paramChild.timeout == 0)                                                                                               \
            paramChild.timeout = param.timeout;                                                                                    \
                                                                                                                                   \
        HRN_FORK_CHILD_EXPECTED_EXIT_STATUS(HRN_FORK_PROCESS_TOTAL()) = paramChild.expectedExitStatus;                             \
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
            /* Declare/assign IoRead/IoWrite */                                                                                    \
            HRN_FORK_CHILD_IO();                                                                                                   \
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
typedef struct HrnForkParentParam
{
    VAR_PARAM_HEADER;

    // Prefix used to name IoRead/IoWrite interfaces. Defaults to "parent" so the default name is "parent [idx] read/write".
    const char *prefix;

    // Timeout in ms for IoRead/IoWrite interfaces. Defaults to the value passed in HRN_FORK_BEGIN().
    uint64_t timeout;
} HrnForkParentParam;

// Declare IoRead/IoWrite
#ifdef HRN_FEATURE_IO
    #define HRN_FORK_PARENT_IO_DECLARE()                                                                                           \
        IoRead *HRN_FORK_PARENT_READ(HRN_FORK_CHILD_MAX) = {0};                                                                    \
        (void)HRN_FORK_PARENT_READ(0);                                                                                             \
        IoWrite *HRN_FORK_PARENT_WRITE(HRN_FORK_CHILD_MAX) = {0};                                                                  \
        (void)HRN_FORK_PARENT_WRITE(0)

    #define HRN_FORK_PARENT_IO_ASSIGN(processIdx)                                                                                  \
        HRN_FORK_PARENT_READ(processIdx) = ioFdReadNewOpen(                                                                        \
            strNewFmt("%s %u read", paramParent.prefix, processIdx), HRN_FORK_PARENT_READ_FD(processIdx), paramParent.timeout);    \
        HRN_FORK_PARENT_WRITE(processIdx) = ioFdWriteNewOpen(                                                                      \
            strNewFmt("%s %u write", paramParent.prefix, processIdx), HRN_FORK_PARENT_WRITE_FD(processIdx), paramParent.timeout)
#else
    #define HRN_FORK_PARENT_IO_DECLARE()
    #define HRN_FORK_PARENT_IO_ASSIGN(processIdx)
#endif

#define HRN_FORK_PARENT_BEGIN(...)                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        HrnForkParentParam paramParent = {VAR_PARAM_INIT, __VA_ARGS__};                                                            \
                                                                                                                                   \
        /* Set prefix default */                                                                                                   \
        if (paramParent.prefix == NULL)                                                                                            \
            paramParent.prefix = "parent";                                                                                         \
                                                                                                                                   \
        /* Set timeout default */                                                                                                  \
        if (paramParent.timeout == 0)                                                                                              \
            paramParent.timeout = param.timeout;                                                                                   \
                                                                                                                                   \
        /* Declare IoRead/IoWrite */                                                                                               \
        HRN_FORK_PARENT_IO_DECLARE();                                                                                              \
                                                                                                                                   \
        for (unsigned int processIdx = 0; processIdx < HRN_FORK_PROCESS_TOTAL(); processIdx++)                                     \
        {                                                                                                                          \
            /* Close child side of pipe */                                                                                         \
            close(HRN_FORK_PIPE(processIdx)[1][0]);                                                                                \
            close(HRN_FORK_PIPE(processIdx)[0][1]);                                                                                \
                                                                                                                                   \
            /* Assign IoRead/IoWrite */                                                                                            \
            HRN_FORK_PARENT_IO_ASSIGN(processIdx);                                                                                 \
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
