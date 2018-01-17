/***********************************************************************************************************************************
Exit Routines
***********************************************************************************************************************************/
#include "command/command.h"
#include "common/exit.h"
#include "common/log.h"

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
        LOG_ERROR(errorCode(), errorMessage());
        result = errorCode();
    }

    // Log command end
    cmdEnd(result);

    // Return result - caller should immediate pass this result to exit()
    return result;
}
