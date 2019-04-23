/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
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
// struct archiveExpire
// {
//     uint64_t total;
//     String *start;
//     String *stop;
// };

// CSHANG Putting manifest here until plans are finalized for new manifest
#define INFO_MANIFEST_FILE                                           "backup.manifest"

static int
archiveIdAscComparator(const void *item1, const void *item2)
{
    StringList *archiveSort1 = strLstNewSplitZ(*(String **)item1, "-");
    StringList *archiveSort2 = strLstNewSplitZ(*(String **)item2, "-");
    int int1 = atoi(strPtr(strLstGet(archiveSort1, 1)));
    int int2 = atoi(strPtr(strLstGet(archiveSort2, 1)));

    if (int1 < int2)
        return -1;

    if (int1 > int2)
        return 1;

    return 0;
}

static int
archiveIdDescComparator(const void *item1, const void *item2)
{
    StringList *archiveSort1 = strLstNewSplitZ(*(String **)item1, "-");
    StringList *archiveSort2 = strLstNewSplitZ(*(String **)item2, "-");
    int int1 = atoi(strPtr(strLstGet(archiveSort1, 1)));
    int int2 = atoi(strPtr(strLstGet(archiveSort2, 1)));

    if (int1 > int2)
        return -1;

    if (int1 < int2)
        return 1;

    return 0;
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

/***********************************************************************************************************************************
Expire backups and archives
***********************************************************************************************************************************/
void
cmdExpire(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
// CSHANG Where to define constants (numbers and strings) for config stuff like MIN/MAX and "full", "diff", "incr"
        // Get the retention options
        // ??? Retention options will need to be indexed
        unsigned int fullRetention = cfgOptionTest(cfgOptRepoRetentionFull) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionFull) : 0;
        unsigned int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionDiff) : 0;
        const String *archiveRetentionType =
            cfgOptionTest(cfgOptRepoRetentionArchiveType) ? cfgOptionStr(cfgOptRepoRetentionArchiveType) : NULL;
        unsigned int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionArchive) : 0;

        // Load the backup.info
        InfoBackup *infoBackup = infoBackupNew(
            storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE), false,
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        StringList *pathList = NULL;

        // Find all the expired full backups
        if (fullRetention > 0)
        {
            // Get list of current full backups (defaul order is oldest to newest)
            pathList = infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(pathList) > fullRetention)
            {
                // Expire all backups that depend on the full backup
                for (unsigned int fullIdx = 0; fullIdx < strLstSize(pathList) - fullRetention; fullIdx++)
                {
                    // The list of backups to remove includes the full backup and the default sort order will put it first
                    StringList *removeList = infoBackupDataLabelListP(
                        infoBackup, .filter = strNewFmt("^%s.*", strPtr(strLstGet(pathList, fullIdx))));

                    // Initialize the log message
                    String *backupExpired = strNew("");

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        const String *removeBackupLabel = strLstGet(removeList, rmvIdx);

                        storageRemoveNP(
                            storageRepoWrite(),
                            strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                        storageRemoveNP(
                            storageRepoWrite(),
                            strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

                        // Remove the backup from the info file
                        infoBackupDataDelete(infoBackup, removeBackupLabel);

                        if (strSize(backupExpired) == 0)
                            strCat(backupExpired, strPtr(removeBackupLabel));
                        else
                            strCatFmt(backupExpired, ", %s", strPtr(removeBackupLabel));
                    }

                    // If the message contains a comma, then prepend "set:"
                    LOG_INFO(
                        "expire full backup %s%s", (strChr(backupExpired, ',') != -1 ? "set: " : ""), strPtr(backupExpired));
                }
            }

        }

        // Find all the expired differential backups
        if (differentialRetention > 0)
        {
            // Get a list of full and differential backups. Full are considered differential for the purpose of retention.
            // Example: F1, D1, D2, F2, repo-retention-diff=2, then F1,D2,F2 will be retained, not D2 and D1 as might be expected.
            pathList = infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true, .differential = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(pathList) > differentialRetention)
            {
                for (unsigned int diffIdx = 0; diffIdx < strLstSize(pathList) - differentialRetention; diffIdx++)
                {
                    // Skip if this is a full backup.  Full backups only count as differential when deciding which differential
                    // backups to expire.
                    if (regExpMatchOne(backupRegExpP(.full = true), strLstGet(pathList, diffIdx)))
                        continue;

                    // Get a list of all differential and incremental backups
                    StringList *removeList = infoBackupDataLabelListP(
                        infoBackup, .filter = backupRegExpP(.differential = true, .incremental = true));

                    // Initialize the log message
                    String *backupExpired = strNew("");

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        const String *removeBackupLabel = strLstGet(removeList, rmvIdx);

                        LOG_DEBUG("checking %s for differential expiration",  strPtr(removeBackupLabel));

                        // Remove all differential and incremental backups before the oldest valid differential
                        // (removeBackupLabel < oldest valid differential)
                        if (strCmp(removeBackupLabel, strLstGet(pathList, diffIdx + 1)) == -1)
                        {
// CSHANG Combine the remove, delete and updating backupExpired into a function since this is duplicate of code above in full
                            storageRemoveNP(
                                storageRepoWrite(),
                                strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                            storageRemoveNP(
                                storageRepoWrite(),
                                strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

                            // Remove the backup from the info file
                            infoBackupDataDelete(infoBackup, removeBackupLabel);

                            if (strSize(backupExpired) == 0)
                                strCat(backupExpired, strPtr(removeBackupLabel));
                            else
                                strCatFmt(backupExpired, ", %s", strPtr(removeBackupLabel));
                        }
                    }

                    // If the message contains a comma, then prepend "set:"
                    LOG_INFO(
                        "expire diff backup %s%s", (strChr(backupExpired, ',') != -1 ? "set: " : ""), strPtr(backupExpired));
                }
            }
        }

