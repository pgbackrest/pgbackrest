/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/stanza/stanzaCreate.h"
#include "common/debug.h"
#include "common/encode.h" // CSHANG Is this necessary?
#include "common/encode/base64.h" // CSHANG Is this necessary?
#include "common/io/handleWrite.h"  // CSHANG Is this necessary?
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"  // CSHANG Is this necessary?
#include "postgres/version.h"  // CSHANG Is this necessary?
#include "storage/helper.h"

/***********************************************************************************************************************************
Process stanza-create
***********************************************************************************************************************************/
void
cmdStanzaCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // ??? Temporary until can communicate with PG: Get control info from the pgControlFile
        // CSHANG pgControlFromFile does not reach out to a remote db. May need to do get first but would still need to know the path to the control file - but we should be able to get that from the pg1-path - but that's where the dbObjectGet would come into play.
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        const Storage *storageRepoStanzaRead = storageRepo();
        InfoArchive *infoArchive = NULL;
        InfoBackup *infoBackup = NULL;

// CSHANG TODO:
// * how to handle --force - especially if something exists and what if encryption reset?
// * is it possible to reconstruct (only if not encrypted)- maybe only if backup.info exists - which means it would have to be the truthsayer
// From the old code: # If something other than the info files exist in the repo (maybe a backup is in progress) and the user is attempting to
// # change the repo encryption in anyway, then error
        bool archiveInfoFileExists = storageExistsNP(storageRepoStanzaRead, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE));
        bool archiveInfoFileCopyExists = storageExistsNP(
            storageRepoStanzaRead, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT));
        bool backupInfoFileExists = storageExistsNP(storageRepoStanzaRead, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE));
        bool backupInfoFileCopyExists = storageExistsNP(
            storageRepoStanzaRead, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT));

        // If neither archive info nor backup info files exist and nothing else exists in the stanza directory
        // then create the stanza
        if (!archiveInfoFileExists && !archiveInfoFileCopyExists && !backupInfoFileExists && !backupInfoFileCopyExists)
        {
            bool archiveNotEmpty = strLstSize(
                storageListNP(storageRepoStanzaRead, STRDEF(STORAGE_REPO_ARCHIVE))) > 0 ? true : false;
            bool backupNotEmpty = strLstSize(
                storageListNP(storageRepoStanzaRead, STRDEF(STORAGE_REPO_BACKUP))) > 0 ? true : false;

            // If something exists in the backup or archive directories for this stanza, then error
            if (archiveNotEmpty || backupNotEmpty)
            {
                THROW_FMT(
                    PathNotEmptyError, "%s%s%snot empty", (backupNotEmpty ? "backup directory " : ""),
                    (backupNotEmpty && archiveNotEmpty ? "and/or " : ""), (archiveNotEmpty ? "archive directory " : ""));
            }

            // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent files
            if (cipherType(cfgOptionStr(cfgOptRepoCipherType)) != cipherTypeNone)
            {
                unsigned char buffer[48]; // 48 is the amount of entropy needed to get a 64 base key
                cryptoRandomBytes(buffer, sizeof(buffer));
                char cipherPassSubChar[64];
                encodeToStr(encodeBase64, buffer, sizeof(buffer), cipherPassSubChar);
                cipherPassSub = strNew(cipherPassSubChar);
            }

            // Create and save archive info
            infoArchive = infoArchiveNew(
                pgControl.version, pgControl.systemId, cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoArchiveSave(
                infoArchive, storageRepoWrite(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

            // Create and save backup info
            infoBackup = infoBackupNew(
                pgControl.version, pgControl.systemId, pgControl.controlVersion, pgControl.catalogVersion,
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoBackupSave(
                infoBackup, storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        }
        // Else if both info files exist and both are valid, resave
        else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
        {
            infoArchive = infoArchiveNewLoad(
                storageRepoStanzaRead, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

            infoBackup = infoBackupNewLoad(
                storageRepoStanzaRead, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

// CSHANG In the mock tests, the database  version is checked during a backup - so we need to make sure that it is also performed there
            // If the versions or system ids don't match in either file, then an upgrade may be necessary
            if ((pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId) &&
                (pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId))
            {
                THROW(FileInvalidError, "backup info file or archive info file invalid\n"
                    "HINT: use stanza-upgrade if the database has been upgraded");
// CSHANG This used to read "or use --force"
// confess &log(ERROR, "backup info file or archive info file invalid\n" .
//     'HINT: use stanza-upgrade if the database has been upgraded or use --force', ERROR_FILE_INVALID);
            }
            else if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
            {
                THROW_FMT(ArchiveMismatchError,
                    "database version = %s, system-id %" PRIu64 " does not match archive version = %s, system-id = %" PRIu64 "\n"
                    "HINT: is this the correct stanza?",
                    strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(archiveInfo.version)),
                    archiveInfo.systemId);
            }
            else if (pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId)
            {
                THROW_FMT(BackupMismatchError,
                    "database version = %s, system-id %" PRIu64 " does not match backup version = %s, system-id = %" PRIu64 "\n"
                    "HINT: is this the correct stanza?",
                    strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(backupInfo.version)), backupInfo.systemId);
            }

            // If the versions and system ids match but the control or catalog doesn't then there may be corruption
            if (pgControl.controlVersion != backupInfo.controlVersion || pgControl.catalogVersion != backupInfo.catalogVersion)
            {
                THROW_FMT(BackupMismatchError,
                    "database control-version = %" PRIu32 ", catalog-version %" PRIu32
                    " does not match backup control-version = %" PRIu32 ", catalog-version = %" PRIu32 "\n"
                    "HINT: this may be a symptom of database or repository corruption!",
                    pgControl.controlVersion, pgControl.catalogVersion, backupInfo.controlVersion, backupInfo.catalogVersion);
            }
// CSHANG I added this because I feel we should also check the db-ids before saving since we don't want them to get out of whack
            // At this point, everything should match so make sure the ids do as well
            if (backupInfo.id != archiveInfo.id)
            {
                THROW(FileInvalidError, "backup info file or archive info file invalid\n"
                    "HINT: this may be a symptom of database or repository corruption!\n"
                    "HINT: delete the stanza and run stanza-create again");
            }
            else
            {
                // If the existing files are valid, resave to ensure there are two files (info and a copy)
                infoArchiveSave(
                    infoArchive, storageRepoWrite(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
                infoBackupSave(
                    infoBackup, storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

                LOG_INFO("stanza-create was already performed");
            }
        }
        // Else if one file is missing, then error
        else
        {
// CSHANG We need to figure out if we want to error or try to create one info file from the other
            THROW_FMT(FileMissingError, "%s\n"
                "HINT: this may be a symptom of database or repository corruption!\n"
                "HINT: delete the stanza and run stanza-create again",
                ((archiveInfoFileExists || archiveInfoFileCopyExists) ? "archive.info exists but backup.info is missing"
                : "backup.info exists but archive.info is missing"));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
