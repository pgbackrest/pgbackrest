/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include "command/command.h"
#include "common/exit.h"
#include "common/log.h"
#include "config/config.h"

/***********************************************************************************************************************************
Do cleanup and return result code
***********************************************************************************************************************************/
int
exitSafe(bool error)
{
    int result = 0;

    // Report error if one was thrown
    if (error)
    {
        LOG_ANY(errorCode() == errorTypeCode(&AssertError) ? logLevelAssert : logLevelError, errorCode(), errorMessage());
        result = errorCode();
    }

    // Log command end if a command is set
    if (cfgCommand() != cfgCmdNone)
        cmdEnd(result);

    // Return result - caller should immediate pass this result to exit()
    return result;
}
