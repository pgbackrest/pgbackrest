/***********************************************************************************************************************************
Test Fork Handler
***********************************************************************************************************************************/
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("forkSafe() and forkAndDetach()"))
    {
        int sessionId = getsid(0);

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN()
            {
                char buffer[1024];

                forkDetach();

                TEST_RESULT_BOOL(getsid(0) != sessionId, true, "new session id has been created");
                TEST_RESULT_Z(getcwd(buffer, sizeof(buffer)), "/", "current working directory is '/'");
                TEST_RESULT_INT(write(STDIN_FILENO, buffer, strlen(buffer)), -1, "write to stdin fails");
                TEST_RESULT_INT(write(STDOUT_FILENO, buffer, strlen(buffer)), -1, "write to stdout fails");
                TEST_RESULT_INT(write(STDERR_FILENO, buffer, strlen(buffer)), -1, "write to stderr fails");
            }
            HRN_FORK_CHILD_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
