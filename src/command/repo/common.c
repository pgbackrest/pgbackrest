/***********************************************************************************************************************************
Common Functions and Definitions for Repo Commands
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/repo/common.h"
#include "common/debug.h"
#include "config/config.h"

/**********************************************************************************************************************************/
String *
repoPathIsValid(const String *path)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(path != NULL);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Make sure there are no occurrences of //
        if (strstr(strZ(path), "//") != NULL)
            THROW_FMT(ParamInvalidError, "path '%s' cannot contain //", strZ(path));

        // Make sure the path does not end with a slash
        if (strEndsWith(path, FSLASH_STR))
            path = strPath(path);

        // Validate absolute paths
        if (strBeginsWith(path, FSLASH_STR))
        {
            // Check that the file path begins with the repo path
            if (!strBeginsWith(path, cfgOptionStr(cfgOptRepoPath)))
            {
                THROW_FMT(
                    ParamInvalidError, "absolute path '%s' is not in base path '%s'", strZ(path),
                    strZ(cfgOptionDisplay(cfgOptRepoPath)));
            }

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                // Get the relative part of the file
                result = strSub(
                    path, strEq(cfgOptionStr(cfgOptRepoPath), FSLASH_STR) ? 1 : strSize(cfgOptionStr(cfgOptRepoPath)) + 1);
            }
            MEM_CONTEXT_PRIOR_END();
        }
        else
        {
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result = strDup(path);
            }
            MEM_CONTEXT_PRIOR_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
