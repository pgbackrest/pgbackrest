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
#include "protocol/helper.h"

#ifdef WITH_PERL
    #include "perl/exec.h"
#endif

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

    FUNCTION_TEST_RETURN(name);
}

/***********************************************************************************************************************************
Catch signals
***********************************************************************************************************************************/
static void
exitOnSignal(int signalType)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, signalType);
    FUNCTION_LOG_END();

    exit(exitSafe(errorTypeCode(&TermError), false, (SignalType)signalType));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Setup signal handlers
***********************************************************************************************************************************/
void
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

/***********************************************************************************************************************************
Do cleanup and return result code
***********************************************************************************************************************************/
int
exitSafe(int result, bool error, SignalType signalType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INT, result);
        FUNCTION_LOG_PARAM(BOOL, error);
        FUNCTION_LOG_PARAM(ENUM, signalType);
    FUNCTION_LOG_END();

    // Report error if one was thrown
    if (error)
    {
        // Don't log the error if it has already been logged by Perl
#ifdef WITH_PERL
        if (strcmp(errorMessage(), PERL_EMBED_ERROR) != 0)
        {
#endif
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
#ifdef WITH_PERL
        }
#endif

        result = errorCode();
    }

    // Release any locks but ignore errors
    TRY_BEGIN()
    {
        lockRelease(false);
    }
    TRY_END();

    // Free protocol objects but ignore errors
    TRY_BEGIN()
    {
        protocolFree();
    }
    TRY_END();

    // Free Perl but ignore errors
#ifdef WITH_PERL
    TRY_BEGIN()
    {
        perlFree(result);
    }
    TRY_END();
#endif

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
    FUNCTION_LOG_RETURN(INT, result);
}
