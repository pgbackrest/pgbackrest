/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/regExp.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

// CSHANG Need to decide if this needs to be a structure and if so if it needs to be instantiated or just variables in cmdExpire
// struct archiveExpire
// {
//     uint64_t total;
//     String *start;
//     String *stop;
// };

// CSHANG Putting manifest here until plans are finalized for new manifest
#define INFO_MANIFEST_FILE                                           "backup.manifest"

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
        String *archiveRetentionType =
            cfgOptionTest(cfgOptRepoRetentionArchiveType) ? cfgOptionStr(cfgOptRepoRetentionArchiveType) : NULL;
        unsigned int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionArchive) : 0;

        // Load the backup.info
        String *stanzaBackupPath = strNewFmt(STORAGE_PATH_BACKUP "/%s", strPtr(cfgOptionStr(cfgOptStanza)));
        InfoBackup *infoBackup = infoBackupNew(
            storageRepo(), strNewFmt("%s/%s", strPtr(stanzaBackupPath), INFO_BACKUP_FILE), false,
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        StringList *pathList = NULL;

        // Find all the expired full backups
        if (fullRetention > 0)
        {
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
                            strNewFmt(
                                "%s/%s/%s", strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                        storageRemoveNP(
                            storageRepoWrite(),
                            strNewFmt(
                                "%s/%s/%s",
                                strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

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
                                strNewFmt(
                                    "%s/%s/%s", strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                            storageRemoveNP(
                                storageRepoWrite(),
                                strNewFmt(
                                    "%s/%s/%s",
                                    strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

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

    StringList *backupList = strLstSort(
        storageListP(storageRepo(), stanzaBackupPath, .errorOnMissing = false, .expression = backupRegExp(true, true, true)),
        sortOrderDesc);

    // Get all the current backups in backup.info
    pathList = infoBackupDataLabelListP(infoBackup);

    // Remove backups from disk not in the backup:current section of backupInfo. This also cleans up orphaned directories.
    for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
    {
        if (!strLstExists(pathList, strLstGet(backupList, backupIdx)))
        {
            LOG_INFO("remove expired backup %s", strLstGet(backupList, backupIdx));

            storagePathRemoveP(storageRepoWrite(), strLstGet(backupList, backupIdx), .recurse = true);
        }
    }

    // If archive retention is undefined, then ignore archiving
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
        if (strCmp(archiveRetentionType, STR("full")) == 0)
        {
            globalBackupRetentionList = strLstSort(
                infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true), sortOrderDesc);
        }
        else if (strCmp(archiveRetentionType, STR("diff")) == 0)
        {
            globalBackupRetentionList = strLstSort(
                infoBackupDataLabelListP(infoBackup, .filter = backupRegExpP(.full = true, .differential = true), sortOrderDesc);
        }
        else if (strCmp(archiveRetentionType, STR("incr")) == 0)
        {
            globalBackupRetentionList = strLstSort(
                infoBackupDataLabelListP(
                    infoBackup, .filter = backupRegExpP(.full = true, .differential = true, .incremental = true),
                sortOrderDesc);
        }

        // If no backups were found then preserve current archive logs - too soon to expire them
        if (strLstSize(globalBackupRetentionList) > 0)
        {
            // CSHANG STOPPED HERE
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
