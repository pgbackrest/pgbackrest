/***********************************************************************************************************************************
Check Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
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
            const String *walSegment = dbWalSwitch(dbGroup.primary);
            dbFree(dbGroup.primary);

            // Wait for the WAL to appear in the repo
            TimeMSec archiveTimeout = (TimeMSec)(cfgOptionDbl(cfgOptArchiveTimeout) * MSEC_PER_SEC);
            const String *walSegmentFile = walSegmentFind(storageRepo(), archiveId, walSegment, archiveTimeout);

            if (walSegmentFile != NULL)
            {
                LOG_INFO(
                    "WAL segment %s successfully archived to '%s'", strPtr(walSegment),
                    strPtr(
                        storagePath(
                            storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walSegmentFile)))));
            }
            else
            {
                THROW_FMT(
                    ArchiveTimeoutError,
                    "WAL segment %s was not archived before the %" PRIu64 "ms timeout\n"
                        "HINT: Check the archive_command to ensure that all options are correct (especially --stanza).\n"
                        "HINT: Check the PostgreSQL server log for errors.",
                    strPtr(walSegment), archiveTimeout);
            }
        }
        else
            LOG_INFO("switch wal not performed because no primary was found");

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
