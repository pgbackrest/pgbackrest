/***********************************************************************************************************************************
Auto-Generate PostgreSQL Interface
***********************************************************************************************************************************/
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

    // If the path was specified
    const String *pathRepo;

    if (argListSize >= 2)
    {
        pathRepo = strPath(STR(argList[1]));
    }
    // Else use current working directory
    else
    {
        char currentWorkDir[1024];
        THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

        pathRepo = strPath(STR(currentWorkDir));
    }

    // Render postgres
    const Storage *const storageRepo = storagePosixNewP(pathRepo, .write = true);
    bldPgRender(storageRepo, bldPgParse(storageRepo));

    return 0;
}
