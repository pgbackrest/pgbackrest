/***********************************************************************************************************************************
Execute Process
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/exec.h"
#include "common/io/handleRead.h"
#include "common/io/handleWrite.h"
#include "common/io/io.h"
#include "common/io/read.intern.h"
#include "common/io/write.intern.h"
#include "common/type/object.h"
#include "common/wait.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Exec
{
    MemContext *memContext;                                         // Mem context
    String *command;                                                // Command to execute
    StringList *param;                                              // List of parameters to pass to command
    const String *name;                                             // Name to display in log/error messages
    TimeMSec timeout;                                               // Timeout for any i/o operation (read, write, etc.)

    pid_t processId;                                                // Process id of the child process

    int handleRead;                                                 // Read handle
    int handleWrite;                                                // Write handle
    int handleError;                                                // Error handle

    IoRead *ioReadHandle;                                           // Handle read interface
    IoWrite *ioWriteHandle;                                         // Handle write interface

    IoRead *ioReadExec;                                             // Wrapper for handle read interface
    IoWrite *ioWriteExec;                                           // Wrapper for handle write interface
};

OBJECT_DEFINE_FREE(EXEC);

/***********************************************************************************************************************************
Macro to close file descriptors after dup2() in the child process

If the parent process is daemomized and has closed stdout, stdin, and stderr or some combination of them, then the newly created
descriptors might overlap stdout, stdin, or stderr.  In that case we don't want to accidentally close the descriptor that we have
just copied.

Note that this is pretty specific to the way that file descriptors are handled in this module and may not be generally applicable in
other code.
***********************************************************************************************************************************/
#define PIPE_DUP2(pipe, pipeIdx, fd)                                                                                               \
    do                                                                                                                             \
    {                                                                                                                              \
        dup2(pipe[pipeIdx], fd);                                                                                                   \
                                                                                                                                   \
        if (pipe[0] != fd)                                                                                                         \
            close(pipe[0]);                                                                                                        \
                                                                                                                                   \
        if (pipe[1] != fd)                                                                                                         \
            close(pipe[1]);                                                                                                        \
    }                                                                                                                              \
    while (0);

/***********************************************************************************************************************************
Free exec handles and ensure process is shut down
***********************************************************************************************************************************/
OBJECT_DEFINE_FREE_RESOURCE_BEGIN(EXEC, LOG, logLevelTrace)
{
    // Close the io handles
    close(this->handleRead);
    close(this->handleWrite);
    close(this->handleError);

    // Wait for the child to exit. We don't really care how it exits as long as it does.
    if (this->processId != 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            int processResult = 0;
            Wait *wait = waitNew(this->timeout);

            do
            {
                THROW_ON_SYS_ERROR(
                    (processResult = waitpid(this->processId, NULL, WNOHANG)) == -1, ExecuteError,
                    "unable to wait on child process");
            }
            while (processResult == 0 && waitMore(wait));

            // If the process did not exit then error -- else we may end up with a collection of zombie processes
            if (processResult == 0)
                THROW_FMT(ExecuteError, "%s did not exit when expected", strZ(this->name));
        }
        MEM_CONTEXT_TEMP_END();
    }
}
OBJECT_DEFINE_FREE_RESOURCE_END(LOG);

/**********************************************************************************************************************************/
Exec *
execNew(const String *command, const StringList *param, const String *name, TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(STRING_LIST, param);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);
    ASSERT(name != NULL);
    ASSERT(timeout > 0);

    Exec *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Exec")
    {
        this = memNew(sizeof(Exec));

        *this = (Exec)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .command = strDup(command),
            .name = strDup(name),
            .timeout = timeout,

            // Parameter list is optional but if not specified we need to build one with the command
            .param = param == NULL ? strLstNew() : strLstDup(param),
        };

        // The first parameter must be the command
        strLstInsert(this->param, 0, this->command);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(EXEC, this);
}

