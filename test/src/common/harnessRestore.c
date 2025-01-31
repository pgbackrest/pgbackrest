/***********************************************************************************************************************************
Harness for Restore Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/restore.h"
#include "common/harnessDebug.h"
#include "common/harnessTest.intern.h"
#include "common/lock.h"
#include "config/config.h"

/**********************************************************************************************************************************/
void
hrnCmdRestore(void)
{
    FUNCTION_HARNESS_VOID();

    lockInit(STR(testPath()), cfgOptionStr(cfgOptExecId), cfgOptionStr(cfgOptStanza), lockTypeRestore);
    lockAcquireP();

    TRY_BEGIN()
    {
        cmdRestore();
    }
    FINALLY()
    {
        lockRelease(true);
    }
    TRY_END();

    FUNCTION_HARNESS_RETURN_VOID();
}