// CSHANG Must save infoBackup here!!!

        // Get all the current backups in backup.info
        pathList = strLstSort(infoBackupDataLabelListP(infoBackup), sortOrderDesc);

        // Get list of all backup directories that are not in the backup:current section of backupInfo
        StringList *backupList = strLstMergeAnti(
            strLstSort(
                storageListP(
                    storageRepo(), STRDEF(STORAGE_REPO_BACKUP), .errorOnMissing = false,
                    .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
                sortOrderDesc),
            pathList);

        // Remove non-current backups from disk
        for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
        {
            LOG_INFO("remove expired backup %s", strLstGet(backupList, backupIdx));
            storagePathRemoveP(storageRepoWrite(), strLstGet(backupList, backupIdx), .recurse = true);
        }

        // If archive retention is undefined, then ignore archiving. The user does not have to set this - it will be defaulted in
        // cfgLoadUpdateOption based on certain rules.
        if (archiveRetention == 0)
        {
             LOG_INFO("option '%s' is not set - archive logs will not be expired", cfgOptionName(cfgOptRepoRetentionArchive));
        }
        else
        {
            StringList *globalBackupRetentionList = NULL;
    // CSHANG Need to have global constants for these not hardcoded full, diff, incr
            // Determine which backup type to use for archive retention (full, differential, incremental) and get a list of the
            // remaining non-expired backups, from newest to oldest, based on the type.
            if (strCmp(archiveRetentionType, STRDEF("full")) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true)), sortOrderDesc);
            }
            else if (strCmp(archiveRetentionType, STRDEF("diff")) == 0)
            {
                globalBackupRetentionList = strLstSort(
                    infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true, .differential = true)), sortOrderDesc);
            }
            else if (strCmp(archiveRetentionType, STRDEF("incr")) == 0)
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
                    storageRepo(),
                    STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), false, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
                InfoPg *infoArchivePg = infoArchivePg(infoArchive);

                // Get a list of archive directories (e.g. 9.4-1, 10-2, etc) sorted by the db-id (number after the dash).
                StringList *listArchiveDisk = sortArchiveId(
                    storageListP(
                        storageRepo(),
                        STRDEF(STORAGE_REPO_ARCHIVE), .errorOnMissing = false, .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                    sortOrderAsc);


                StringList *globalBackupArchiveRetentionList = strLstNew();

                // globalBackupRetentionList is ordered newest to oldest backup, so create globalBackupArchiveRetentionList of the
                // newest backups whose archives will be retained
                for (unsigned int idx = 0; idx < archiveRetention; idx++)
                    strLstAdd(globalBackupArchiveRetentionList, strLstGet(globalBackupRetentionList, idx));

                // Loop through the archive.info history from oldest to newest and if there is a corresponding directory on disk
                // then remove WAL that are not part of retention
                for (unsigned int pgIdx = infoPgDataTotal(infoArchivePg) - 1; (int)pgIdx >= 0; pgIdx--))
                {
                    String *archiveId = infoPgArchiveId(infoArchivePg, pgIdx);
                    StringList *localBackupRetentionList = strLstNew();

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
                                    (backupData.backupPgId == archivePgId)
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
                            InfoPgData currentPg = infoPgDataCurrent(infoArchivePg);
                            if (currentPg.id != archivePgId)
                            {
                                String *fullPath = storagePath(storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId));
                                storagePathRemoveP(storageRepoWrite(), fullPath, .recurse = true);
                                LOG_INFO("remove archive path: %s", strPtr(fullPath));
                            }

                            // Continue to next directory
                            break;
                        }
                    }
                // CSHANG Stooped here. Next: my @stryLocalBackupArchiveRentention;
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
