/***********************************************************************************************************************************
Test Exit Routines
***********************************************************************************************************************************/
#include "common/error.h"
#include "common/log.h"
#include "config/config.h"

#include "common/harnessFork.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    FUNCTION_HARNESS_VOID();

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
        HARNESS_FORK_BEGIN()
        {
            HARNESS_FORK_CHILD()
            {
                exitInit();
                raise(SIGTERM);
            }

            HARNESS_FORK_CHILD_EXPECTED_EXIT_STATUS_SET(errorTypeCode(&TermError));
        }
        HARNESS_FORK_END();
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
        logInit(logLevelDebug, logLevelOff, logLevelOff, false);

        TRY_BEGIN()
        {
            THROW(RuntimeError, "test debug error message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            testLogResultRegExp(
                "P00  ERROR\\: \\[122\\]\\: test debug error message\n"
                "            STACK TRACE\\:\n"
                "            module\\/common\\/exitTest\\:testRun\\:.*\n"
                "            test\\:main\\:.*\n");
        }
        TRY_END();

        logInit(logLevelInfo, logLevelOff, logLevelOff, false);

        // -------------------------------------------------------------------------------------------------------------------------
        TRY_BEGIN()
        {
            THROW(AssertError, "test assert message");
        }
        CATCH_ANY()
        {
            exitSafe(0, true, signalTypeNone);
            testLogResultRegExp(
                "P00 ASSERT\\: \\[025\\]\\: test assert message\n"
                "            STACK TRACE\\:\n"
                "            module/common/exitTest\\:testRun\\:.*\n"
                "            test\\:main\\:.*\n");
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

    FUNCTION_HARNESS_RESULT_VOID();
}
