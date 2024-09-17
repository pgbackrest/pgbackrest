/***********************************************************************************************************************************
Check Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/archive/find.h"
#include "command/check/check.h"
#include "command/check/common.h"
#include "command/check/report.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "config/load.h"
#include "config/parse.h"
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
checkStandby(const DbGetResult dbGroup, const unsigned int pgPathDefinedTotal)
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
            // If any repo is local or more than one pg-path is found then a primary should have been found so error
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

        // Check the user configured path and version against the database
        checkDbConfig(dbPgControl(dbGroup.standby).version, dbGroup.standbyIdx, dbGroup.standby, true);

        // Check each repository configured
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " %s (standby)", cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *const storageRepo = storageRepoIdx(repoIdx);

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo, dbPgControl(dbGroup.standby).version, dbPgControl(dbGroup.standby).systemId,
                cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
        }

        LOG_INFO("switch wal not performed because this is a standby");

        // Free the standby connection
        dbFree(dbGroup.standby);
    }
    // If backup from standby is true then warn when a standby not found
    else if (cfgOptionStrId(cfgOptBackupStandby) != CFGOPTVAL_BACKUP_STANDBY_N)
    {
        LOG_WARN("option '" CFGOPT_BACKUP_STANDBY "' is enabled but standby is not properly configured");
    }

    FUNCTION_LOG_RETURN_VOID();
}

static void
checkPrimary(const DbGetResult dbGroup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(DB_GET_RESULT, dbGroup);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    // If a primary is defined, check the configuration and perform a WAL switch and make sure the WAL is archived
    if (dbGroup.primary != NULL)
    {
        // Check the user configured path and version against the database
        checkDbConfig(dbPgControl(dbGroup.primary).version, dbGroup.primaryIdx, dbGroup.primary, false);

        // Check configuration of each repo
        const String **const repoArchiveId = memNew(sizeof(String *) * cfgOptionGroupIdxTotal(cfgOptGrpRepo));

        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " %s configuration (primary)", cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            // Get the repo storage in case it is remote and encryption settings need to be pulled down (performed here for testing)
            const Storage *const storageRepo = storageRepoIdx(repoIdx);

            // Check that the backup and archive info files exist and are valid for the current database of the stanza
            checkStanzaInfoPg(
                storageRepo, dbPgControl(dbGroup.primary).version, dbPgControl(dbGroup.primary).systemId,
                cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

            // Attempt to load the archive info file and retrieve the archiveId
            const InfoArchive *const archiveInfo = infoArchiveLoadFile(
                storageRepo, INFO_ARCHIVE_PATH_FILE_STR, cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx),
                cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

            repoArchiveId[repoIdx] = infoArchiveId(archiveInfo);
        }

        // Perform a WAL switch
        const String *const walSegment = dbWalSwitch(dbGroup.primary);

        // Wait for the WAL to appear in each repo
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(CFGCMD_CHECK " %s archive for WAL (primary)", cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

            const String *const walSegmentFile = walSegmentFindOne(
                storageRepoIdx(repoIdx), repoArchiveId[repoIdx], walSegment, cfgOptionUInt64(cfgOptArchiveTimeout));

            LOG_INFO_FMT(
                "WAL segment %s successfully archived to '%s' on %s",
                strZ(walSegment),
                strZ(
                    storagePathP(
                        storageRepoIdx(repoIdx),
                        strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(repoArchiveId[repoIdx]), strZ(walSegmentFile)))),
                cfgOptionGroupName(cfgOptGrpRepo, repoIdx));
        }

        dbFree(dbGroup.primary);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdCheck(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptReport))
            ioFdWriteOneStr(STDOUT_FILENO, checkReport());
        else
        {
            // Build stanza list based on whether a stanza was specified or not
            StringList *stanzaList;
            bool stanzaSpecified = cfgOptionTest(cfgOptStanza);

            if (stanzaSpecified)
            {
                stanzaList = strLstNew();
                strLstAdd(stanzaList, cfgOptionStr(cfgOptStanza));
            }
            else
            {
                stanzaList = cfgParseStanzaList();

                if (strLstSize(stanzaList) == 0)
                {
                    LOG_WARN(
                        "no stanzas found to check\n"
                        "HINT: are there non-empty stanza sections in the configuration?");
                }
            }

            // Iterate stanzas
            for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaList); stanzaIdx++)
            {
                // Switch stanza if required
                if (!stanzaSpecified)
                {
                    const String *const stanza = strLstGet(stanzaList, stanzaIdx);
                    LOG_INFO_FMT("check stanza '%s'", strZ(stanza));

                    // Free storage and protocol cache
                    storageHelperFree();
                    protocolFree();

                    // Reload config with new stanza
                    cfgLoadStanza(stanza);
                }

                // Get the primary/standby connections (standby is only required if backup from standby is enabled)
                DbGetResult dbGroup = dbGet(false, false, CFGOPTVAL_BACKUP_STANDBY_N);

                if (dbGroup.standby == NULL && dbGroup.primary == NULL)
                    THROW(ConfigError, "no database found\nHINT: check indexed pg-path/pg-host configurations");

                const unsigned int pgPathDefinedTotal = checkManifest();
                checkStandby(dbGroup, pgPathDefinedTotal);
                checkPrimary(dbGroup);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
