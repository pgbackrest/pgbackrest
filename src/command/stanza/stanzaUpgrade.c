/***********************************************************************************************************************************
Stanza Update Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/stanza/stanzaUpgrade.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

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

        // Load the info files (errors if missing)
        InfoArchive *infoArchive = infoArchiveNewLoad(
            storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

        InfoBackup *infoBackup = infoBackupNewLoad(
            storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

        // Since the file save of archive.info and backup.info are not atomic, then check and update each as separately.
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

        // Get the backup and archive info pg data and ensure the ids match before saving (even if only one needed to be updated)
        backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));
        archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));
        if (backupInfo.id != archiveInfo.id)
        {
            THROW_FMT(
                FileInvalidError, "backup info file and archive info file do not match\n"
                "archive: id=%u, version=%s, system-id=%" PRIu64 "\n"
                "backup: id=%u, version=%s, system-id=%" PRIu64 "\n"
                "HINT: this may be a symptom of repository corruption!",
                archiveInfo.id, strPtr(pgVersionToStr(archiveInfo.version)), archiveInfo.systemId, backupInfo.id,
                strPtr(pgVersionToStr(backupInfo.version)), backupInfo.systemId);
        }
        else
        {
            // Save archive info
            if (infoArchiveUpgrade)
            {
                infoArchiveSave(
                    infoArchive, storageRepoWriteStanza, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
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
            LOG_INFO("stanza is already up to date");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
