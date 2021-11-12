/***********************************************************************************************************************************
Test Exit Routines
***********************************************************************************************************************************/
#include "common/error.h"
#include "common/log.h"
#include "config/config.h"
#include "version.h"

#include "common/harnessConfig.h"
#include "common/harnessError.h"
#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("exitSignalName()"))
    {
        TEST_RESULT_Z(exitSignalName(signalTypeHup), "HUP", "SIGHUP name");
        TEST_RESULT_Z(exitSignalName(signalTypeInt), "INT", "SIGINT name");
        TEST_RESULT_Z(exitSignalName(signalTypeTerm), "TERM", "SIGTERM name");
        TEST_ERROR(exitSignalName(signalTypeNone), AssertError, "no name for signal none");
    }

    // *****************************************************************************************************************************
    if (testBegin("exitInit() and exitOnSignal()"))
    {
        HRN_CFG_LOAD(cfgCmdHelp, strLstNew());

        HRN_FORK_BEGIN()
        {
            HRN_FORK_CHILD_BEGIN(.expectedExitStatus = errorTypeCode(&TermError))
            {
                exitInit();
                raise(SIGTERM);
            }
            HRN_FORK_CHILD_END();                               // {uncoverable - signal is raised in block}
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("exitSafe()"))
    {
        HRN_CFG_LOAD(cfgCmdHelp, strLstNew());
        cfgCommandSet(cfgCmdNone, cfgCmdRoleMain);

        TEST_RESULT_INT(exitSafe(0, false, signalTypeNone), 0, "exit with no command");

        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        hrnCfgArgRawZ(argList, cfgOptStanza, "test");
        hrnCfgArgRawNegate(argList, cfgOptLogTimestamp);
        HRN_CFG_LOAD(cfgCmdArchivePush, argList);

        TEST_RESULT_INT(exitSafe(0, false, signalTypeNone), 0, "exit with no error");
        TEST_RESULT_LOG("P00   INFO: archive-push command end: completed successfully");

        TEST_RESULT_INT(exitSafe(1, false, signalTypeNone), 1, "exit with no error");
        TEST_RESULT_LOG("P00   INFO: archive-push command end: completed successfully");

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            THROW(RuntimeError, "test error message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            TEST_RESULT_LOG(
                "P00  ERROR: [122]: test error message\n"
                "P00   INFO: archive-push command end: aborted with exception [122]");
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "--" CFGOPT_STANZA "=test");
        strLstAddZ(argList, "--" CFGOPT_PROCESS_MAX "=4");
        HRN_CFG_LOAD(cfgCmdArchivePush, argList, .role = cfgCmdRoleAsync, .noStd = true);

        harnessLogLevelSet(logLevelDebug);

        TRY_BEGIN()
        {
            hrnErrorThrowP(.errorType = &RuntimeError, .message = "test debug error message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            TEST_RESULT_LOG(
                "P00  DEBUG:     " TEST_PGB_PATH "/src/common/exit::exitSafe: (result: 0, error: true, signalType: 0)\n"
                "P00  ERROR: [122]: test debug error message\n"
                "            --------------------------------------------------------------------\n"
                "            If SUBMITTING AN ISSUE please provide the following information:\n"
                "            \n"
                "            version: " PROJECT_VERSION "\n"
                "            command: archive-push:async\n"
                "            options: --exec-id=1-test --process-max=4 --stanza=test\n"
                "            \n"
                "            stack trace:\n"
                "            ERR_STACK_TRACE\n"
                "            --------------------------------------------------------------------\n"
                "P00   INFO: archive-push:async command end: aborted with exception [122]\n"
                "P00  DEBUG:     " TEST_PGB_PATH "/src/common/lock::lockRelease: (failOnNoLock: false)\n"
                "P00  DEBUG:     " TEST_PGB_PATH "/src/common/lock::lockRelease: => false\n"
                "P00  DEBUG:     " TEST_PGB_PATH "/src/common/exit::exitSafe: => 122");
        }
        TRY_END();

        harnessLogLevelReset();

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            hrnErrorThrowP(.message = "test assert message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            TEST_RESULT_LOG(
                "P00 ASSERT: [025]: test assert message\n"
                "            --------------------------------------------------------------------\n"
                "            If SUBMITTING AN ISSUE please provide the following information:\n"
                "            \n"
                "            version: " PROJECT_VERSION "\n"
                "            command: archive-push:async\n"
                "            options: --exec-id=1-test --process-max=4 --stanza=test\n"
                "            \n"
                "            stack trace:\n"
                "            ERR_STACK_TRACE\n"
                "            --------------------------------------------------------------------\n"
                "P00   INFO: archive-push:async command end: aborted with exception [025]");
        }
        TRY_END();

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(
            exitSafe(errorTypeCode(&TermError), false, signalTypeNone), errorTypeCode(&TermError), "exit on term with no signal");
        TEST_RESULT_LOG("P00   INFO: archive-push:async command end: terminated on signal from child process");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_INT(
            exitSafe(errorTypeCode(&TermError), false, signalTypeTerm), errorTypeCode(&TermError), "exit on term with SIGTERM");
        TEST_RESULT_LOG("P00   INFO: archive-push:async command end: terminated on signal [SIGTERM]");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
