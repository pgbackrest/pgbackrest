/***********************************************************************************************************************************
Auto-Generate Help
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/config/parse.h"
#include "build/help/parse.h"
#include "build/help/render.h"

int
main(int argListSize, const char *argList[])
{
    // Check parameters
    CHECK(ParamInvalidError, argListSize <= 3, "only two parameters allowed");

    // Initialize logging
    logInit(logLevelWarn, logLevelError, logLevelOff, false, 0, 1, false);

    // Get current working directory
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // Get repo path (cwd if it was not passed)
    const String *pathRepo = strPath(STR(currentWorkDir));
    String *pathBuild = strCatZ(strNew(), currentWorkDir);

    if (argListSize >= 2)
    {
        const String *const pathArg = STR(argList[1]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathRepo = strPath(pathArg);
        else
        {
            pathRepo = strPathAbsolute(pathArg, STR(currentWorkDir));
            strCatZ(pathBuild, "/src");
        }
    }

    // If the build path was specified
    if (argListSize >= 3)
    {
        const String *const pathArg = STR(argList[2]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathBuild = strPath(pathArg);
        else
            pathBuild = strPathAbsolute(pathArg, STR(currentWorkDir));
    }

    // Render help
    const Storage *const storageRepo = storagePosixNewP(pathRepo);
    const Storage *const storageBuild = storagePosixNewP(pathBuild, .write = true);
    const BldCfg bldCfg = bldCfgParse(storageRepo);
    bldHlpRender(storageBuild, bldCfg, bldHlpParse(storageRepo, bldCfg));

    return 0;
}
