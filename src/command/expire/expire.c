/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
#include "command/backup/common.h"
#include "common/type/list.h"
#include "common/debug.h"
#include "common/regExp.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
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

static int
archiveIdComparator(const void *item1, const void *item2)
{
    StringList *archiveSort1 = strLstNewSplitZ(*(String **)item1, "-");
    StringList *archiveSort2 = strLstNewSplitZ(*(String **)item2, "-");
    int int1 = atoi(strPtr(strLstGet(archiveSort1, 1)));
    int int2 = atoi(strPtr(strLstGet(archiveSort2, 1)));

    return (int1 - int2);
}

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
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(removeBackupLabel)));
                storageRemoveP(
                    storageRepoWrite(),
                    strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strPtr(removeBackupLabel)));
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
Expire backups based on dates
***********************************************************************************************************************************/
static unsigned int
expireTimeBasedFullBackup(InfoBackup *infoBackup, const unsigned int backupRetentionDays)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(UINT, backupRetentionDays);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(backupRetentionDays > 0);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the min date to keep and the list of full backups
        TimeMSec minDateToKeepMSec = timeMSec();
        time_t minDateToKeepSec = (time_t)(minDateToKeepMSec / MSEC_PER_SEC - backupRetentionDays * 24 * 3600);
        const String *lastBackupLabelToKeep = NULL;
        StringList *fullBackupList = infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true));
        unsigned int i = strLstSize(fullBackupList);

        /*
         * Find out the point where we will have to stop purging backups.
         * We start by the end of the list, skipping any non-full backup.
         * This way, if we have F1 D1a D1b D1c F2 D2a D2b F3 D3a D3b and the expiration at D2b,
         * we will purge only F1, D1a, D1b and D1c, keeping the next full backups and all intermediate non-full
         */
        if (i > 0)
        {
            do
            {
                i--;
                InfoBackupData *info = infoBackupDataByLabel(infoBackup, strLstGet(fullBackupList, i));
                lastBackupLabelToKeep = info->backupLabel;
                if (info->backupTimestampStart < minDateToKeepSec)
                {
                    // We can start deleting after this backup. This way, we keep one full backup and its dependents
                    break;
                }
            } while (i != 0);

            LOG_INFO_FMT("deleting all backups older than %s", strPtr(lastBackupLabelToKeep));

            // Since expireBackup will remove the requested entry from the backup list, we keep checking the first entry
            while (strCmp(infoBackupData(infoBackup, 0).backupLabel, lastBackupLabelToKeep) != 0)
            {
                StringList *backupExpired = expireBackup(infoBackup, infoBackupData(infoBackup, 0).backupLabel);
                result += strLstSize(backupExpired);

                // Log the expired backups. If there is more than one backup, then prepend "set:"
                LOG_INFO_FMT(
                    "expire backup %s%s", (strLstSize(backupExpired) > 1 ? "set: " : ""), strPtr(strLstJoin(backupExpired, ", ")));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}


/***********************************************************************************************************************************
Expire differential backups
***********************************************************************************************************************************/
static unsigned int
expireDiffBackup(InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
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
                        "expire diff backup %s%s", (strLstSize(backupExpired) > 1 ? "set: " : ""),
                        strPtr(strLstJoin(backupExpired, ", ")));
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
expireFullBackup(InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
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
                        "expire full backup %s%s", (strLstSize(backupExpired) > 1 ? "set: " : ""),
                        strPtr(strLstJoin(backupExpired, ", ")));
                }
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
logExpire(ArchiveExpired *archiveExpire, String *archiveId)
{
    if (archiveExpire->start != NULL)
    {
        // Force out any remaining message
        LOG_DETAIL_FMT(
            "remove archive: archiveId = %s, start = %s, stop = %s", strPtr(archiveId), strPtr(archiveExpire->start),
            strPtr(archiveExpire->stop));

        archiveExpire->start = NULL;
    }
}

