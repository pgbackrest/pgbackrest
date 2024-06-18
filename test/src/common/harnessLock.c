/***********************************************************************************************************************************
Lock Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessDebug.h"
#include "common/harnessLock.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/**********************************************************************************************************************************/
FN_EXTERN void
lockInit(const String *const path, const String *const execId)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, path);
        FUNCTION_HARNESS_PARAM(STRING, execId);
    FUNCTION_HARNESS_END();

    hrnLockUnInit();
    lockInit_SHIMMED(path, execId);

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
hrnLockUnInit(void)
{
    FUNCTION_HARNESS_VOID();

    // Free mem context it it exists
    if (lockLocal.memContext != NULL)
    {
        memContextFree(lockLocal.memContext);
        lockLocal = (struct LockLocal){0};
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
