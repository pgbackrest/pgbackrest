/***********************************************************************************************************************************
Auto-Generate Command and Option Configuration Enums, Constants and Data
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/log.h"
#include "storage/posix/storage.h"

#include "build/config/parse.h"
#include "build/config/render.h"

int
main(int argListSize, const char *argList[])
{
    // Check parameters
    CHECK(argListSize <= 2);

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

    // Render config
    const Storage *const storageRepo = storagePosixNewP(pathRepo, .write = true);
    bldCfgRender(storageRepo, bldCfgParse(storageRepo));

    return 0;
}
