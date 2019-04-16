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
        // Get the retention options
        // ??? Retention options will need to be indexed
        unsigned int fullRetention = cfgOptionTest(cfgOptRepoRetentionFull) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionFull) : 0;
        unsigned int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ?
            (unsigned int) cfgOptionInt(cfgOptRepoRetentionDiff) : 0;
        // String *archiveRetentionType =
        //     cfgOptionTest(cfgOptRepoRetentionArchiveType) ? cfgOptionStr(cfgOptRepoRetentionArchiveType) : NULL;
        // int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ? cfgOptionInt(cfgOptRepoRetentionArchive) : 0;

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
                    if (strLstSize(removeList) > 1)
                        strCat(backupExpired, "set: ");

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        const String *removeBackupLabel = strLstGet(removeList, rmvIdx);

                        storageRemoveNP(
                            storageRepo(),
                            strNewFmt(
                                "%s/%s/%s", strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                        storageRemoveNP(
                            storageRepo(),
                            strNewFmt(
                                "%s/%s/%s",
                                strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE ".copy"));

                        // Remove the backup from the info file
                        infoBackupDataDelete(infoBackup, removeBackupLabel);

                        strCat(backupExpired, strPtr(removeBackupLabel));

                        // If this is not the last in the list, then separate the item in the list with a comma
                        if (rmvIdx < strLstSize(removeList) - 1)
                            strCat(backupExpired, ",");
                    }

                    LOG_INFO("expire full backup %s", strPtr(backupExpired));
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
                            storageRemoveNP(
                                storageRepo(),
                                strNewFmt(
                                    "%s/%s/%s", strPtr(stanzaBackupPath), strPtr(removeBackupLabel), INFO_MANIFEST_FILE));
// CSHANG before this was not removing the COPY file - probably an oversight?
// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                            storageRemoveNP(
                                storageRepo(),
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
                            "expire diff backup %s%s", (strChr(backupExpired, ',') != -1 ? "set: " : ""), strPtr(backupExpired));
                    }
                }
            }
        }

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
