/***********************************************************************************************************************************
Auto-Generate PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/postgres/parse.h"
#include "build/postgres/render.h"

int
main(int argListSize, const char *argList[])
{
    // Check parameters
    CHECK(ParamInvalidError, argListSize <= 2, "only one parameter allowed");

    // Initialize logging
    logInit(logLevelWarn, logLevelError, logLevelOff, false, 0, 1, false);

    // Get current working directory
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // Get repo path (cwd if it was not passed)
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

    // Render postgres
    const Storage *const storageRepo = storagePosixNewP(pathRepo);
    const Storage *const storageBuild = storagePosixNewP(pathOut, .write = true);
    bldPgRender(storageBuild, bldPgParse(storageRepo));

    return 0;
}
