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
    FUNCTION_LOG_VOID(logLevelDebug);

    // Return the actual number of pg* defined
    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Loop through all defined databases and attempt to build a manifest
        for (unsigned int pgIdx = 0; pgIdx < cfgOptionGroupIdxTotal(cfgOptGrpPg); pgIdx++)
        {
            result++;
            // ??? Placeholder for manifest build
            storageListP(storagePgIdx(pgIdx), cfgOptionIdxStr(cfgOptPgPath, pgIdx));
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
        // If primary was not found (only have 1 pg configured locally, and we want to still run because this is a standby)
        if (dbGroup.primary == NULL)
        {
            // If any repo is local or more than one pg-path is found then a master should have been found so error
            bool error = pgPathDefinedTotal > 1;
            unsigned int repoIdx = 0;

            while (!error && repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo))
            {
                if (repoIsLocal(repoIdx))
                    error = true;

                repoIdx++;
            }

            if (error)
                THROW(ConfigError, "primary database not found\nHINT: check indexed pg-path/pg-host configurations");
        }

        // Validate the standby database config
        PgControl pgControl = pgControlFromFile(storagePgIdx(dbGroup.standbyIdx));

        // Check the user configured path and version against the database
        checkDbConfig(pgControl.version, dbGroup.standbyIdx, dbGroup.standby, true);

        // Check each repository configured
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " repo%u (standby)", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));

            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *storageRepo = storageRepoIdx(repoIdx);

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo, pgControl.version, pgControl.systemId, cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoIdx)),
                cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
        }

        LOG_INFO("switch wal not performed because this is a standby");

        // Free the standby connection
        dbFree(dbGroup.standby);
    }
    // If backup from standby is true then warn when a standby not found
    else if (cfgOptionBool(cfgOptBackupStandby))
    {
        LOG_WARN_FMT("option '%s' is enabled but standby is not properly configured", cfgOptionName(cfgOptBackupStandby));
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
        PgControl pgControl = pgControlFromFile(storagePgIdx(dbGroup.primaryIdx));

        // Check the user configured path and version against the database
        checkDbConfig(pgControl.version, dbGroup.primaryIdx, dbGroup.primary, false);

        // Check configuration of each repo
        const String **repoArchiveId = memNew(sizeof(String *) * cfgOptionGroupIdxTotal(cfgOptGrpRepo));

        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " repo%u configuration (primary)", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));

            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *storageRepo = storageRepoIdx(repoIdx);

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo, pgControl.version, pgControl.systemId, cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoIdx)),
                cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

            // Attempt to load the archive info file and retrieve the archiveId
            InfoArchive *archiveInfo = infoArchiveLoadFile(
                storageRepo, INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionIdxStr(cfgOptRepoCipherType, repoIdx)),
                cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

            repoArchiveId[repoIdx] = infoArchiveId(archiveInfo);
        }

        // Perform a WAL switch
        const String *walSegment = dbWalSwitch(dbGroup.primary);

        // Wait for the WAL to appear in each repo
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " repo%u archive for WAL (primary)", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));

            const Storage *storageRepo = storageRepoIdx(repoIdx);
            const String *walSegmentFile = walSegmentFind(
                storageRepo, repoArchiveId[repoIdx], walSegment, cfgOptionUInt64(cfgOptArchiveTimeout));

            LOG_INFO_FMT(
                "WAL segment %s successfully archived to '%s' on repo%u", strZ(walSegment),
                strZ(storagePathP(storageRepo, strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(repoArchiveId[repoIdx]),
                strZ(walSegmentFile)))), cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));
        }

        dbFree(dbGroup.primary);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdCheck(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the primary/standby connections (standby is only required if backup from standby is enabled)
        DbGetResult dbGroup = dbGet(false, false, false);

        if (dbGroup.standby == NULL && dbGroup.primary == NULL)
            THROW(ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");

        unsigned int pgPathDefinedTotal = checkManifest();
        checkStandby(dbGroup, pgPathDefinedTotal);
        checkPrimary(dbGroup);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