/***********************************************************************************************************************************
Process archive retention
***********************************************************************************************************************************/
static void
removeExpiredArchive(InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the retention options. repo-archive-retention-type always has a value as it defaults to "full"
        const String *archiveRetentionType = cfgOptionStr(cfgOptRepoRetentionArchiveType);
        unsigned int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ? cfgOptionUInt(cfgOptRepoRetentionArchive) : 0;
        bool timeBasedFullRetention = cfgOptionTest(cfgOptRepoRetentionFullPeriod);

        // If retention-full-period is set and valid and archive retention was not explicitly set then set it greater than the
        // number of full backups remaining so that all archive prior to the oldest full backup is expired
        if (timeBasedFullRetention && archiveRetention == 0)
            archiveRetention = strLstSize(infoBackupDataLabelList(infoBackup, backupRegExpP(.full = true))) + 1;

        // If archive retention is undefined, then ignore archiving. The user does not have to set this - it will be defaulted in
        // cfgLoadUpdateOption based on certain rules.
        if (archiveRetention == 0)
        {
             LOG_INFO_FMT("option '%s' is not set - archive logs will not be expired", cfgOptionName(cfgOptRepoRetentionArchive));
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

            // Expire archives. If no backups were found then preserve current archive logs - too soon to expire them.
            if (strLstSize(globalBackupRetentionList) > 0)
            {
                // Attempt to load the archive info file
                InfoArchive *infoArchive = infoArchiveLoadFile(
                    storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                    cfgOptionStr(cfgOptRepoCipherPass));

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
                        THROW(FormatError, "archive expiration cannot continue - archive and backup history lists do not match");
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
                        unsigned int archivePgId = cvtZToUInt(strPtr(strLstGet(archiveSplit, 1)));

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
                                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId)));

                                LOG_INFO_FMT("remove archive path: %s", strPtr(fullPath));

                                // Execute the real expiration and deletion only if the dry-run option is disabled
                                if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
                                    storagePathRemoveP(storageRepoWrite(), fullPath, .recurse = true);
                            }

                            // Continue to next directory
                            break;
                        }

                        // If we get here, then a local backup was found for retention
                        StringList *localBackupArchiveRetentionList = strLstNew();

                        // If the archive retention is less than or equal to the number of all backups, then perform selective
                        // expiration
                        if (archiveRetention <= strLstSize(globalBackupRetentionList))
                        {
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
                        }
                        // Else if there are not enough backups yet globally to start archive expiration then set the archive
                        // retention to the oldest backup so anything prior to that will be removed as it is not needed but
                        // everything else is. This is incase there are old archives left around so that they don't stay around
                        // forever.
                        else
                        {
                            // Don't display log message if using time-based retention
                            if (!timeBasedFullRetention)
                            {
                                LOG_INFO_FMT(
                                    "full backup total < %u - using oldest full backup for %s archive retention", archiveRetention,
                                    strPtr(archiveId));
                            }
                            strLstAdd(localBackupArchiveRetentionList, strLstGet(localBackupRetentionList, 0));
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
                        List *archiveIdBackupList = lstNew(sizeof(InfoBackupData));
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
                            List *archiveRangeList = lstNew(sizeof(ArchiveRange));

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
                                        "archive retention on backup %s, archiveId = %s, start = %s%s",
                                        strPtr(backupData->backupLabel),  strPtr(archiveId), strPtr(archiveRange.start),
                                        archiveRange.stop != NULL ?
                                            strPtr(strNewFmt(", stop = %s", strPtr(archiveRange.stop))) : "");

                                    // Add the archive range to the list
                                    lstAdd(archiveRangeList, &archiveRange);
                                }
                            }

                            // Get all major archive paths (timeline and first 32 bits of LSN)
                            StringList *walPathList =
                                strLstSort(
                                    storageListP(
                                        storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId)),
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
                                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath)),
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
                                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath)),
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
                                                        STORAGE_REPO_ARCHIVE "/%s/%s/%s", strPtr(archiveId), strPtr(walPath),
                                                        strPtr(walSubPath)));
                                            }

                                            // Track that this archive was removed
                                            archiveExpire.total++;
                                            archiveExpire.stop = strDup(strSubN(walSubPath, 0, 24));
                                            if (archiveExpire.start == NULL)
                                                archiveExpire.start = strDup(strSubN(walSubPath, 0, 24));
                                        }
                                        else
                                            logExpire(&archiveExpire, archiveId);
                                    }
                                }
                            }

                            // Log if no archive was expired
                            if (archiveExpire.total == 0)
                            {
                                LOG_DETAIL_FMT("no archive to remove, archiveId = %s", strPtr(archiveId));
                            }
                            // Log if there is more to log
                            else
                                logExpire(&archiveExpire, archiveId);
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
removeExpiredBackup(InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Get all the current backups in backup.info
    StringList *currentBackupList = strLstSort(infoBackupDataLabelList(infoBackup, NULL), sortOrderDesc);
    StringList *backupList = strLstSort(
        storageListP(
            storageRepo(), STORAGE_REPO_BACKUP_STR,
            .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
        sortOrderDesc);

    // Remove non-current backups from disk
    for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
    {
        if (!strLstExists(currentBackupList, strLstGet(backupList, backupIdx)))
        {
            LOG_INFO_FMT("remove expired backup %s", strPtr(strLstGet(backupList, backupIdx)));

            // Execute the real expiration and deletion only if the dry-run mode is disabled
            if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
            {
                storagePathRemoveP(
                    storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(strLstGet(backupList, backupIdx))),
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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Load the backup.info
        InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        if (cfgOptionTest(cfgOptRepoRetentionFullPeriod))
            expireTimeBasedFullBackup(infoBackup, cfgOptionUInt(cfgOptRepoRetentionFullPeriod));
        else
            expireFullBackup(infoBackup);

        expireDiffBackup(infoBackup);

        // Store the new backup info only if the dry-run mode is disabled
        if (!cfgOptionValid(cfgOptDryRun) || !cfgOptionBool(cfgOptDryRun))
        {
            infoBackupSaveFile(
                infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }

        removeExpiredBackup(infoBackup);
        removeExpiredArchive(infoBackup);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
