/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "command/backup/common.h"
#include "common/debug.h"
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
        // int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ? cfgOptionInt(cfgOptRepoRetentionDiff) : 0;
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
            pathList = infoBackupDataLabelListP(infoBackup, .filter = backupRegExpGetP(.full = true));

            // If there are more full backups then the number to retain, then expire the oldest ones
            if (strLstSize(pathList) > fullRetention)
            {
                // Expire all backups that depend on the full backup
                for (unsigned int fullIdx = 0; fullIdx < strLstSize(pathList) - fullRetention; fullIdx++)
                {
                    StringList *removeList = infoBackupDataLabelListP(
                        infoBackup, .filter = strNewFmt("^%s.*", strPtr(strLstGet(pathList, fullIdx))));

                    // Remove the manifest files in each directory and remove the backup from the current section of backup.info
                    for (unsigned int rmvIdx = 0; rmvIdx < strLstSize(removeList); rmvIdx++)
                    {
                        storageRemoveNP(
                            storageRepo(),
                            strNewFmt(
                                "%s/%s/%s", strPtr(stanzaBackupPath), strPtr(strLstGet(removeList, rmvIdx)), INFO_MANIFEST_FILE));

// CSHANG Maybe expose INI_COPY_EXT vs hardcode .copy, but David is working on manifest and there may be a different way of handling the file
                        storageRemoveNP(
                            storageRepo(),
                            strNewFmt(
                                "%s/%s/%s",
                                strPtr(stanzaBackupPath), strPtr(strLstGet(removeList, rmvIdx)), INFO_MANIFEST_FILE ".copy"));
                    // CSHANG Need to create a delete function in infoBackup to remove the backup from the current section
        LOG_INFO("removeList %s", strPtr(strLstGet(removeList, rmvIdx)));
                    }
        LOG_INFO("pathList %s", strPtr(strLstGet(pathList, fullIdx)));
                    // CSHANG Need the log info for the backups expired
                }
            }

        }

        if (strLstSize(pathList) > fullRetention)
        {
            LOG_INFO("pathlist > fullretention");
        }

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
