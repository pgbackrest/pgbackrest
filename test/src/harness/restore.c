/***********************************************************************************************************************************
Harness for Restore Testing
***********************************************************************************************************************************/
#include <build.h>

#include "command/lock.h"
#include "command/restore/restore.h"
#include "config/config.h"
#include "harness/debug.h"
#include "harness/restore.h"
#include "harness/test.intern.h"

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
