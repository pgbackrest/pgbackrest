/***********************************************************************************************************************************
CIFS Storage Driver
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/driver/cifs/storage.h"
#include "storage/driver/posix/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant string
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_DRIVER_CIFS_TYPE_STR,                         STORAGE_DRIVER_CIFS_TYPE);

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Storage *
storageDriverCifsNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storageDriverPosixNewInternal(
            STORAGE_DRIVER_CIFS_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, false));
}
