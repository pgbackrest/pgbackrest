/***********************************************************************************************************************************
Check Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/check/check.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoArchive.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Perform standard checks
***********************************************************************************************************************************/
void
cmdCheck(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Attempt to load the archive info file
        InfoArchive *archiveInfo = infoArchiveNewLoad(
            storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        const String *archiveId = infoArchiveId(archiveInfo);

        // Get the primary/standby connections (standby is only required if backup from standby is enabled)
        DbGetResult dbGroup = dbGet(false, false);

        // Free the standby connection immediately since we don't need it for anything
        dbFree(dbGroup.standby);

        // Perform a WAL switch and make sure the WAL is archived if a primary was found
        if (dbGroup.primary != NULL)
        {
            // Perform WAL switch
            dbWalSwitch(dbGroup.primary);
            dbFree(dbGroup.primary);

            // Wait for the WAL to appear in the repo
            (void)archiveId;
            // !!!
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
