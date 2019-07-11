/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/control/control.h"
#include "command/stanza/stanzaDelete.h"
#include "command/backup/common.h"
#include "common/debug.h"
#include "common/encode.h" // CSHANG Is this necessary?
#include "common/encode/base64.h" // CSHANG Is this necessary?
#include "common/io/handleWrite.h" // CSHANG Is this necessary?
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoManifest.h"
#include "info/infoPg.h"
#include "postgres/interface.h" // CSHANG Is this necessary?
#include "postgres/version.h" // CSHANG Is this necessary?
#include "storage/helper.h"

/***********************************************************************************************************************************
Callback function for StorageInfoList
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Process stanza-delete
***********************************************************************************************************************************/
void
cmdStanzaDelete(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *storageRepoReadStanza = storageRepo();
        const Storage *storageRepoWriteStanza = storageRepoWrite();

        // For most drivers, NULL indicates the directory does not exist at all. For those that do not support paths (e.g. S3) an
        // empty StringList will be returned which will result in the attempt to delete the stanza even though it may not exist.
        StringList *archiveList = storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE), .nullOnMissing = true);
        StringList *backupList = storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP), .nullOnMissing = true);
        if (archiveList != NULL || backupList != NULL)
        {
            // If anything exists in the stanza repo, then ensure the pgbackrest stop command was issued for the stanza before
            // attempting the delete.
            if (strLstSize(archiveList) > 0 || strLstSize(backupList) > 0)
            {
                // If the stop file does not exist, then error. This check is required even when --force is issued.
                if (!storageExistsNP(storageLocal(), lockStopFileName(cfgOptionStr(cfgOptStanza))))
                {
                    THROW_FMT(
                        FileMissingError, "stop file does not exist for stanza '%s'\n"
                        "HINT: has the pgbackrest stop command been run on this server?", strPtr(cfgOptionStr(cfgOptStanza)));
                }

                // if (!cfgOptionTest(cfgOptForce))

                // # If a force has not been issued, then check the database
                // if (!cfgOption(CFGOPT_FORCE))
                // {
                //     # Get the master database object and index
                //     my ($oDbMaster, $iMasterRemoteIdx) = dbObjectGet({bMasterOnly => true});
                //
                //     # Initialize the master file object and path
                //     my $oStorageDbMaster = storageDb({iRemoteIdx => $iMasterRemoteIdx});
                //
                //     # Check if Postgres is running and if so only continue when forced
                //     if ($oStorageDbMaster->exists(DB_FILE_POSTMASTERPID))
                //     {
                        // confess &log(ERROR, DB_FILE_POSTMASTERPID . " exists - looks like the postmaster is running. " .
                        //     "To delete stanza '${strStanza}', shutdown the postmaster for stanza '${strStanza}' and try again, " .
                        //     "or use --force.", ERROR_POSTMASTER_RUNNING);
                //     }
                // }

                // Delete the archive info files
                storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE));
                storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT));

                // Delete the backup info files
                storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE));
                storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT));

    //CSHANG if a file that matches the regex, but not a directory then error? Remove should fail if not a file -- add test for this and leave as a test if it errors

                // Get the list of backup directories from newest to oldest since don't want to invalidate a backup before
                // invalidating any backups that depend on it.
                StringList *backupList = strLstSort(storageListP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)), sortOrderDesc);

                // Delete all manifest files
                for (unsigned int idx = 0; idx < strLstSize(backupList); idx++)
                {
                    storageRemoveNP(
                        storageRepoWriteStanza,
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE, strPtr(strLstGet(backupList, idx))));
                    storageRemoveNP(
                        storageRepoWriteStanza,
                        strNewFmt(STORAGE_REPO_BACKUP "/%s/" INFO_MANIFEST_FILE INFO_COPY_EXT, strPtr(strLstGet(backupList, idx))));
                }
            }
            // Recusively remove the entire staza repo
            storagePathRemoveP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE), .recurse = true);
            storagePathRemoveP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP), .recurse = true);

// CSHANG We need a lockStart to remove the file or just remove it ourselves
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
