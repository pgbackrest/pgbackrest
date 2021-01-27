/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
#include "command/backup/common.h"
#include "command/control/common.h"
#include "common/time.h"
#include "common/type/list.h"
#include "common/debug.h"
#include "common/regExp.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "protocol/helper.h"
#include "storage/helper.h"

#include <stdlib.h>

/***********************************************************************************************************************************
Helper functions and structures
***********************************************************************************************************************************/
typedef struct ArchiveExpired
{
    uint64_t total;
    String *start;
    String *stop;
} ArchiveExpired;

typedef struct ArchiveRange
{
    const String *start;
    const String *stop;
} ArchiveRange;

/***********************************************************************************************************************************
Given a backup label, expire a backup and all its dependents (if any).
***********************************************************************************************************************************/
static StringList *
expireBackup(InfoBackup *infoBackup, const String *backupLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(backupLabel != NULL);

    // Return a list of all backups being expired
    StringList *result = strLstNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the backup and all its dependents sorted from newest to oldest - that way if the process is aborted before all
        // dependencies have been dealt with, the backups remaining should still be usable.
        StringList *backupList = strLstSort(infoBackupDataDependentList(infoBackup, backupLabel), sortOrderDesc);

        // Expire each backup in the list
        for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
        {
            String *removeBackupLabel = strLstGet(backupList, backupIdx);

            // Execute the real expiration and deletion only if the dry-run option is disabled
            if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
            {
                // Remove the manifest files to invalidate the backup
                storageRemoveP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(removeBackupLabel)));
                storageRemoveP(
                    storageRepoWrite(),
                    strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(removeBackupLabel)));
            }

            // Remove the backup from the info object
            infoBackupDataDelete(infoBackup, removeBackupLabel);
            strLstAdd(result, removeBackupLabel);
        }

        // Sort from oldest to newest and return the list of expired backups
        strLstSort(result, sortOrderAsc);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Function to expire a selected backup (and all its dependents) regardless of retention rules.
***********************************************************************************************************************************/
static unsigned int
expireAdhocBackup(InfoBackup *infoBackup, const String *backupLabel, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(backupLabel != NULL);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the label format is invalid, then error
        if (!regExpMatchOne(backupRegExpP(.full = true, .differential = true, .incremental = true), backupLabel))
        {
            THROW_FMT(OptionInvalidValueError, "'%s' is not a valid backup label format", strZ(backupLabel));
        }

        // If the label is not a current backup then notify user and exit
        if (infoBackupDataByLabel(infoBackup, backupLabel) == NULL)
        {
            LOG_WARN_FMT(
                "backup %s does not exist\nHINT: run the info command and confirm the backup is listed", strZ(backupLabel));
        }
        else
        {
            // Get a list of all full backups with most recent in position 0
            StringList *fullList = strLstSort(infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true)), sortOrderDesc);

            // If the requested backup to expire is the latest full backup
            if (strCmp(strLstGet(fullList, 0), backupLabel) == 0)
            {
                // If the latest full backup requested is the only backup or the prior full backup is not for the same db-id
                // then the backup requested cannot be expired
                if (strLstSize(fullList) == 1 || infoBackupDataByLabel(infoBackup, backupLabel)->backupPgId !=
                    infoBackupDataByLabel(infoBackup, strLstGet(fullList, 1))->backupPgId)
                {
                    THROW_FMT(
                        BackupSetInvalidError,
                        "full backup repo%u: %s cannot be expired until another full backup has been created on this repo",
                        cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(backupLabel));
                }
            }

            // Save off what is currently the latest backup (it may be removed if it is the adhoc backup or is a dependent of the
            // adhoc backup
            const String *latestBackup = infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel;

            // Expire the requested backup and any dependents
            StringList *backupExpired = expireBackup(infoBackup, backupLabel);

            // If the latest backup was removed, then update the latest link if not a dry-run
            if (infoBackupDataByLabel(infoBackup, latestBackup) == NULL)
            {
                // If retention settings have been configured, then there may be holes in the archives. For example, if the archive
                // for db-id=1 has 01,02,03,04,05 and F1 backup has archive start-stop 02-03 and rentention-full=1
                // (hence retention-archive=1 and retention-archive-type=full), then when F2 backup is created and assuming its
                // archive start-stop=05-06 then archives 01 and 04 will be removed resulting in F1 not being able to play through
                // PITR, which is expected. Now adhoc expire is attempted on F2 - it will be allowed but now there will be no
                // backups that can be recovered through PITR until the next full backup is created. Same problem for differential
                // backups with retention-diff.
                LOG_WARN_FMT(
                    "expiring latest backup repo%u: %s - the ability to perform point-in-time-recovery (PITR) may be affected\n"
                    "HINT: non-default settings for '%s'/'%s' (even in prior expires) can cause gaps in the WAL.",
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(latestBackup), cfgOptionName(cfgOptRepoRetentionArchive),
                    cfgOptionName(cfgOptRepoRetentionArchiveType));

                // Adhoc expire is never performed through backup command so only check to determine if dry-run has been set or not
                if (!cfgOptionBool(cfgOptDryRun))
                    backupLinkLatest(infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel);
            }

            result = strLstSize(backupExpired);

            // Log the expired backup list (prepend "set:" if there were any dependents that were also expired)
            LOG_INFO_FMT(
                "expire adhoc backup %srepo%u: %s", (result > 1 ? "set " : ""), cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                strZ(strLstJoin(backupExpired, ", ")));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Expire differential backups
