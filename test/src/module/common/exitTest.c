/***********************************************************************************************************************************
Test Exit Routines
***********************************************************************************************************************************/
#include <sys/wait.h>
#include <unistd.h>

#include "common/error.h"
#include "config/config.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // *****************************************************************************************************************************
    if (testBegin("exitSignalName()"))
    {
        TEST_RESULT_STR(exitSignalName(signalTypeHup), "HUP", "SIGHUP name");
        TEST_RESULT_STR(exitSignalName(signalTypeInt), "INT", "SIGINT name");
        TEST_RESULT_STR(exitSignalName(signalTypeTerm), "TERM", "SIGTERM name");
        TEST_ERROR(exitSignalName(signalTypeNone), AssertError, "no name for signal none");
    }

    // *****************************************************************************************************************************
    if (testBegin("exitInit() and exitOnSignal()"))
    {
        int processId = fork();

        // If this is the fork
        if (processId == 0)
        {
            exitInit();
            raise(SIGTERM);
        }
        else
        {
            int processStatus;

            if (waitpid(processId, &processStatus, 0) != processId)                         // {uncoverable - fork() does not fail}
                THROW_SYS_ERROR(AssertError, "unable to find child process");               // {uncoverable+}

            TEST_RESULT_INT(WEXITSTATUS(processStatus), errorTypeCode(&TermError), "test error result");
        }
    }

    // *****************************************************************************************************************************
    if (testBegin("exitSafe()"))
    {
        cfgInit();
        cfgCommandSet(cfgCmdNone);

        TEST_RESULT_INT(exitSafe(0, false, signalTypeNone), 0, "exit with no command")

        // -------------------------------------------------------------------------------------------------------------------------
        cfgInit();
        cfgCommandSet(cfgCmdArchivePush);

        TEST_RESULT_INT(exitSafe(0, false, signalTypeNone), 0, "exit with no error")
        testLogResult("P00   INFO: archive-push command end: completed successfully");

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            THROW(RuntimeError, "test error message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            testLogResult(
                "P00  ERROR: [122]: test error message\n"
                "P00   INFO: archive-push command end: aborted with exception [122]");
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            THROW(RuntimeError, PERL_EMBED_ERROR);
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            testLogResult(
                "P00   INFO: archive-push command end: aborted with exception [122]");
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(
            exitSafe(errorTypeCode(&TermError), false, signalTypeNone), errorTypeCode(&TermError), "exit on term with no signal");
        testLogResult("P00   INFO: archive-push command end: terminated on signal from child process");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(
            exitSafe(errorTypeCode(&TermError), false, signalTypeTerm), errorTypeCode(&TermError), "exit on term with SIGTERM");
        testLogResult("P00   INFO: archive-push command end: terminated on signal [SIGTERM]");
    }
}
