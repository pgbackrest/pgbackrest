/***********************************************************************************************************************************
Test Fork Handler
***********************************************************************************************************************************/
#include <sys/wait.h>

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("forkAndDetach()"))
    {
        int sessionId = getsid(0);
        int processId = fork();

        // If this is the fork
        if (processId == 0)
        {
            char buffer[1024];

            forkDetach();

            TEST_RESULT_BOOL(getsid(0) != sessionId, true, "new session id has been created");
            TEST_RESULT_STR(getcwd(buffer, sizeof(buffer)), "/", "current working directory is '/'");
            TEST_RESULT_INT(write(STDIN_FILENO, buffer, strlen(buffer)), -1, "write to stdin fails");
            TEST_RESULT_INT(write(STDOUT_FILENO, buffer, strlen(buffer)), -1, "write to stdout fails");
            TEST_RESULT_INT(write(STDERR_FILENO, buffer, strlen(buffer)), -1, "write to stderr fails");

            exit(0);
        }
        else
        {
            int processStatus;

            if (waitpid(processId, &processStatus, 0) != processId)                         // {uncoverable - fork() does not fail}
                THROW_SYS_ERROR(AssertError, "unable to find child process");               // {uncoverable+}

            if (WEXITSTATUS(processStatus) != 0)
                THROW(AssertError, "perl exited with error %d", WEXITSTATUS(processStatus));
        }
    }
}
