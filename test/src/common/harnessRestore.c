/***********************************************************************************************************************************
Harness for Restore Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/lock.h"
#include "command/restore/restore.h"
#include "common/harnessDebug.h"
#include "common/harnessRestore.h"
#include "common/harnessTest.intern.h"
#include "config/config.h"

/**********************************************************************************************************************************/
void
hrnCmdRestore(void)
{
    FUNCTION_HARNESS_VOID();

    lockInit(STR(testPath()), cfgOptionStr(cfgOptExecId));
    cmdLockAcquireP();

    TRY_BEGIN()
    {
        cmdRestore();
    }
    FINALLY()
    {
        cmdLockReleaseP();
    }
    TRY_END();

    FUNCTION_HARNESS_RETURN_VOID();
}
