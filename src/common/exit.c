/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "command/command.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/exit.h"
#include "common/lock.h"
#include "common/log.h"
#include "config/config.h"
#include "perl/exec.h"

/***********************************************************************************************************************************
Return signal names
***********************************************************************************************************************************/
static const char *
exitSignalName(int signalType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INT, signalType);
    FUNCTION_TEST_END();

    const char *name = NULL;

    switch (signalType)
    {
        case signalTypeHup:
        {
            name = "HUP";
            break;
        }

        case signalTypeInt:
        {
            name = "INT";
            break;
        }

        case signalTypeTerm:
        {
            name = "TERM";
            break;
        }

        case signalTypeNone:
            THROW(AssertError, "no name for signal none");
    }

    FUNCTION_TEST_RESULT(STRINGZ, name);
}

/***********************************************************************************************************************************
Catch signals
***********************************************************************************************************************************/
static void
exitOnSignal(int signalType)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, signalType);
    FUNCTION_DEBUG_END();

    exit(exitSafe(errorTypeCode(&TermError), false, (SignalType)signalType));

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Setup signal handlers
***********************************************************************************************************************************/
void
exitInit(void)
{
    FUNCTION_DEBUG_VOID(logLevelTrace);

    signal(SIGHUP, exitOnSignal);
    signal(SIGINT, exitOnSignal);
    signal(SIGTERM, exitOnSignal);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Do cleanup and return result code
***********************************************************************************************************************************/
int
exitSafe(int result, bool error, SignalType signalType)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(INT, result);
        FUNCTION_DEBUG_PARAM(BOOL, error);
        FUNCTION_DEBUG_PARAM(ENUM, signalType);
    FUNCTION_DEBUG_END();

    // Report error if one was thrown
    if (error)
    {
        // Don't log the error if it has already been logged by Perl
        if (strcmp(errorMessage(), PERL_EMBED_ERROR) != 0)
        {
            LogLevel logLevel = errorCode() == errorTypeCode(&AssertError) ? logLevelAssert : logLevelError;

            // Assert errors always output a stack trace
            if (logLevel == logLevelAssert)
                LOG(logLevel, errorCode(), "%s\nSTACK TRACE:\n%s", errorMessage(), errorStackTrace());
            else
            {
                // Log just the error to non-debug levels
                LOG_INTERNAL(logLevel, LOG_LEVEL_MIN, logLevelDetail, errorCode(), errorMessage());

                // Log the stack trace debug levels
                if (logWill(logLevelDebug))
                {
                    LOG_INTERNAL(
                        logLevel, logLevelDebug, LOG_LEVEL_MAX, errorCode(), "%s\nSTACK TRACE:\n%s", errorMessage(),
                        errorStackTrace());
                }
            }
        }

        result = errorCode();
    }

    // Release any locks but ignore errors
    TRY_BEGIN()
    {
        lockRelease(false);
    }
    TRY_END();

    // Free Perl but ignore errors
    TRY_BEGIN()
    {
        perlFree(result);
    }
    TRY_END();

    // Log command end if a command is set
    if (cfgCommand() != cfgCmdNone)
    {
        String *errorMessage = NULL;

        // On error generate an error message
        if (result != 0)
        {
            // On process terminate
            if (result == errorTypeCode(&TermError))
            {
                errorMessage = strNew("terminated on signal ");

                // Terminate from a child
                if (signalType == signalTypeNone)
                    strCat(errorMessage, "from child process");
                // Else terminated directly
                else
                    strCatFmt(errorMessage, "[SIG%s]", exitSignalName(signalType));
            }
            // Standard error exit message
            else if (error)
                errorMessage = strNewFmt("aborted with exception [%03d]", result);
        }

        cmdEnd(result, errorMessage);
    }

    // Return result - caller should immediate pass this result to exit()
    FUNCTION_DEBUG_RESULT(INT, result);
}
