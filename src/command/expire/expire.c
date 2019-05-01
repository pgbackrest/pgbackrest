/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "command/archive/common.h"
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/regExp.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

// CSHANG These are for the archiveId sort function but maybe the functions should go in stringList.c/.h? I think they only should go there if there is another use case or maybe go in the command/backup/common.c file? Or maybe we need an info/common.c file
#include <stdlib.h>
#include "common/type/list.h"

// CSHANG Need to decide if this needs to be a structure and if so if it needs to be instantiated or just variables in cmdExpire
struct ArchiveExpired
{
    uint64_t total;
    String *start;
    String *stop;
};

// CSHANG Putting manifest here until plans are finalized for new manifest
#define INFO_MANIFEST_FILE                                           "backup.manifest"

static int
archiveIdAscComparator(const void *item1, const void *item2)
{
    StringList *archiveSort1 = strLstNewSplitZ(*(String **)item1, "-");
    StringList *archiveSort2 = strLstNewSplitZ(*(String **)item2, "-");
    int int1 = atoi(strPtr(strLstGet(archiveSort1, 1)));
    int int2 = atoi(strPtr(strLstGet(archiveSort2, 1)));

    return (int1 - int2);
}

static int
archiveIdDescComparator(const void *item1, const void *item2)
{
    StringList *archiveSort1 = strLstNewSplitZ(*(String **)item1, "-");
    StringList *archiveSort2 = strLstNewSplitZ(*(String **)item2, "-");
    int int1 = atoi(strPtr(strLstGet(archiveSort1, 1)));
    int int2 = atoi(strPtr(strLstGet(archiveSort2, 1)));

    return (int2 - int1);
}

static StringList *
sortArchiveId(StringList *this, SortOrder sortOrder)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING_LIST, this);
        FUNCTION_TEST_PARAM(ENUM, sortOrder);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    switch (sortOrder)
    {
        case sortOrderAsc:
        {
            lstSort((List *)this, archiveIdAscComparator);
            break;
        }

        case sortOrderDesc:
        {
            lstSort((List *)this, archiveIdDescComparator);
            break;
        }
    }

    FUNCTION_TEST_RETURN(this);
}

typedef struct ArchiveRange
{
    const String *start;
    const String *stop;
} ArchiveRange;

static void
expireBackup(InfoBackup *infoBackup, String *removeBackupLabel, String *backupExpired)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_TEST_PARAM(STRING, removeBackupLabel);
        FUNCTION_TEST_PARAM(STRING, backupExpired);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);
    ASSERT(removeBackupLabel != NULL);
    ASSERT(backupExpired != NULL);

