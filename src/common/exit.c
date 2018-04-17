/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "command/command.h"
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

    return name;
}

/***********************************************************************************************************************************
Catch signals
***********************************************************************************************************************************/
static void
exitOnSignal(int signalType)
{
    exit(exitSafe(errorTypeCode(&TermError), false, (SignalType)signalType));
}

/***********************************************************************************************************************************
Setup signal handlers
***********************************************************************************************************************************/
void
exitInit()
{
    signal(SIGHUP, exitOnSignal);
    signal(SIGINT, exitOnSignal);
    signal(SIGTERM, exitOnSignal);
}

/***********************************************************************************************************************************
Do cleanup and return result code
***********************************************************************************************************************************/
int
exitSafe(int result, bool error, SignalType signalType)
{
    // Report error if one was thrown
    if (error)
    {
        // Don't log the error if it has already been logged by Perl
        if (strcmp(errorMessage(), PERL_EMBED_ERROR) != 0)
            LOG_ANY(errorCode() == errorTypeCode(&AssertError) ? logLevelAssert : logLevelError, errorCode(), errorMessage());

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
            else
                errorMessage = strNewFmt("aborted with exception [%03d]", result);
        }

        cmdEnd(result, errorMessage);
    }

    // Return result - caller should immediate pass this result to exit()
    return result;
}
