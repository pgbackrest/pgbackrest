/***********************************************************************************************************************************
Code Builder
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/config/parse.h"
#include "build/config/render.h"
#include "build/error/parse.h"
#include "build/error/render.h"
#include "build/help/parse.h"
#include "build/help/render.h"
#include "build/postgres/parse.h"
#include "build/postgres/render.h"

int
main(const int argListSize, const char *const argList[])
{
    // Check parameters
    CHECK(ParamInvalidError, argListSize >= 2 && argListSize <= 4, "only one to three parameters allowed");

    // Initialize logging
    logInit(logLevelWarn, logLevelError, logLevelOff, false, 0, 1, false);

    // Get current working directory
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // Get repo path (cwd if it was not passed)
    const String *pathRepo = strPath(STR(currentWorkDir));
    String *pathBuild = strCat(strNew(), pathRepo);

    if (argListSize >= 3)
    {
        const String *const pathArg = STR(argList[2]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathRepo = strPath(pathArg);
        else
            pathRepo = strPathAbsolute(pathArg, STR(currentWorkDir));

        pathBuild = strDup(pathRepo);
    }

    // If the build path was specified
    if (argListSize >= 4)
    {
        const String *const pathArg = STR(argList[3]);

        if (strBeginsWith(pathArg, FSLASH_STR))
            pathBuild = strDup(pathArg);
        else
            pathBuild = strPathAbsolute(pathArg, STR(currentWorkDir));
    }

    // Repo and build storage
    const Storage *const storageRepo = storagePosixNewP(pathRepo);
    const Storage *const storageBuild = storagePosixNewP(pathBuild, .write = true);

    // Config
    if (strEqZ(STRDEF("config"), argList[1]))
        bldCfgRender(storageBuild, bldCfgParse(storageRepo), true);

    // Error
    if (strEqZ(STRDEF("error"), argList[1]))
        bldErrRender(storageRepo, bldErrParse(storageRepo));

    // Help
    if (strEqZ(STRDEF("help"), argList[1]))
    {
        const BldCfg bldCfg = bldCfgParse(storageRepo);
        bldHlpRender(storageBuild, bldCfg, bldHlpParse(storageRepo, bldCfg));
    }

    // PostgreSQL
    if (strEqZ(STRDEF("postgres"), argList[1]))
        bldPgRender(storageBuild, bldPgParse(storageRepo));

    return 0;
}
