/***********************************************************************************************************************************
Restore Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/restore.h"
#include "common/debug.h"
#include "common/log.h"
// #include "common/memContext.h"
#include "config/config.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Restore a backup
***********************************************************************************************************************************/
void
cmdRestore(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check that PostgreSQL is local
        if (!pgIsLocal(1))
            THROW(HostInvalidError, CFGCMD_RESTORE " command must be run on the " PG_NAME " host");

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
