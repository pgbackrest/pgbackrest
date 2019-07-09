/***********************************************************************************************************************************
Stanza Update Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/stanza/stanzaUpgrade.h"
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
Process stanza-upgrade
***********************************************************************************************************************************/
void
cmdStanzaUpgrade(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Storage *storageRepoReadStanza = storageRepo();
        const Storage *storageRepoWriteStanza = storageRepoWrite();
        bool infoArchiveUpgrade = false;
        bool infoBackupUpgrade = false;

        // ??? Temporary until can communicate with PG: Get control info from the pgControlFile
        // CSHANG pgControlFromFile does not reach out to a remote db. May need to do get first but would still need to know the path to the control file - but we should be able to get that from the pg1-path - but that's where the dbObjectGet would come into play.
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

// CSHANG TODO:
// * how to handle --force
// * is it possible to reconstruct (only if not encrypted)- maybe only if backup.info exists - which means it would have to be the truthsayer
// * should we be comparing the db-id of the info files (e.g. archiveInfo.id != backupInfo.id then error that repo is corrupted)?

        // Load the info files (errors if missing)
        InfoArchive *infoArchive = infoArchiveNewLoad(
            storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

        InfoBackup *infoBackup = infoBackupNewLoad(
            storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

        // Update archive
        if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
        {
            infoArchivePgSet(infoArchive, pgControl.version, pgControl.systemId);
            infoArchiveUpgrade = true;
        }

        // Update backup
        if ((pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId) &&
            (pgControl.controlVersion != backupInfo.controlVersion || pgControl.catalogVersion != backupInfo.catalogVersion))
        {
            infoBackupPgSet(
                infoBackup, pgControl.version, pgControl.systemId, pgControl.controlVersion, pgControl.catalogVersion);
            infoBackupUpgrade = true;
        }

// CSHANG I added this to check the ids before saving the file because they need to match at this point
        // Get the backup and archive info pg data and ensure the ids match
        backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));
        archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));
        if (backupInfo.id != archiveInfo.id)
        {
            THROW(FileInvalidError, "backup info file or archive info file invalid\n"
                "HINT: this may be a symptom of database or repository corruption!\n"
                "HINT: delete the stanza and run stanza-create again");
        }
        else
        {
            // Save archive info
            if (infoArchiveUpgrade)
            {
                infoArchiveSave(
                    infoArchive, storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            }

            // Save backup info
            if (infoBackupUpgrade)
            {
                infoBackupSave(
                    infoBackup, storageRepoWriteStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            }
        }

        if (!(infoArchiveUpgrade || infoBackupUpgrade))
        {
            LOG_INFO("the stanza data is already up to date");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
