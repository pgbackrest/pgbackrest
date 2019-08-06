/***********************************************************************************************************************************
Restore Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/restore/restore.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"
#include "postgres/interface.h"
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
        // PostgreSQL must be local
        if (!pgIsLocal(1))
            THROW(HostInvalidError, CFGCMD_RESTORE " command must be run on the " PG_NAME " host");

        // The PGDATA directory must exist
        // ??? We should also do this for the rest of the paths that backrest will not create
        if (!storagePathExistsNP(storagePg(), NULL))
            THROW_FMT(PathMissingError, "$PGDATA directory '%s' does not exist", strPtr(cfgOptionStr(cfgOptPgPath)));

        // PostgreSQL must not be running
        if (storageExistsNP(storagePg(), STRDEF(PG_FILE_POSTMASTERPID)))
        {
            THROW_FMT(
                PostmasterRunningError,
                "unable to restore while PostgreSQL is running\n"
                    "HINT: presence of '" PG_FILE_POSTMASTERPID "' in '%s' indicates PostgreSQL is running.\n"
                    "HINT: remove '" PG_FILE_POSTMASTERPID "' only if PostgreSQL is not running.",
                strPtr(cfgOptionStr(cfgOptPgPath)));
        }

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