***********************************************************************************************************************************/
static unsigned int
expireDiffBackup(InfoBackup *infoBackup, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Total of all backups being expired
    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ? cfgOptionUInt(cfgOptRepoRetentionDiff) : 0;

        // Find all the expired differential backups
        if (differentialRetention > 0)
        {
            // Get a list of full and differential backups. Full are considered differential for the purpose of retention.
            // Example: F1, D1, D2, F2, repo-retention-diff=2, then F1,D2,F2 will be retained, not D2 and D1 as might be expected.
            StringList *currentBackupList = infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true, .differential = true));

            // If there are more backups than the number to retain, then expire the oldest ones
            if (strLstSize(currentBackupList) > differentialRetention)
            {
                for (unsigned int diffIdx = 0; diffIdx < strLstSize(currentBackupList) - differentialRetention; diffIdx++)
                {
                    // Skip if this is a full backup.  Full backups only count as differential when deciding which differential
                    // backups to expire.
                    if (regExpMatchOne(backupRegExpP(.full = true), strLstGet(currentBackupList, diffIdx)))
                        continue;

                    // Expire the differential and any dependent backups
                    StringList *backupExpired = expireBackup(infoBackup, strLstGet(currentBackupList, diffIdx));
                    result += strLstSize(backupExpired);

                    // Log the expired backups. If there is more than one backup, then prepend "set:"
                    LOG_INFO_FMT(
                        "expire diff backup %srepo%u: %s", (strLstSize(backupExpired) > 1 ? "set " : ""),
                        cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(strLstJoin(backupExpired, ", ")));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Expire full backups
***********************************************************************************************************************************/
static unsigned int
expireFullBackup(InfoBackup *infoBackup, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Total of all backups being expired
    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        unsigned int fullRetention = cfgOptionTest(cfgOptRepoRetentionFull) ? cfgOptionUInt(cfgOptRepoRetentionFull) : 0;

        // Find all the expired full backups
        if (fullRetention > 0)
        {
            // Get list of current full backups (default order is oldest to newest)
            StringList *currentBackupList = infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(currentBackupList) > fullRetention)
            {
                // Expire all backups that depend on the full backup
                for (unsigned int fullIdx = 0; fullIdx < strLstSize(currentBackupList) - fullRetention; fullIdx++)
                {
                    // Expire the full backup and all its dependents
                    StringList *backupExpired = expireBackup(infoBackup, strLstGet(currentBackupList, fullIdx));
                    result += strLstSize(backupExpired);

                    // Log the expired backups. If there is more than one backup, then prepend "set:"
                    LOG_INFO_FMT(
                        "expire full backup %srepo%u: %s", (strLstSize(backupExpired) > 1 ? "set " : ""),
                        cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(strLstJoin(backupExpired, ", ")));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Expire backups based on time
***********************************************************************************************************************************/
static unsigned int
expireTimeBasedBackup(InfoBackup *infoBackup, const time_t minTimestamp, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(TIME, minTimestamp);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();


    ASSERT(infoBackup != NULL);
    ASSERT(minTimestamp > 0);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the list of full backups
        StringList *currentBackupList = strLstSort(infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true)), sortOrderAsc);
        unsigned int backupIdx = strLstSize(currentBackupList);

        // Find out the point where we will have to stop purging backups. Starting with the newest backup (the end of the list),
        // find the first backup that is older than the expire time period by checking the backup stop time. This way, if the
        // backups are F1 D1a D1b D1c F2 D2a D2b F3 D3a D3b and the expiration time period is at D2b, then purge only F1, D1a, D1b
        // and D1c, and keep the next full backups (F2 and F3) and all intermediate non-full backups.
        if (backupIdx > 0)
        {
            const String *lastBackupLabelToKeep = NULL;

            do
            {
                backupIdx--;

                InfoBackupData *info = infoBackupDataByLabel(infoBackup, strLstGet(currentBackupList, backupIdx));
                lastBackupLabelToKeep = info->backupLabel;

                // We can start deleting before this backup. This way, we keep one full backup and its dependents.
                if (info->backupTimestampStop < minTimestamp)
                    break;
            }
            while (backupIdx != 0);

            // Count number of full backups being expired
            unsigned int numFullExpired = 0;

            // Since expireBackup will remove the requested entry from the backup list, we keep checking the first entry which is
            // always the oldest so if it is not the backup to keep then we can remove it
            while (!strEq(infoBackupData(infoBackup, 0).backupLabel, lastBackupLabelToKeep))
            {
                StringList *backupExpired = expireBackup(infoBackup, infoBackupData(infoBackup, 0).backupLabel);

                result += strLstSize(backupExpired);
                numFullExpired++;

                // Log the expired backups. If there is more than one backup, then prepend "set:"
                LOG_INFO_FMT(
                    "expire time-based backup %srepo%u: %s", (strLstSize(backupExpired) > 1 ? "set " : ""),
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(strLstJoin(backupExpired, ", ")));
            }

            if (strEqZ(cfgOptionStr(cfgOptRepoRetentionArchiveType), CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL) &&
                !cfgOptionTest(cfgOptRepoRetentionArchive) && numFullExpired > 0)
            {
                cfgOptionSet(
                    cfgOptRepoRetentionArchive, cfgSourceDefault, varNewUInt(strLstSize(currentBackupList) - numFullExpired));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Log detailed information about archive logs removed
***********************************************************************************************************************************/
static void
logExpire(ArchiveExpired *archiveExpire, String *archiveId, unsigned int repoIdx)
{
    if (archiveExpire->start != NULL)
    {
        // Force out any remaining message
        LOG_DETAIL_FMT(
            "remove archive repo%u: %s, start = %s, stop = %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(archiveId),
            strZ(archiveExpire->start), strZ(archiveExpire->stop));

        archiveExpire->start = NULL;
    }
}

/***********************************************************************************************************************************
Process archive retention
***********************************************************************************************************************************/
static void
removeExpiredArchive(InfoBackup *infoBackup, bool timeBasedFullRetention, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(BOOL, timeBasedFullRetention);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the retention options. repo-archive-retention-type always has a value as it defaults to "full"
        const String *archiveRetentionType = cfgOptionStr(cfgOptRepoRetentionArchiveType);
        unsigned int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ? cfgOptionUInt(cfgOptRepoRetentionArchive) : 0;

        // If archive retention is undefined, then ignore archiving. The user does not have to set this - it will be defaulted in
        // cfgLoadUpdateOption based on certain rules.
        if (archiveRetention == 0)
        {
            String *msg = strNew("- archive logs will not be expired");

            // Only notify user if not time-based retention
            if (!timeBasedFullRetention)
                LOG_INFO_FMT("option '%s' is not set %s", cfgOptionName(cfgOptRepoRetentionArchive), strZ(msg));
            else
            {
                LOG_INFO_FMT(
                    "time-based archive retention not met for repo%u %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                    strZ(msg));
            }
        }
        else
        {
            // Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
            // remaining non-expired backups, from newest to oldest, based on the type.
            StringList *globalBackupRetentionList = NULL;

            if (strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL)) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true)), sortOrderDesc);
            }
            else if (strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF)) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true, .differential = true)), sortOrderDesc);
            }
            else
            {   // Incrementals can depend on Full or Diff so get a list of all incrementals
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true, .differential = true, .incremental = true)),
                    sortOrderDesc);
            }

            // Expire archives. If no backups were found or the number of backups found is not enough to satify archive retention
            // then preserve current archive logs - too soon to expire them.
            if (strLstSize(globalBackupRetentionList) > 0 && archiveRetention <= strLstSize(globalBackupRetentionList))
            {
                // Attempt to load the archive info file
                InfoArchive *infoArchive = infoArchiveLoadFile(
                    storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                    cfgOptionStrNull(cfgOptRepoCipherPass));

                InfoPg *infoArchivePgData = infoArchivePg(infoArchive);

                // Get a list of archive directories (e.g. 9.4-1, 10-2, etc) sorted by the db-id (number after the dash).
                StringList *listArchiveDisk = strLstSort(
                    strLstComparatorSet(
                        storageListP(storageRepo(), STORAGE_REPO_ARCHIVE_STR, .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                        archiveIdComparator),
                    sortOrderAsc);

                StringList *globalBackupArchiveRetentionList = strLstNew();

                // globalBackupRetentionList is ordered newest to oldest backup, so create globalBackupArchiveRetentionList of the
                // newest backups whose archives will be retained
                for (unsigned int idx = 0;
                     idx < (archiveRetention < strLstSize(globalBackupRetentionList) ?
                        archiveRetention : strLstSize(globalBackupRetentionList));
                     idx++)
                {
                    strLstAdd(globalBackupArchiveRetentionList, strLstGet(globalBackupRetentionList, idx));
                }

                // From newest to oldest, confirm the pgVersion and pgSystemId from the archive.info history id match that of the
                // same history id of the backup.info and if not, there is a mismatch between the info files so do not continue.
                // NOTE: If the archive.info file was reconstructed and contains history 4: version 10, sys-id 456 and history 3:
                // version 9.6, sys-id 123, but backup.info contains history 4 and 3 (identical to archive.info) but also 2 and 1
                // then an error will not be thrown since only archive 4 and 3 would have existed and the others expired so
                // nothing really to do.
                for (unsigned int infoPgIdx = 0; infoPgIdx < infoPgDataTotal(infoArchivePgData); infoPgIdx++)
                {
                    InfoPgData archiveInfoPgHistory = infoPgData(infoArchivePgData, infoPgIdx);
                    InfoPgData backupInfoPgHistory = infoPgData(infoBackupPg(infoBackup), infoPgIdx);

                    if (archiveInfoPgHistory.id != backupInfoPgHistory.id ||
                        archiveInfoPgHistory.systemId != backupInfoPgHistory.systemId ||
                        archiveInfoPgHistory.version != backupInfoPgHistory.version)
                    {
                        THROW_FMT(
                            FormatError, "archive expiration cannot continue for repo%u - archive and backup history lists do not"
                            " match", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));
                    }
                }

                // Loop through the archive.info history from oldest to newest and if there is a corresponding directory on disk
                // then remove WAL that are not part of retention as long as the db:history id verion/system-id matches backup.info
                for (unsigned int pgIdx = infoPgDataTotal(infoArchivePgData) - 1; (int)pgIdx >= 0; pgIdx--)
                {
                    String *archiveId = infoPgArchiveId(infoArchivePgData, pgIdx);
                    StringList *localBackupRetentionList = strLstNew();

                    // Initialize the expired archive information for this archive ID
                    ArchiveExpired archiveExpire = {.total = 0, .start = NULL, .stop = NULL};

                    for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(listArchiveDisk); archiveIdx++)
                    {
                        // Is there an archive directory for this archiveId? If not, move on to the next.
                        if (strCmp(archiveId, strLstGet(listArchiveDisk, archiveIdx)) != 0)
                            continue;

                        StringList *archiveSplit = strLstNewSplitZ(archiveId, "-");
                        unsigned int archivePgId = cvtZToUInt(strZ(strLstGet(archiveSplit, 1)));

                        // From the global list of backups to retain, create a list of backups, oldest to newest, associated with
                        // this archiveId (e.g. 9.4-1), e.g. If globalBackupRetention has 4F, 3F, 2F, 1F then
                        // localBackupRetentionList will have 1F, 2F, 3F, 4F (assuming they all have same history id)
                        for (unsigned int retentionIdx = strLstSize(globalBackupRetentionList) - 1;
                             (int)retentionIdx >=0; retentionIdx--)
                        {
                            for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
                            {
                                InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);

                                if (strCmp(backupData.backupLabel, strLstGet(globalBackupRetentionList, retentionIdx)) == 0 &&
                                    backupData.backupPgId == archivePgId)
                                {
                                    strLstAdd(localBackupRetentionList, backupData.backupLabel);
                                }
                            }
                        }

                        // If no backup to retain was found
                        if (strLstSize(localBackupRetentionList) == 0)
                        {
                            // If this is not the current database, then delete the archive directory else do nothing since the
                            // current DB archive directory must not be deleted
                            InfoPgData currentPg = infoPgDataCurrent(infoArchivePgData);

                            if (currentPg.id != archivePgId)
                            {
                                String *fullPath = storagePathP(
                                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveId)));

                                LOG_INFO_FMT(
                                    "remove archive path repo%u: %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                                    strZ(fullPath));

                                // Execute the real expiration and deletion only if the dry-run option is disabled
                                if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
                                    storagePathRemoveP(storageRepoWrite(), fullPath, .recurse = true);
                            }

                            // Continue to next directory
                            break;
                        }

                        // If we get here, then a local backup was found for retention
                        StringList *localBackupArchiveRetentionList = strLstNew();

                        // From the full list of backups in archive retention, find the intersection of local backups to retain
                        // from oldest to newest
                        for (unsigned int globalIdx = strLstSize(globalBackupArchiveRetentionList) - 1;
                             (int)globalIdx >= 0; globalIdx--)
                        {
                            for (unsigned int localIdx = 0; localIdx < strLstSize(localBackupRetentionList); localIdx++)
                            {
                                if (strCmp(
                                        strLstGet(globalBackupArchiveRetentionList, globalIdx),
                                        strLstGet(localBackupRetentionList, localIdx)) == 0)
                                {
                                    strLstAdd(localBackupArchiveRetentionList, strLstGet(localBackupRetentionList, localIdx));
                                }
                            }
                        }

                        // If no local backups were found as part of retention then set the backup archive retention to the newest
                        // backup so that the database is fully recoverable (can be recovered from the last backup through pitr)
                        if (strLstSize(localBackupArchiveRetentionList) == 0)
                        {
                            strLstAdd(
                                localBackupArchiveRetentionList,
                                strLstGet(localBackupRetentionList, strLstSize(localBackupRetentionList) - 1));
                        }

                        // Get the data for the backup selected for retention and all backups associated with this archive id
                        List *archiveIdBackupList = lstNewP(sizeof(InfoBackupData));
                        InfoBackupData archiveRetentionBackup = {0};

                        for (unsigned int infoBackupIdx = 0; infoBackupIdx < infoBackupDataTotal(infoBackup); infoBackupIdx++)
                        {
                            InfoBackupData archiveIdBackup = infoBackupData(infoBackup, infoBackupIdx);

                            // If this is the backup selected for retention, store its data
                            if (strCmp(archiveIdBackup.backupLabel, strLstGet(localBackupArchiveRetentionList, 0)) == 0)
                                archiveRetentionBackup = infoBackupData(infoBackup, infoBackupIdx);

                            // If this is a backup associated with this archive Id, then add it to the list to check
                            if (archiveIdBackup.backupPgId == archivePgId)
                                lstAdd(archiveIdBackupList, &archiveIdBackup);
                        }

                        // Only expire if the selected backup has archive data - backups performed with --no-online will
                        // not have archive data and cannot be used for expiration.
                        bool removeArchive = false;

                        if (archiveRetentionBackup.backupArchiveStart != NULL)
                        {
                            // Get archive ranges to preserve.  Because archive retention can be less than total retention it is
                            // important to preserve archive that is required to make the older backups consistent even though they
                            // cannot be played any further forward with PITR.
                            String *archiveExpireMax = NULL;
                            List *archiveRangeList = lstNewP(sizeof(ArchiveRange));

                            // From the full list of backups, loop through those associated with this archiveId
                            for (unsigned int backupListIdx = 0; backupListIdx < lstSize(archiveIdBackupList); backupListIdx++)
                            {
                                InfoBackupData *backupData = lstGet(archiveIdBackupList, backupListIdx);

                                // If the backup is earlier than or the same as the retention backup and the backup has an
                                // archive start
                                if (strCmp(backupData->backupLabel, archiveRetentionBackup.backupLabel) <= 0 &&
                                    backupData->backupArchiveStart != NULL)
                                {
                                    ArchiveRange archiveRange =
                                    {
                                        .start = strDup(backupData->backupArchiveStart),
                                        .stop = NULL,
                                    };

                                    // If this is not the retention backup, then set the stop, otherwise set the expire max to
                                    // the archive start of the archive to retain
                                    if (strCmp(backupData->backupLabel, archiveRetentionBackup.backupLabel) != 0)
                                        archiveRange.stop = strDup(backupData->backupArchiveStop);
                                    else
                                        archiveExpireMax = strDup(archiveRange.start);

                                    LOG_DETAIL_FMT(
                                        "archive retention on backup %s repo%u: %s, start = %s%s", strZ(backupData->backupLabel),
                                        cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), strZ(archiveId), strZ(archiveRange.start),
                                        archiveRange.stop != NULL ? strZ(strNewFmt(", stop = %s", strZ(archiveRange.stop))) : "");

                                    // Add the archive range to the list
                                    lstAdd(archiveRangeList, &archiveRange);
                                }
                            }

                            // Get all major archive paths (timeline and first 32 bits of LSN)
                            StringList *walPathList =
                                strLstSort(
                                    storageListP(
                                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveId)),
                                        .expression = STRDEF(WAL_SEGMENT_DIR_REGEXP)),
                                    sortOrderAsc);

                            for (unsigned int walIdx = 0; walIdx < strLstSize(walPathList); walIdx++)
                            {
                                String *walPath = strLstGet(walPathList, walIdx);
                                removeArchive = true;

                                // Keep the path if it falls in the range of any backup in retention
                                for (unsigned int rangeIdx = 0; rangeIdx < lstSize(archiveRangeList); rangeIdx++)
                                {
                                    ArchiveRange *archiveRange = lstGet(archiveRangeList, rangeIdx);

                                    if (strCmp(walPath, strSubN(archiveRange->start, 0, 16)) >= 0 &&
                                        (archiveRange->stop == NULL || strCmp(walPath, strSubN(archiveRange->stop, 0, 16)) <= 0))
                                    {
                                        removeArchive = false;
                                        break;
                                    }
                                }

                                // Remove the entire directory if all archive is expired
                                if (removeArchive)
                                {
                                    // Execute the real expiration and deletion only if the dry-run mode is disabled
                                    if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
                                    {
                                        storagePathRemoveP(
                                            storageRepoWrite(),
                                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(walPath)),
                                            .recurse = true);
                                    }

                                    archiveExpire.total++;
                                    archiveExpire.start = strDup(walPath);
                                    archiveExpire.stop = strDup(walPath);
                                }
                                // Else delete individual files instead if the major path is less than or equal to the most recent
                                // retention backup.  This optimization prevents scanning though major paths that could not possibly
                                // have anything to expire.
                                else if (strCmp(walPath, strSubN(archiveExpireMax, 0, 16)) <= 0)
                                {
                                    // Look for files in the archive directory
                                    StringList *walSubPathList =
                                        strLstSort(
                                            storageListP(
                                                storageRepo(),
                                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(walPath)),
                                                .expression = STRDEF("^[0-F]{24}.*$")),
                                            sortOrderAsc);

                                    for (unsigned int subIdx = 0; subIdx < strLstSize(walSubPathList); subIdx++)
                                    {
                                        removeArchive = true;
                                        String *walSubPath = strLstGet(walSubPathList, subIdx);

                                        // Determine if the individual archive log is used in a backup
                                        for (unsigned int rangeIdx = 0; rangeIdx < lstSize(archiveRangeList); rangeIdx++)
                                        {
                                            ArchiveRange *archiveRange = lstGet(archiveRangeList, rangeIdx);

                                            if (strCmp(strSubN(walSubPath, 0, 24), archiveRange->start) >= 0 &&
                                                (archiveRange->stop == NULL ||
                                                 strCmp(strSubN(walSubPath, 0, 24), archiveRange->stop) <= 0))
                                            {
                                                removeArchive = false;
                                                break;
                                            }
                                        }

                                        // Remove archive log if it is not used in a backup
                                        if (removeArchive)
                                        {
                                            // Execute the real expiration and deletion only if the dry-run mode is disabled
                                            if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
                                            {
                                                storageRemoveP(
                                                    storageRepoWrite(),
                                                    strNewFmt(
                                                        STORAGE_REPO_ARCHIVE "/%s/%s/%s", strZ(archiveId), strZ(walPath),
                                                        strZ(walSubPath)));
                                            }

                                            // Track that this archive was removed
                                            archiveExpire.total++;
                                            archiveExpire.stop = strDup(strSubN(walSubPath, 0, 24));
                                            if (archiveExpire.start == NULL)
                                                archiveExpire.start = strDup(strSubN(walSubPath, 0, 24));
                                        }
                                        else
                                            logExpire(&archiveExpire, archiveId, repoIdx);
                                    }
                                }
                            }

                            // Log if no archive was expired
                            if (archiveExpire.total == 0)
                            {
                                LOG_DETAIL_FMT(
                                    "no archive to remove for repo%u: %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                                    strZ(archiveId));
                            }
                            // Log if there is more to log
                            else
                                logExpire(&archiveExpire, archiveId, repoIdx);

                            // Look for history files to expire based on the timeline of backupArchiveStart
                            const String *backupArchiveStartTimeline = strSubN(archiveRetentionBackup.backupArchiveStart, 0, 8);

                            StringList *historyFilesList =
                                strLstSort(
                                    storageListP(
                                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveId)),
                                        .expression = WAL_TIMELINE_HISTORY_REGEXP_STR),
                                    sortOrderAsc);


                            for (unsigned int historyFileIdx = 0; historyFileIdx < strLstSize(historyFilesList); historyFileIdx++)
                            {
                                String *historyFile = strLstGet(historyFilesList, historyFileIdx);

                                // Expire history files older than the oldest retained timeline
                                if (strCmp(strSubN(historyFile, 0, 8), backupArchiveStartTimeline) < 0)
                                {
                                    // Execute the real expiration and deletion only if the dry-run mode is disabled
                                    if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
                                    {
                                        storageRemoveP(
                                            storageRepoWrite(),
                                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(archiveId), strZ(historyFile)));
                                    }

                                    LOG_DETAIL_FMT(
                                        "remove history file repo%u: %s, file = %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                                        strZ(archiveId), strZ(historyFile));
                                }
                            }

                        }
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove expired backups from repo
***********************************************************************************************************************************/
static void
removeExpiredBackup(InfoBackup *infoBackup, const String *adhocBackupLabel, unsigned int repoIdx)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(STRING, adhocBackupLabel);
        FUNCTION_LOG_PARAM(UINT, repoIdx);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Get all the current backups in backup.info - these will not be expired
    StringList *currentBackupList = strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderDesc);

    // Get all the backups on disk
    StringList *backupList = strLstSort(
        storageListP(
            storageRepo(), STORAGE_REPO_BACKUP_STR,
            .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
        sortOrderDesc);

    // Initialize the index to the lastest backup on disk
    unsigned int backupIdx = 0;

    // Only remove the resumable backup if there is a possibility it is a dependent of the adhoc label being expired
    if (adhocBackupLabel != NULL)
    {
        String *manifestFileName = strNewFmt(
            STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(strLstGet(backupList, backupIdx)));
        String *manifestCopyFileName = strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFileName));

        // If the latest backup is resumable (has a backup.manifest.copy but no backup.manifest)
        if (!storageExistsP(storageRepo(), manifestFileName) && storageExistsP(storageRepo(), manifestCopyFileName))
        {
            // If the resumable backup is not related to the expired adhoc backup then don't remove it
            if (!strBeginsWith(strLstGet(backupList, backupIdx), strSubN(adhocBackupLabel, 0, 16)))
            {
                backupIdx = 1;
            }
            // Else it may be related to the adhoc backup so check if its ancestor still exists
            else
            {
                Manifest *manifestResume = manifestLoadFile(
                    storageRepo(), manifestFileName, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                    infoPgCipherPass(infoBackupPg(infoBackup)));

                // If the ancestor of the resumable backup still exists in backup.info then do not remove the resumable backup
                if (infoBackupDataByLabel(infoBackup, manifestData(manifestResume)->backupLabelPrior) != NULL)
                    backupIdx = 1;
            }
        }
    }

    // Remove non-current backups from disk
    for (; backupIdx < strLstSize(backupList); backupIdx++)
    {
        if (!strLstExists(currentBackupList, strLstGet(backupList, backupIdx)))
        {
            LOG_INFO_FMT(
                "remove expired backup repo%u: %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                strZ(strLstGet(backupList, backupIdx)));

            // Execute the real expiration and deletion only if the dry-run mode is disabled
            if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
            {
                storagePathRemoveP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(strLstGet(backupList, backupIdx))),
                    .recurse = true);
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
cmdExpire(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify the repo is local
    repoIsLocalVerify();

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load the backup.info
        InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStrNull(cfgOptRepoCipherPass));

        const String *adhocBackupLabel = NULL;
        bool timeBasedFullRetention = strEqZ(
            cfgOptionStr(cfgOptRepoRetentionFullType), CFGOPTVAL_TMP_REPO_RETENTION_FULL_TYPE_TIME);

        unsigned int repoIdx = cfgOptionGroupIdxDefault(cfgOptGrpRepo);

        // If the --set option is valid (i.e. expire is called on its own) and is set then attempt to expire the requested backup
        if (cfgOptionTest(cfgOptSet))
        {
            adhocBackupLabel = cfgOptionStr(cfgOptSet);
            expireAdhocBackup(infoBackup, adhocBackupLabel, repoIdx);
        }
        else
        {
            // If time-based retention for full backups is set, then expire based on time period
            if (timeBasedFullRetention)
            {
                // If a time period was provided then run time-based expiration otherwise do nothing (the user has already been
                // warned by the config system that retention-full was not set)
                if (cfgOptionTest(cfgOptRepoRetentionFull))
                {
                    expireTimeBasedBackup(
                        infoBackup, time(NULL) - (time_t)(cfgOptionUInt(cfgOptRepoRetentionFull) * SEC_PER_DAY), repoIdx);
                }
            }
            else
                expireFullBackup(infoBackup, repoIdx);

            expireDiffBackup(infoBackup, repoIdx);
        }

        // Store the new backup info only if the dry-run mode is disabled
        if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
        {
            infoBackupSaveFile(
                infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStrNull(cfgOptRepoCipherPass));
        }

        // Remove all files on disk that are now expired
        removeExpiredBackup(infoBackup, adhocBackupLabel, repoIdx);
        removeExpiredArchive(infoBackup, timeBasedFullRetention, repoIdx);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
