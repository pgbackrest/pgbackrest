/***********************************************************************************************************************************
Auto-Generate Help
***********************************************************************************************************************************/
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
    CHECK(ParamInvalidError, argListSize <= 2, "only one parameter allowed");

    // Initialize logging
    logInit(logLevelWarn, logLevelError, logLevelOff, false, 0, 1, false);

    // Get current working directory
    char currentWorkDir[1024];
    THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

    // Get repo path (cwd if it was not passed)
    const String *pathRepo = argListSize >= 2 ? strPath(STR(argList[1])) : strPath(STR(currentWorkDir));

    // Render config
    const Storage *const storageRepo = storagePosixNewP(pathRepo);
    const Storage *const storageBuild = storagePosixNewP(STR(currentWorkDir), .write = true);
    const BldCfg bldCfg = bldCfgParse(storageRepo);
    bldHlpRender(storageBuild, bldCfg, bldHlpParse(storageRepo, bldCfg));

    return 0;
}