/***********************************************************************************************************************************
Check if the process is still running

This should be called when anything unexpected happens while reading or writing, including errors and eof.  If this function returns
then the original error should be rethrown.
***********************************************************************************************************************************/
static void
execCheck(Exec *this)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(EXEC, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    int processStatus;
    int processResult;

    THROW_ON_SYS_ERROR(
        (processResult = waitpid(this->processId, &processStatus, WNOHANG)) == -1, ExecuteError, "unable to wait on child process");

    if (processResult != 0)
    {
        // Clear the process id so we don't try to wait for this process on free
        this->processId = 0;

        // If the process exited normally
        if (WIFEXITED(processStatus))
        {
            // Get data from stderr to help diagnose the problem
            IoRead *ioReadError = ioHandleReadNew(strNewFmt("%s error", strZ(this->name)), this->handleError, 0);
            ioReadOpen(ioReadError);
            String *errorStr = strTrim(strNewBuf(ioReadBuf(ioReadError)));

            // Throw the error with as much information as is available
            THROWP_FMT(
                errorTypeFromCode(WEXITSTATUS(processStatus)), "%s terminated unexpectedly [%d]%s%s", strZ(this->name),
                WEXITSTATUS(processStatus), strSize(errorStr) > 0 ? ": " : "", strSize(errorStr) > 0 ? strZ(errorStr) : "");
        }

        // If the process did not exit normally then it must have been a signal
        THROW_FMT(ExecuteError, "%s terminated unexpectedly on signal %d", strZ(this->name), WTERMSIG(processStatus));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Read from the process
***********************************************************************************************************************************/
static size_t
execRead(THIS_VOID, Buffer *buffer, bool block)
{
    THIS(Exec);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(EXEC, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
        FUNCTION_LOG_PARAM(BOOL, block);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    size_t result = 0;

    TRY_BEGIN()
    {
        result = ioReadInterface(this->ioReadHandle)->read(ioReadDriver(this->ioReadHandle), buffer, block);
    }
    CATCH_ANY()
    {
        execCheck(this);
        RETHROW();
    }
    TRY_END();

    FUNCTION_LOG_RETURN(SIZE, result);
}

/***********************************************************************************************************************************
Write to the process
***********************************************************************************************************************************/
static void
execWrite(THIS_VOID, const Buffer *buffer)
{
    THIS(Exec);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(EXEC, this);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(buffer != NULL);

    TRY_BEGIN()
    {
        ioWrite(this->ioWriteHandle, buffer);
        ioWriteFlush(this->ioWriteHandle);
    }
    CATCH_ANY()
    {
        execCheck(this);
        RETHROW();
    }
    TRY_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Is the process eof?
***********************************************************************************************************************************/
static bool
execEof(THIS_VOID)
{
    THIS(Exec);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(EXEC, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Check if the process is still running on eof
    if (ioReadInterface(this->ioReadHandle)->eof(ioReadDriver(this->ioReadHandle)))
        execCheck(this);

    FUNCTION_LOG_RETURN(BOOL, false);
}

/***********************************************************************************************************************************
Get the read handle
***********************************************************************************************************************************/
static int
execHandleRead(const THIS_VOID)
{
    THIS(const Exec);

    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(EXEC, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->handleRead);
}

/**********************************************************************************************************************************/
void
execOpen(Exec *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(EXEC, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Create pipes to communicate with the subprocess.  The names of the pipes are from the perspective of the parent process since
    // the child process will use them only briefly before exec'ing.
    int pipeRead[2];
    int pipeWrite[2];
    int pipeError[2];

    THROW_ON_SYS_ERROR(pipe(pipeRead) == -1, KernelError, "unable to create read pipe");
    THROW_ON_SYS_ERROR(pipe(pipeWrite) == -1, KernelError, "unable to create write pipe");
    THROW_ON_SYS_ERROR(pipe(pipeError) == -1, KernelError, "unable to create error pipe");

    // Fork the subprocess
    this->processId = fork();

    // Exec command in the child process
    if (this->processId == 0)
    {
        // Disable logging and close log file
        logClose();

        // Assign stdout to the input side of the read pipe
        PIPE_DUP2(pipeRead, 1, STDOUT_FILENO);

        // Assign stdin to the output side of the write pipe
        PIPE_DUP2(pipeWrite, 0, STDIN_FILENO);

        // Assign stderr to the input side of the error pipe
        PIPE_DUP2(pipeError, 1, STDERR_FILENO);

        // Execute the binary.  This statement will not return if it is successful
        execvp(strZ(this->command), (char ** const)strLstPtr(this->param));

        // If we got here then there was an error.  We can't use a throw as we normally would because we have already shutdown
        // logging and we don't want to execute exit paths that might free parent resources which we still have references to.
        fprintf(stderr, "unable to execute '%s': [%d] %s\n", strZ(this->command), errno, strerror(errno));
        exit(errorTypeCode(&ExecuteError));
    }

    // Close the unused handles
    close(pipeRead[1]);
    close(pipeWrite[0]);
    close(pipeError[1]);

    // Store the handles we'll use and need to close when the process terminates
    this->handleRead = pipeRead[0];
    this->handleWrite = pipeWrite[1];
    this->handleError = pipeError[0];

    // Assign handles to io interfaces
    this->ioReadHandle = ioHandleReadNew(strNewFmt("%s read", strZ(this->name)), this->handleRead, this->timeout);
    this->ioWriteHandle = ioHandleWriteNew(strNewFmt("%s write", strZ(this->name)), this->handleWrite);
    ioWriteOpen(this->ioWriteHandle);

    // Create wrapper interfaces that check process state
    this->ioReadExec = ioReadNewP(this, .block = true, .read = execRead, .eof = execEof, .handle = execHandleRead);
    ioReadOpen(this->ioReadExec);
    this->ioWriteExec = ioWriteNewP(this, .write = execWrite);
    ioWriteOpen(this->ioWriteExec);

    // Set a callback so the handles will get freed
    memContextCallbackSet(this->memContext, execFreeResource, this);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
IoRead *
execIoRead(const Exec *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(EXEC, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->ioReadExec);
}

/**********************************************************************************************************************************/
IoWrite *
execIoWrite(const Exec *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(EXEC, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->ioWriteExec);
}

/**********************************************************************************************************************************/
MemContext *
execMemContext(const Exec *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(EXEC, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->memContext);
}
