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

// CSHANG In the old code we use pathExists to determine if there is anything to do, but I see that "not all drivers implement" it meaning s3. Since the deletes are not autonomous, we could end up in a state where there might be something left. I don't undestand why storageList can return paths but storagePathExists cannot.
        if (strLstSize(storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP))) > 0 ||
            strLstSize(storageListP(storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE))) > 0)
        {
            bool lockStopExists = false;

    // CSHANG Maybe bypass this check if --force used?
            // Check for a stop file for this or all stanzas
            TRY_BEGIN()
            {
// CSHANG In a separate commit, modify lockStopTest to return a boolean and accept a parameter whether stanza is required - rather than doing a TRY/CATCH here
                lockStopTest();
            }
            CATCH(StopError)
            {
                lockStopExists = true;
            }
            TRY_END();

            // If the stop file does not exist, then error
            if (!lockStopExists)
            {
    // CSHANG Maybe add HINT to use force?
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
        //         confess &log(ERROR, DB_FILE_POSTMASTERPID . " exists - looks like the postmaster is running. " .
        //             "To delete stanza '${strStanza}', shutdown the postmaster for stanza '${strStanza}' and try again, " .
        //             "or use --force.", ERROR_POSTMASTER_RUNNING);
        //     }
        // }

            // Delete the archive info files
            storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE));
            storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT));

            // Delete the backup info files
            storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE));
            storageRemoveNP(storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT));

    //CSHANG if a file that matches the regex, but not a directory then error? Remove should fail if not a file -- add test for this and leave as a test if it errors

    // CSHANG What about sort order? Sorting from newest to oldest with sortOrderDesc - old code used "reverse" which I believe was newest to oldest
            StringList *backupList = strLstSort(storageListP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP), .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)), sortOrderDesc);

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
        else
        {
            LOG_INFO("stanza %s already deleted", strPtr(cfgOptionStr(cfgOptStanza)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
