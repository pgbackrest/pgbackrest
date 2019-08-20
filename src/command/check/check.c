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
// CSHANG Maybe add a check that confirms if backup-standby is set that there IS a standby configured
// CSHANG May need to do something like we do in pgValidate of stanza-create. Basically, both primary and standby can be defined when the repo is remote. Primary and standby never "know" about each other - only the repo server knows about both. The repo can be on the primary, though or it could be on the standby (neither config is ideal but it can happen). If the standby is only a standby, no primary will be set - so maybe part of check will have to confirm that - how? if pg*-path total = 1 and repo not local, it is OK to have standby-only set?
// CSHANG Will the pg-control only be read from the local system? That can't be right since if the repo is remote and the command is run on the repo, we have to get the pg-control from the remote db, no?
            // // Get the pgControl information from the pg*-path deemed to be the master
            // result = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath + dbObject.primaryId - 1));
            //
            // // Check the user configured path and version against the database
            // checkDbConfig(result.version,  dbObject.primaryId, dbPgVersion(dbObject.primary), dbPgDataPath(dbObject.primary));
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
 // CSHANG I'm not a fan of this message because it sounds like something is wrong
            LOG_INFO("switch wal not performed because no primary was found");

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
