/***********************************************************************************************************************************
Auto-Generate Command and Option Configuration Enums, Constants and Data
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/config/parse.h"
#include "build/config/render.h"

int
main(int argListSize, const char *argList[])
{
    // Check parameters
    CHECK(ParamInvalidError, argListSize <= 3, "only two parameters allowed");

    // Initialize logging
    logInit(logLevelWarn, logLevelError, logLevelOff, false, 0, 1, false);

    // Get current working dir
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // If the repo path was specified
    const String *pathRepo = strPath(STR(currentWorkDir));
    String *const pathOut = strCatZ(strNew(), currentWorkDir);

    if (argListSize >= 2)
    {
        const String *const pathArg = STR(argList[1]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathRepo = strPath(pathArg);
        else
        {
            pathRepo = strPathAbsolute(pathArg, STR(currentWorkDir));
            strCatZ(pathOut, "/src");
        }
    }

    // If the build path was specified
    const String *pathBuild = pathRepo;

    if (argListSize >= 3)
    {
        const String *const pathArg = STR(argList[2]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathBuild = strPath(pathArg);
        else
            pathBuild = strPathAbsolute(pathArg, STR(currentWorkDir));
    }

    // Render config
    const Storage *const storageRepo = storagePosixNewP(pathRepo);
    const Storage *const storageBuild = storagePosixNewP(pathBuild, .write = true);
    bldCfgRender(storageBuild, bldCfgParse(storageRepo), true);

    return 0;
}