// CSHANG Maybe expose INI_COPY_EXT in info.c/.h vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
    storageRemoveNP(
        storageRepoWrite(),
        strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

    storageRemoveNP(
        storageRepoWrite(),
        strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

    // Remove the backup from the info file
    infoBackupDataDelete(infoBackup, removeBackupLabel);

    if (strSize(backupExpired) == 0)
        strCat(backupExpired, strPtr(removeBackupLabel));
    else
        strCatFmt(backupExpired, ", %s", strPtr(removeBackupLabel));

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Expire differential backups
***********************************************************************************************************************************/
static unsigned int
expireDiffBackup(InfoBackup *infoBackup)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // ??? Retention options will need to be indexed
        unsigned int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionDiff) : 0;

        // Find all the expired differential backups
        if (differentialRetention > 0)
        {
            // Get a list of full and differential backups. Full are considered differential for the purpose of retention.
            // Example: F1, D1, D2, F2, repo-retention-diff=2, then F1,D2,F2 will be retained, not D2 and D1 as might be expected.
            StringList *currentBackupList = infoBackupDataLabelListP(
                infoBackup, .filter = backupRegExpP(.full = true, .differential = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(currentBackupList) > differentialRetention)
            {
                for (unsigned int diffIdx = 0; diffIdx < strLstSize(currentBackupList) - differentialRetention; diffIdx++)
                {
                    // Skip if this is a full backup.  Full backups only count as differential when deciding which differential
                    // backups to expire.
                    if (regExpMatchOne(backupRegExpP(.full = true), strLstGet(currentBackupList, diffIdx)))
                        continue;

                    // Get a list of all differential and incremental backups
                    StringList *removeList = infoBackupDataLabelListP(
                        infoBackup, .filter = backupRegExpP(.differential = true, .incremental = true));

                    // Initialize the log message
                    String *backupExpired = strNew("");

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        String *removeBackupLabel = strLstGet(removeList, rmvIdx);

                        LOG_DEBUG("Expire process, checking %s for differential expiration",  strPtr(removeBackupLabel));

                        // Remove all differential and incremental backups before the oldest valid differential
                        // (removeBackupLabel < oldest valid differential)
                        if (strCmp(removeBackupLabel, strLstGet(currentBackupList, diffIdx + 1)) < 0)
                        {
                            expireBackup(infoBackup, removeBackupLabel, backupExpired);
                            result++;
                        }
                    }

                    // If the message contains a comma, then prepend "set:"
                    LOG_INFO(
                        "expire diff backup %s%s", (strChr(backupExpired, ',') != -1 ? "set: " : ""), strPtr(backupExpired));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Expire full backups
***********************************************************************************************************************************/
static unsigned int
expireFullBackup(InfoBackup *infoBackup)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);

    unsigned int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // ??? Retention options will need to be indexed
        unsigned int fullRetention = cfgOptionTest(cfgOptRepoRetentionFull) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionFull) : 0;

        // Find all the expired full backups
        if (fullRetention > 0)
        {
            // Get list of current full backups (default order is oldest to newest)
            StringList *currentBackupList = infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(currentBackupList) > fullRetention)
            {
                // Expire all backups that depend on the full backup
                for (unsigned int fullIdx = 0; fullIdx < strLstSize(currentBackupList) - fullRetention; fullIdx++)
                {
                    // The list of backups to remove includes the full backup and the default sort order will put it first
                    StringList *removeList = infoBackupDataLabelListP(
                        infoBackup, .filter = strNewFmt("^%s.*", strPtr(strLstGet(currentBackupList, fullIdx))));

                    // Initialize the log message
                    String *backupExpired = strNew("");

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        String *removeBackupLabel = strLstGet(removeList, rmvIdx);
                        expireBackup(infoBackup, removeBackupLabel, backupExpired);
                        result++;
                    }

                    // If the message contains a comma, then prepend "set:"
                    LOG_INFO(
                        "expire full backup %s%s", (strChr(backupExpired, ',') != -1 ? "set: " : ""), strPtr(backupExpired));
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Process archive retention
***********************************************************************************************************************************/
static void
removeExpiredArchive(InfoBackup *infoBackup)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the retention options
        const String *archiveRetentionType =
            cfgOptionTest(cfgOptRepoRetentionArchiveType) ? cfgOptionStr(cfgOptRepoRetentionArchiveType) : NULL;
        unsigned int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionArchive) : 0;
LOG_INFO("ARCHIVE-RETENT: %u", (unsigned int) cfgOptionTest(cfgOptRepoRetentionArchive));
        // If archive retention is undefined, then ignore archiving. The user does not have to set this - it will be defaulted in
        // cfgLoadUpdateOption based on certain rules.
        if (archiveRetention == 0)
        {
             LOG_INFO("option '%s' is not set - archive logs will not be expired", cfgOptionName(cfgOptRepoRetentionArchive));
        }
        else
        {
            StringList *globalBackupRetentionList = NULL;

            // Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
            // remaining non-expired backups, from newest to oldest, based on the type.
            if (strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL)) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true)), sortOrderDesc);
            }
            else if (strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_DIFF)) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true, .differential = true)), sortOrderDesc);
            }
            else if (strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_INCR)) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelListP(
                        infoBackup, .filter = backupRegExpP(.full = true, .differential = true, .incremental = true)),
                    sortOrderDesc);
            }

            // Expire archives. If no backups were found then preserve current archive logs - too soon to expire them.
            if (strLstSize(globalBackupRetentionList) > 0)
            {
                // Attempt to load the archive info file
                InfoArchive *infoArchive = infoArchiveNew(
                    storageRepo(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), false,
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

                InfoPg *infoArchivePgData = infoArchivePg(infoArchive);

                // Get a list of archive directories (e.g. 9.4-1, 10-2, etc) sorted by the db-id (number after the dash).
                StringList *listArchiveDisk = sortArchiveId(
                    storageListP(
                        storageRepo(),
                        STRDEF(STORAGE_REPO_ARCHIVE), .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                    sortOrderAsc);

                StringList *globalBackupArchiveRetentionList = strLstNew();

                // globalBackupRetentionList is ordered newest to oldest backup, so create globalBackupArchiveRetentionList of the
                // newest backups whose archives will be retained
                for (unsigned int idx = 0; idx < archiveRetention; idx++)
                    strLstAdd(globalBackupArchiveRetentionList, strLstGet(globalBackupRetentionList, idx));

                // Loop through the archive.info history from oldest to newest and if there is a corresponding directory on disk
                // then remove WAL that are not part of retention
                for (unsigned int pgIdx = infoPgDataTotal(infoArchivePgData) - 1; (int)pgIdx >= 0; pgIdx--)
                {
                    String *archiveId = infoPgArchiveId(infoArchivePgData, pgIdx);
                    StringList *localBackupRetentionList = strLstNew();

                    // Initialize the expired archive information for this archive ID
                    struct ArchiveExpired archiveExpire = {.total = 0, .start = NULL, .stop = NULL};

                    for (unsigned int archiveIdx = 0; archiveIdx < strLstSize(listArchiveDisk); archiveIdx++)
                    {
                        if (strCmp(archiveId, strLstGet(listArchiveDisk, archiveIdx)) != 0)
                            continue;

                        StringList *archiveSplit = strLstNewSplitZ(archiveId, "-");
                        unsigned int archivePgId = cvtZToUInt(strPtr(strLstGet(archiveSplit, 1)));

                        // From the global list of backups to retain, create a list of backups, oldest to newest, associated with this
                        // archiveId (e.g. 9.4-1), e.g. If globalBackupRetention has 4F, 3F, 2F, 1F then localBackupRetentionList will
                        // have 1F, 2F, 3F, 4F (assuming they all have same history id)
                        for (unsigned int retentionIdx = strLstSize(globalBackupRetentionList) - 1;
                            (int)retentionIdx >=0; retentionIdx--)
                        {
                            for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
                            {
                                InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);
                                if ((strCmp(backupData.backupLabel, strLstGet(globalBackupRetentionList, retentionIdx)) == 0) &&
                                    (backupData.backupPgId == archivePgId))
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
                                String *fullPath = storagePath(
                                    storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId)));
                                storagePathRemoveP(storageRepoWrite(), fullPath, .recurse = true);
                                LOG_INFO("remove archive path: %s", strPtr(fullPath));
                            }

                            // Continue to next directory
                            break;
                        }

                        // If get here, then a local backup was found for retention
                        StringList *localBackupArchiveRententionList = strLstNew();

                        // If the archive retention is less than or equal to the number of all backups, then perform selective
                        // expiration
                        if ((strLstSize(globalBackupArchiveRetentionList) > 0) &&
                            (archiveRetention <= strLstSize(globalBackupRetentionList)))
                        {
                            // From the full list of backups in archive retention, find the intersection of local backups to retain
                            // from oldest to newest
                            for (unsigned int globalIdx = strLstSize(globalBackupArchiveRetentionList) - 1;
                                (int)globalIdx >= 0; globalIdx--)
                            {
                                for (unsigned int localIdx = 0; localIdx < strLstSize(localBackupRetentionList); localIdx++)
                                {
                                    if (strCmp(strLstGet(globalBackupArchiveRetentionList, globalIdx),
                                        strLstGet(localBackupRetentionList, localIdx)) == 0)
                                    {
                                        strLstAdd(localBackupArchiveRententionList, strLstGet(localBackupRetentionList, localIdx));
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
                            if ((strCmp(archiveRetentionType, STRDEF(CFGOPTVAL_TMP_REPO_RETENTION_ARCHIVE_TYPE_FULL)) == 0) &&
                                (strLstSize(localBackupRetentionList) > 0))
                            {
                                LOG_INFO(
                                    "full backup total < %u - using oldest full backup for %s archive retention",
                                    archiveRetention, strPtr(archiveId));
                                strLstAdd(localBackupArchiveRententionList, strLstGet(localBackupRetentionList, 0));
                            }
                        }

                        // If no local backups were found as part of retention then set the backup archive retention to the newest backup
                        // so that the database is fully recoverable (can be recovered from the last backup through pitr)
                        if (strLstSize(localBackupArchiveRententionList) == 0)
                        {
                            strLstAdd(
                                localBackupArchiveRententionList,
                                strLstGet(localBackupRetentionList, strLstSize(localBackupRetentionList) - 1));
                        }

                        // Get the data for the backup selected for retention and all backups associated with this archive id
                        List *archiveIdBackupList = NULL;
                        InfoBackupData archiveRetentionBackup = {0};
                        for (unsigned int infoBackupIdx = 0; infoBackupIdx < infoBackupDataTotal(infoBackup); infoBackupIdx++)
                        {
                            InfoBackupData archiveIdBackup = infoBackupData(infoBackup, infoBackupIdx);

                            if (strCmp(archiveIdBackup.backupLabel, strLstGet(localBackupArchiveRententionList, 0)) == 0)
                                archiveRetentionBackup = infoBackupData(infoBackup, infoBackupIdx);

                            if (archiveIdBackup.backupPgId == archivePgId)
                                lstAdd(archiveIdBackupList, &archiveIdBackup);
                        }

                        bool removeArchive = false;
                        // Only expire if the selected backup has archive info - backups performed with --no-online will
                        // not have archive info and cannot be used for expiration.
                        if (archiveRetentionBackup.backupArchiveStart != NULL)
                        {
                            // Get archive ranges to preserve.  Because archive retention can be less than total retention it is
                            // important to preserve archive that is required to make the older backups consistent even though they
                            // cannot be played any further forward with PITR.
                            String *archiveExpireMax = NULL;
                            List *archiveRangeList = NULL;

                            // From the full list of backups, loop through those associated with this archiveId
                            for (unsigned int backupListIdx = 0; backupListIdx < lstSize(archiveIdBackupList); backupListIdx++)
                            {
                                InfoBackupData *backupData = lstGet(archiveIdBackupList, backupListIdx);

                                // If the backup is earlier than or the same as the retention backup and the backup has an
                                // archive start
                                if ((strCmp(backupData->backupLabel, archiveRetentionBackup.backupLabel) <= 0) &&
                                    (backupData->backupArchiveStart != NULL))
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

                                    LOG_DETAIL("archive retention on backup %s, archiveId = %s, start = %s",
                                        strPtr(backupData->backupLabel),  strPtr(archiveId), strPtr(archiveRange.start),
                                        (archiveRange.stop != NULL ?
                                            strPtr(strNewFmt(", stop = %s", strPtr(archiveRange.stop))) : ""));

                                    // Add the archive range to the list
                                    lstAdd(archiveRangeList, &archiveRange);
                                }
                            }

                            // Get all major archive paths (timeline and first 32 bits of LSN)
    // CSHANG Wait, what is being returned here - the fully qualified path or just the subdir
                            StringList *walPathList =
                                storageListP(
                                    storageRepo(),
                                    strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId)), .errorOnMissing = true,
                                    .expression = STRDEF(WAL_SEGMENT_DIR_REGEXP));

                            for (unsigned int walIdx = 0; walIdx < strLstSize(walPathList); walIdx++)
                            {
                                String *walPath = strLstGet(walPathList, walIdx);
                                removeArchive = true;

                                LOG_DEBUG("Expire process, found major WAL path: %s", strPtr(walPath));

                                // Keep the path if it falls in the range of any backup in retention
                                for (unsigned int rangeIdx = 0; rangeIdx < lstSize(archiveRangeList); rangeIdx++)
                                {
                                    ArchiveRange *archiveRange = lstGet(archiveRangeList, rangeIdx);

                                    if ((strCmp(walPath, strSubN(archiveRange->start, 0, 16)) >= 0) &&
                                        (archiveRange->stop != NULL || strCmp(walPath, strSubN(archiveRange->stop, 0, 16)) <= 0))
                                    {
                                        removeArchive = false;
                                        break;
                                    }
                                }

                                // Remove the entire directory if all archive is expired
                                if (removeArchive)
                                {
                                    String *fullPath = strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath));
                                    storagePathRemoveP(storageRepoWrite(), fullPath, .recurse = true);
                                    LOG_DEBUG("Expire process, remove major WAL path: %s", strPtr(fullPath));
                                    archiveExpire.total++;
                                    archiveExpire.start = strDup(walPath);
                                    archiveExpire.stop = strDup(walPath);
    // CSHANG Must figure out how to deal with the logging self->logExpire($strArchiveId, $strPath);
                                }
                                // Else delete individual files instead if the major path is less than or equal to the most recent
                                // retention backup.  This optimization prevents scanning though major paths that could not possibly
                                // have anything to expire.
                                else if (strCmp(walPath, strSubN(archiveExpireMax, 0, 16)) <= 0)
                                {
                                    // Look for files in the archive directory
    // CSHANG What does this really return? Should be like 000000010000000000000002-224520cda22b2788ca91630706080cca2634f813.gz
                                    StringList *walSubPathList =
                                        storageListP(
                                            storageRepo(),
                                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath)),
                                            .errorOnMissing = true, .expression = STRDEF("^[0-F]{24}.*$"));

                                    for (unsigned int subIdx = 0; subIdx < strLstSize(walSubPathList); subIdx++)
                                    {
                                        removeArchive = true;
                                        String *walSubPath = strLstGet(walSubPathList, subIdx);

                                        // Determine if the individual archive log is used in a backup
                                        for (unsigned int rangeIdx = 0; rangeIdx < lstSize(archiveRangeList); rangeIdx++)
                                        {
                                            ArchiveRange *archiveRange = lstGet(archiveRangeList, rangeIdx);

                                            if ((strCmp(strSubN(walSubPath, 0, 24), archiveRange->start) >= 0) &&
                                                ((archiveRange->stop != NULL) ||
                                                (strCmp(strSubN(walSubPath, 0, 24), archiveRange->stop) <= 0)))
                                            {
                                                removeArchive = false;
                                                break;
                                            }
                                        }
    // CSHANG ??? I don't understand how the old code actually removed the archive log. I've changed it here.
                                        // Remove archive log if it is not used in a backup
                                        if (removeArchive)
                                        {
                                            storageRemoveNP(
                                                storageRepoWrite(),
                                                strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s/%s",
                                                    strPtr(archiveId), strPtr(walPath), strPtr(walSubPath)));

                                            LOG_DEBUG("Expire process, remove WAL segment: %s", strPtr(walSubPath));

                                            // Track that this archive was removed
                                            archiveExpire.total++;
                                            archiveExpire.stop = strDup(strSubN(walSubPath, 0, 24));
                                            if (archiveExpire.start == NULL)
                                                archiveExpire.start = strDup(strSubN(walSubPath, 0, 24));
                                        }
                                        else
                                        {
                                            if (archiveExpire.start != NULL)
                                            {
                                                LOG_DETAIL(
                                                    "remove archive: archiveId = %s, start = %s, stop = %s",
                                                    archiveId, strPtr(archiveExpire.start), strPtr(archiveExpire.stop));

                                                archiveExpire.start = NULL;
                                            }
                                        }
                                    }
                                }
                            }
    // CSHANG Must figure out how to deal with the logging:
                        // Log if no archive was expired
                            if (archiveExpire.total == 0)
                                LOG_DETAIL("no archive to remove, archiveId = %s", strPtr(archiveId));
                            else if (archiveExpire.start != NULL)
                            {
                                // Force out any remaining message
                                LOG_DETAIL(
                                    "remove archive: archiveId = %s, start = %s, stop = %s",
                                    archiveId, strPtr(archiveExpire.start), strPtr(archiveExpire.stop));

                                archiveExpire.start = NULL;
                            }
                        }
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove expired backups from repo
***********************************************************************************************************************************/
static void
removeExpiredBackup(InfoBackup *infoBackup)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);

    // Get all the current backups in backup.info
    StringList *currentBackupList = strLstSort(infoBackupDataLabelListP(infoBackup), sortOrderDesc);
    StringList *backupList = strLstSort(
        storageListP(
            storageRepo(), STRDEF(STORAGE_REPO_BACKUP), .errorOnMissing = false,
            .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
        sortOrderDesc);

    // Remove non-current backups from disk
    for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
    {
        if (!strLstExists(currentBackupList, strLstGet(backupList, backupIdx)))
        {
            LOG_INFO("remove expired backup %s", strPtr(strLstGet(backupList, backupIdx)));
            storagePathRemoveP(
                storageRepoWrite(),
                strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(strLstGet(backupList, backupIdx))), .recurse = true);
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Expire backups and archives
***********************************************************************************************************************************/
void
cmdExpire(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load the backup.info
        InfoBackup *infoBackup = infoBackupNew(
            storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE), false,
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        expireFullBackup(infoBackup);
        expireDiffBackup(infoBackup);

        infoBackupSave(infoBackup, storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        removeExpiredBackup(infoBackup);
        removeExpiredArchive(infoBackup);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
