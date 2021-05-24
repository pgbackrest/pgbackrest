/***********************************************************************************************************************************
CIFS Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/cifs/storage.h"
#include "storage/posix/storage.intern.h"

/**********************************************************************************************************************************/
Storage *
storageCifsNew(
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
        STORAGE, storagePosixNewInternal(STORAGE_CIFS_TYPE, path, modeFile, modePath, write, pathExpressionFunction, false));
}
