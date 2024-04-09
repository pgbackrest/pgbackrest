/***********************************************************************************************************************************
Common Functions and Definitions for Repo Commands
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/repo/common.h"
#include "common/debug.h"
#include "config/config.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN String *
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
            // Get the repo path using repo storage in case it is remotely configured
            const String *const repoPath = storagePathP(storageRepo(), NULL);

            // If the path is exactly equal to the repo path then the relative path is empty
            if (strEq(path, repoPath))
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strNew();
                }
                MEM_CONTEXT_PRIOR_END();
            }
            // Else check that the file path begins with the repo path
            else
            {
                if (!strEq(repoPath, FSLASH_STR) && !strBeginsWith(path, strNewFmt("%s/", strZ(repoPath))))
                {
                    THROW_FMT(
                        ParamInvalidError, "absolute path '%s' is not in base path '%s'", strZ(path),
                        strZ(cfgOptionDisplay(cfgOptRepoPath)));
                }

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    // Get the relative part of the path/file
                    result = strSub(path, strEq(repoPath, FSLASH_STR) ? 1 : strSize(repoPath) + 1);
                }
                MEM_CONTEXT_PRIOR_END();
            }
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
