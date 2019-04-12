/***********************************************************************************************************************************
Common functions and definitions for backup and expire commands
***********************************************************************************************************************************/
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/log.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define DATE_TIME_REGEX                                             "[0-9]{8}\\-[0-9]{6}"

/***********************************************************************************************************************************
backupRegExpGet returns an anchored regex string for filtering backups based on the type (at least one type is required to be true)
***********************************************************************************************************************************/
String *
backupRegExpGet(BackupRegExpGetParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, param.full);
        FUNCTION_LOG_PARAM(BOOL, param.differential);
        FUNCTION_LOG_PARAM(BOOL, param.incremental);
    FUNCTION_LOG_END();

    ASSERT(param.full || param.differential || param.incremental);

    String *result = NULL;

    // Start the expression with the anchor, date/time regexp and full backup indicator
    result = strNew("^" DATE_TIME_REGEX "F");

    // Add the diff and/or incr expressions if requested
    if (param.differential || param.incremental)
    {
        // If full requested then diff/incr is optional
        if (param.full)
        {
            strCat(result, "(\\_");
        }
        // Else diff/incr is required
        else
        {
            strCat(result, "\\_");
        }

        // Append date/time regexp for diff/incr
        strCat(result, DATE_TIME_REGEX);

        // Filter on both diff/incr
        if (param.differential && param.incremental)
        {
            strCat(result, "(D|I)");
        }
        // Else just diff
        else if (param.differential)
        {
            strCatChr(result, 'D');
        }
        // Else just incr
        else
        {
            strCatChr(result, 'I');
        }

        // If full requested then diff/incr is optional
        if (param.full)
        {
            strCat(result, "){0,1}");
        }
    }

    // Append the end anchor
    strCat(result, "$");

    FUNCTION_LOG_RETURN(STRING, result);
}
