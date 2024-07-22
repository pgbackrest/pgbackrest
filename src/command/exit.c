/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>

#include "command/command.h"
#include "command/exit.h"
#include "command/lock.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.intern.h"
#include "protocol/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Return signal names
***********************************************************************************************************************************/
static const char *
exitSignalName(const SignalType signalType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, signalType);
    FUNCTION_TEST_END();

    const char *name = NULL;

    switch (signalType)
    {
        case signalTypeHup:
            name = "HUP";
            break;

        case signalTypeInt:
            name = "INT";
            break;

        case signalTypeTerm:
            name = "TERM";
            break;

        case signalTypeNone:
            THROW(AssertError, "no name for signal none");
    }

    FUNCTION_TEST_RETURN_CONST(STRINGZ, name);
}

/***********************************************************************************************************************************
Catch signals
***********************************************************************************************************************************/
static void
exitOnSignal(const int signalType)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, signalType);
    FUNCTION_LOG_END();

    exit(exitSafe(errorTypeCode(&TermError), false, (SignalType)signalType));

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
exitInit(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    signal(SIGHUP, exitOnSignal);
    signal(SIGINT, exitOnSignal);
    signal(SIGTERM, exitOnSignal);

    // Ignore SIGPIPE and check for EPIPE errors on write() instead
    signal(SIGPIPE, SIG_IGN);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to provide details for error logging
static String *
exitErrorDetail(void)
{
    FUNCTION_TEST_VOID();

    String *const role = cfgCommandRoleName();
    String *const result = strNewFmt(
        "--------------------------------------------------------------------\n"
        "If SUBMITTING AN ISSUE please provide the following information:\n"
        "\n"
        "version: " PROJECT_VERSION "\n"
        "command: %s\n"
        "options:%s\n"
        "\n"
        "stack trace:\n%s\n"
        "--------------------------------------------------------------------",
        strZ(role), strZ(cmdOption()), errorStackTrace());

    strFree(role);

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN int
exitSafe(int result, const bool error, const SignalType signalType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, result);
        FUNCTION_LOG_PARAM(BOOL, error);
        FUNCTION_LOG_PARAM(ENUM, signalType);
    FUNCTION_LOG_END();

    // Report error if one was thrown
    if (error)
    {
        LogLevel logLevel = errorCode() == errorTypeCode(&AssertError) ? logLevelAssert : logLevelError;

        // Output stack track on assert or when an output (e.g. console) is at least debug level
        if (logLevel == logLevelAssert || logAny(logLevelDebug))
        {
            String *const detail = exitErrorDetail();

            LOG_INTERNAL_FMT(
                logLevel, logLevel == logLevelAssert ? LOG_LEVEL_MIN : logLevelDebug, LOG_LEVEL_MAX, 0, errorCode(), "%s\n%s",
                errorMessage(), strZ(detail));

            strFree(detail);
        }

        // Log just the error to non-debug levels
        if (logLevel != logLevelAssert)
            LOG_INTERNAL(logLevel, LOG_LEVEL_MIN, logLevelDetail, 0, errorCode(), errorMessage());

        result = errorCode();
    }

    // On error generate an error message for command end when config is initialized
    if (cfgInited())
    {
        String *errorMessage = NULL;

        if (result != 0)
        {
            // On process terminate
            if (result == errorTypeCode(&TermError))
            {
                errorMessage = strCatZ(strNew(), "terminated on signal ");

                // Terminate from a child
                if (signalType == signalTypeNone)
                    strCatZ(errorMessage, "from child process");
                // Else terminated directly
                else
                    strCatFmt(errorMessage, "[SIG%s]", exitSignalName(signalType));
            }
            // Standard error exit message
            else if (error)
                errorMessage = strNewFmt("aborted with exception [%03d]", result);
        }

        cmdEnd(result, errorMessage);
        strFree(errorMessage);
    }

    // Release any locks but ignore errors
    TRY_BEGIN()
    {
        cmdLockReleaseP(.returnOnNoLock = true);
    }
    TRY_END();

    // Return result - caller should immediate pass this result to exit()
    FUNCTION_LOG_RETURN(INT, result);
}
