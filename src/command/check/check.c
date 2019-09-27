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
Helper functions (to assist with testing)
***********************************************************************************************************************************/
static unsigned int
checkManifest(void)
{
    FUNCTION_LOG_VOID(logLevelTrace);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
// CSHANG The perl code for testing the manifest build doesn't seem right as the outer loop was using the CFGOPT_PG_HOST total. I've changed it to PG_PATH here - any reason why this would not be correct?
        // Loop through all defined databases and attempt to build a manifest
        for (unsigned int pgIdx = 0; pgIdx < cfgOptionIndexTotal(cfgOptPgPath); pgIdx++)
        {
            if (cfgOptionTest(cfgOptPgHost + pgIdx) || cfgOptionTest(cfgOptPgPath + pgIdx))
            {
                result++;
                // CSHANG Placeholder for manifest builds - this is here just to make sure the loop is run and to check error
                // StringList *listTest =
                storageListNP(storagePgId(pgIdx + 1), varStr(cfgOption(cfgOptPgPath + pgIdx)));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

static void
checkStandby(const DbGetResult dbGroup, unsigned int pgPathDefinedTotal)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB_GET_RESULT, dbGroup);
        FUNCTION_LOG_PARAM(UINT, pgPathDefinedTotal);
    FUNCTION_LOG_END();

    // If a standby is defined, check the configuration
    if (dbGroup.standby != NULL)
    {
        // If primary was not found
        if (dbGroup.primary == NULL)
        {
            // If the repo is local or more than one pg-path is found then a master should have been found so error
            if (repoIsLocal() || pgPathDefinedTotal > 1)
                THROW(ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");
        }
// CSHANG What worries me is that we're requiring that pg1-* always be the local db host (you mentioned archive-push) so I think we need to make that very clear in the user-guide. Do we test for p2-path being the only thing configured on the primary to confirm archive push still works?
        // Validate the standby database config
        PgControl pgControl = pgControlFromFile(
            storagePgId(dbGroup.standbyId), cfgOptionStr(cfgOptPgPath + dbGroup.standbyId - 1));

        // Check the user configured path and version against the database
        checkDbConfig(pgControl.version, dbGroup.standbyId, dbGroup.standby, true);

        // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
        storageRepo();

        // Check that the backup and archive info files exist and are valid for the current database of the stanza
        checkStanzaInfoPg(
            storageRepo(), pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        LOG_INFO("switch wal not performed because this is a standby");

        // Free the standby connection
        dbFree(dbGroup.standby);
    }
    // If backup from standby is true then warn when a standby not found
    else if (cfgOptionBool(cfgOptBackupStandby))
    {
        LOG_WARN("option '%s' is enabled but standby is not properly configured", cfgOptionName(cfgOptBackupStandby));
    }

    FUNCTION_LOG_RETURN_VOID();
}

static void
checkPrimary(const DbGetResult dbGroup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB_GET_RESULT, dbGroup);
    FUNCTION_LOG_END();

    // If a primary is defined, check the configuration and perform a WAL switch and make sure the WAL is archived
    if (dbGroup.primary != NULL)
    {
        // Validate the primary database config
        PgControl pgControl = pgControlFromFile(
            storagePgId(dbGroup.primaryId), cfgOptionStr(cfgOptPgPath + dbGroup.primaryId - 1));

        // Check the user configured path and version against the database
        checkDbConfig(pgControl.version, dbGroup.primaryId, dbGroup.primary, false);

        // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
        storageRepo();

        // Check that the backup and archive info files exist and are valid for the current database of the stanza
        checkStanzaInfoPg(
            storageRepo(), pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        // Attempt to load the archive info file and retrieve the archiveId
        InfoArchive *archiveInfo = infoArchiveLoadFile(
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
                strPtr(storagePath(storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walSegmentFile)))));
        }
        else
        {
            THROW_FMT(
                ArchiveTimeoutError,
                "WAL segment %s was not archived before the %" PRIu64 "ms timeout\n"
                    "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
                    "HINT: check the PostgreSQL server log for errors.",
                strPtr(walSegment), archiveTimeout);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Perform standard checks
***********************************************************************************************************************************/
void
cmdCheck(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the primary/standby connections (standby is only required if backup from standby is enabled)
        DbGetResult dbGroup = dbGet(false, false);

        if (dbGroup.standby == NULL && dbGroup.primary == NULL)
            THROW(ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");

        unsigned int pgPathDefinedTotal = checkManifest();
        checkStandby(dbGroup, pgPathDefinedTotal);
        checkPrimary(dbGroup);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
