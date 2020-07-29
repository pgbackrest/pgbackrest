/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include "build.auto.h"

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
#include "version.h"

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

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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
        LOG_ERROR_FMT(
            errorCode(),
            "%s\n"
            "--------------------------------------------------------------------\n"
            "If SUBMITTING AN ISSUE please provide the following information:\n"
            "\n"
            "version: " PROJECT_VERSION "\n"
            "command: %s\n"
            "options:%s\n"
            "\n"
            "stack trace:\n%s\n"
            "--------------------------------------------------------------------",
            errorMessage(), strPtr(cfgCommandRoleName()), strPtr(cmdOption()), errorStackTrace());

        result = errorCode();
    }

    // Free protocol objects
    protocolFree();

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
    }

    // Release any locks but ignore errors
    TRY_BEGIN()
    {
        lockRelease(false);
    }
    TRY_END();

    // Return result - caller should immediate pass this result to exit()
    FUNCTION_LOG_RETURN(INT, result);
}
