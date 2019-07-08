/***********************************************************************************************************************************
Stanza Delete Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/control/control.h"
#include "command/stanza/stanzaDelete.h"
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
        bool lockStopExists = false;
        Storage *storageRepoWriteStanza = storageRepoWrite();

// CSHANG Maybe bypass this check if --force used?
        // Check for a stop file for this or all stanzas
        TRY_BEGIN()
        {
            lockStopTest();  // CSHANG this uses storageLocal so does it work remotely? Does it need to?
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
                FileMissingError, "stop file does not exist for stanza '%s'" .
                "\nHINT: has the pgbackrest stop command been run on this server?", strPtr(cfgOptionStr(cfgOptStanza)));
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

// CSHANG Need to call storageInfoList

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
