/***********************************************************************************************************************************
Execute Process
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
    if (testBegin("Exec"))
    {
        Exec *exec = NULL;

        TEST_ASSIGN(exec, execNew(STRDEF("catt"), NULL, STRDEF("cat"), 1000), "invalid exec");
        TEST_RESULT_VOID(execOpen(exec), "open invalid exec");
        TEST_RESULT_VOID(ioWriteStrLine(execIoWrite(exec), EMPTY_STR), "write invalid exec");
        sleep(1);
        TEST_ERROR(
            ioWriteFlush(execIoWrite(exec)), ExecuteError,
            "cat terminated unexpectedly [102]: unable to execute 'catt': [2] No such file or directory");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(STRDEF("cat"), NULL, STRDEF("cat"), 1000), "new cat exec");
        TEST_RESULT_PTR(execMemContext(exec), exec->pub.memContext, "get mem context");
        TEST_RESULT_INT(execFdRead(exec), exec->fdRead, "check read file descriptor");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        const String *message = STRDEF("ACKBYACK");
        TEST_RESULT_VOID(ioWriteStrLine(execIoWrite(exec), message), "write cat exec");
        ioWriteFlush(execIoWrite(exec));
        TEST_RESULT_STR(ioReadLine(execIoRead(exec)), message, "read cat exec");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(STRDEF("cat"), NULL, STRDEF("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");
        close(exec->fdWrite);

        TEST_ERROR(strZ(ioReadLine(execIoRead(exec))), UnknownError, "cat terminated unexpectedly [0]");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(exec, execNew(STRDEF("cat"), NULL, STRDEF("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");
        kill(exec->processId, SIGKILL);

        TEST_ERROR(strZ(ioReadLine(execIoRead(exec))), ExecuteError, "cat terminated unexpectedly on signal 9");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *option = strLstNew();
        strLstAddZ(option, "-b");

        TEST_ASSIGN(exec, execNew(STRDEF("cat"), option, STRDEF("cat"), 1000), "new cat exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        TEST_RESULT_VOID(ioWriteStrLine(execIoWrite(exec), message), "write cat exec");
        ioWriteFlush(execIoWrite(exec));
        TEST_RESULT_STR_Z(ioReadLine(execIoRead(exec)), "     1\tACKBYACK", "read cat exec");
        TEST_RESULT_VOID(execFree(exec), "free exec");

        // Run the same test as above but close all file descriptors first to ensure we don't accidentally close a required
        // descriptor while running dup2()/close() between the fork() and the exec().
        // -------------------------------------------------------------------------------------------------------------------------
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD_BEGIN(0, false)
            {
                // This is not really fd max but for the purposes of testing is fine -- we won't have more than 64 fds open
                for (int fd = 0; fd < 64; fd++)
                    close(fd);

                StringList *option = strLstNew();
                strLstAddZ(option, "-b");

                TEST_ASSIGN(exec, execNew(STRDEF("cat"), option , STRDEF("cat"), 1000), "new cat exec");
                TEST_RESULT_VOID(execOpen(exec), "open cat exec");

                TEST_RESULT_VOID(ioWriteStrLine(execIoWrite(exec), message), "write cat exec");
                ioWriteFlush(execIoWrite(exec));
                TEST_RESULT_STR_Z(ioReadLine(execIoRead(exec)), "     1\tACKBYACK", "read cat exec");
                TEST_RESULT_VOID(execFree(exec), "free exec");
            }
            HARNESS_FORK_CHILD_END();
        }
        HARNESS_FORK_END();

        // -------------------------------------------------------------------------------------------------------------------------
        option = strLstNew();
        strLstAddZ(option, "2");

        TEST_ASSIGN(exec, execNew(STRDEF("sleep"), option, STRDEF("sleep"), 1000), "new sleep exec");
        TEST_RESULT_VOID(execOpen(exec), "open cat exec");

        TEST_ERROR(execFreeResource(exec), ExecuteError, "sleep did not exit when expected");

        TEST_ERROR(ioReadLine(execIoRead(exec)), FileReadError, "unable to read from sleep read: [9] Bad file descriptor");
        ioWriteStrLine(execIoWrite(exec), strNew());
        TEST_ERROR(ioWriteFlush(execIoWrite(exec)), FileWriteError, "unable to write to sleep write: [9] Bad file descriptor");

        sleepMSec(500);
        TEST_RESULT_VOID(execFree(exec), "sleep exited as expected");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
