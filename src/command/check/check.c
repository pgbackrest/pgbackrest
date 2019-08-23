/***********************************************************************************************************************************
Check Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
#include "command/check/check.h"
#include "command/check/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "db/helper.h"
#include "info/infoArchive.h"
#include "postgres/interface.h"
#include "protocol/helper.h"
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
// CSHANG Need changes from the stanza-to-c conversion to pass storagePg to pgControlFile so we can get the remote DB info.
            // // Get the pgControl information from the pg*-path deemed to be the master
            // result = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath + dbObject.primaryId - 1));
            //
            // // Check the user configured path and version against the database
            // checkDbConfig(result.version,  dbObject.primaryId, dbPgVersion(dbObject.primary), dbPgDataPath(dbObject.primary));
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Get the primary/standby connections (standby is only required if backup from standby is enabled)
        DbGetResult dbGroup = dbGet(false, false);

        // If a standby is defined, check the configuration
        if (dbGroup.standby != NULL)
        {
            // If primary was not found
            if (dbGroup.primary == NULL)
            {
                // If the repo is local or more than one pg-path is found then a master should have been found so error
                if (repoIsLocal() || cfgOptionIndexTotal(cfgOptPgPath) > 1)
                    THROW(ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");
            }
// CSHANG I don't see how storagePg can be used here since we could have a standby and primary configured on the repo with pg1-host and pg2-host
// so calling just storagePg would always get the local host configured (as far as I can tell). What worries me is that we're requiring that pg1-*
// always be the local db host (you mentioned archive-push) so I think we need to make that very clear in the user-guide. Do we test for p2-path being the only thing configured on the primary to confirm archive push still works?
            // Validate the standby database config
            PgControl pgControl = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath + dbGroup.standbyId - 1));

            // Check the user configured path and version against the database
            checkDbConfig(pgControl.version, dbGroup.standbyId, dbPgVersion(dbGroup.standby), dbPgDataPath(dbGroup.standby));

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo(), pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
// CSHANG "switch wal not performed because no primary was found" sounds like something is wrong. Since now adding code to ensure the standby is
// configed properly, do we even need this message? It used to log:
//  &log(INFO, 'switch ' . $oDb->walId() . ' cannot be performed on the standby, all other checks passed successfully');
            LOG_INFO("switch wal not performed because this is a standby");

            // Free the standby connection
            dbFree(dbGroup.standby);
        }

        // If a primary is defined, check the configuration and perform a WAL switch and make sure the WAL is archived
        if (dbGroup.primary != NULL)
        {
            // Validate the primary database config
            PgControl pgControl = pgControlFromFile(storagePg(), cfgOptionStr(cfgOptPgPath + dbGroup.primaryId - 1));

            // Check the user configured path and version against the database
            checkDbConfig(pgControl.version, dbGroup.primaryId, dbPgVersion(dbGroup.primary), dbPgDataPath(dbGroup.primary));

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo(), pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));

            // Attempt to load the archive info file and retrieve the archiveId
            InfoArchive *archiveInfo = infoArchiveNewLoad(
                storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
            const String *archiveId = infoArchiveId(archiveInfo);

            // Perform a WAL switch
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
                        "HINT: check the archive_command to ensure that all options are correct (especially --stanza)\n"
                        "HINT: check the PostgreSQL server log for errors",
                    strPtr(walSegment), archiveTimeout);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
