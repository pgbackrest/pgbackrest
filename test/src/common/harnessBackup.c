/***********************************************************************************************************************************
Harness for Creating Test Backups
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "command/backup/backup.h"
#include "common/error.h"
#include "common/lock.h"
#include "config/config.h"

#include "common/harnessDebug.h"
#include "common/harnessTest.h"

/**********************************************************************************************************************************/
void
hrnCmdBackup(void)
{
    FUNCTION_HARNESS_VOID();

    lockAcquire(STR(testPath()), cfgOptionStr(cfgOptStanza), cfgOptionStr(cfgOptExecId), lockTypeBackup, 0, true);

    TRY_BEGIN()
    {
        cmdBackup();
    }
    FINALLY()
    {
        lockRelease(true);
    }
    TRY_END();

    FUNCTION_HARNESS_RETURN_VOID();
}
