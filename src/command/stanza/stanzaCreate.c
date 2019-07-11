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
        const Storage *storageRepoReadStanza = storageRepo();
        const Storage *storageRepoWriteStanza = storageRepoWrite();
        const String *archiveInfoFile = STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE);
        const String *archiveInfoFileCopy = STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE INFO_COPY_EXT);
        const String *backupInfoFile = STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE);
        const String *backupInfoFileCopy = STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE INFO_COPY_EXT);
        InfoArchive *infoArchive = NULL;
        InfoBackup *infoBackup = NULL;

        // ??? Temporary until can communicate with PG: Get control info from the pgControlFile
        // CSHANG pgControlFromFile does not reach out to a remote db. May need to do get first but would still need to know the path to the control file - but we should be able to get that from the pg1-path - but that's where the dbObjectGet would come into play.
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        bool archiveInfoFileExists = storageExistsNP(storageRepoReadStanza, archiveInfoFile);
        bool archiveInfoFileCopyExists = storageExistsNP(storageRepoReadStanza, archiveInfoFileCopy);
        bool backupInfoFileExists = storageExistsNP(storageRepoReadStanza, backupInfoFile);
        bool backupInfoFileCopyExists = storageExistsNP(storageRepoReadStanza, backupInfoFileCopy);

        // If neither archive info nor backup info files exist and nothing else exists in the stanza directory
        // then create the stanza
        if (!archiveInfoFileExists && !archiveInfoFileCopyExists && !backupInfoFileExists && !backupInfoFileCopyExists)
        {
            bool archiveNotEmpty = strLstSize(
                storageListNP(storageRepoReadStanza, STRDEF(STORAGE_REPO_ARCHIVE))) > 0 ? true : false;
            bool backupNotEmpty = strLstSize(
                storageListNP(storageRepoReadStanza, STRDEF(STORAGE_REPO_BACKUP))) > 0 ? true : false;

            // If something exists in the backup or archive directories for this stanza, then error
            if (archiveNotEmpty || backupNotEmpty)
            {
                THROW_FMT(
                    PathNotEmptyError, "%s%s%snot empty", (backupNotEmpty ? "backup directory " : ""),
                    (backupNotEmpty && archiveNotEmpty ? "and/or " : ""), (archiveNotEmpty ? "archive directory " : ""));
            }

            // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent files
            String *cipherPassSub = NULL;
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
                infoArchive, storageRepoWriteStanza, archiveInfoFile, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));

            // Create and save backup info
            infoBackup = infoBackupNew(
                pgControl.version, pgControl.systemId, pgControl.controlVersion, pgControl.catalogVersion,
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoBackupSave(
                infoBackup, storageRepoWriteStanza, backupInfoFile, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }
        // Else if at least one archive and one backup info file exists and then ensure both are valid
        else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
        {
            infoArchive = infoArchiveNewLoad(
                storageRepoReadStanza, archiveInfoFile,
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

            infoBackup = infoBackupNewLoad(
                storageRepoReadStanza, backupInfoFile,
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

            // Error if there is a mismatch between the archive and backup info files
            if (archiveInfo.id != backupInfo.id || archiveInfo.systemId != backupInfo.systemId ||
                archiveInfo.version != backupInfo.version)
            {
                THROW_FMT(
                    FileInvalidError, "backup info file and archive info file do not match\n"
                    "archive: id=%u, version=%s, system-id=%" PRIu64 "\n"
                    "backup: id=%u, version=%s, system-id=%" PRIu64 "\n"
                    "HINT: this may be a symptom of repository corruption!",
                    archiveInfo.id, strPtr(pgVersionToStr(archiveInfo.version)), archiveInfo.systemId, backupInfo.id,
                    strPtr(pgVersionToStr(backupInfo.version)), backupInfo.systemId);
            }
            // If the versions or system ids don't match the database, then an upgrade may be necessary
            else if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
            {
                THROW(FileInvalidError, "backup and archive info files already exist but do not match the database\n"
                    "HINT: is this the correct stanza?\n"
                    "HINT: did an error occur during stanza-upgrade?");
            }
            // Else the files are valid
            else
            {
                String *sourceFile = NULL;
                String *destinationFile = NULL;

                // If the existing files are valid, then, if a file is missing, copy the existing one to the missing one to ensure
                // there is both a .info and .info.copy
                if ((archiveInfoFileExists && !archiveInfoFileCopyExists) || (!archiveInfoFileExists && archiveInfoFileCopyExists))
                {
                    sourceFile = archiveInfoFileExists ? archiveInfoFile : archiveInfoFileCopy;
                    destinationFile = !archiveInfoFileExists ? archiveInfoFile : archiveInfoFileCopy;
                    storageCopyNP(
                        storageNewReadNP(storageRepoReadStanza, sourceFile),
                        storageNewWriteNP(storageRepoWriteStanza, destinationFile));
                }

                if ((backupInfoFileExists && !backupInfoFileCopyExists) || (!backupInfoFileExists && backupInfoFileCopyExists))
                {
                    sourceFile = backupInfoFileExists ? backupInfoFile : backupInfoFileCopy;
                    destinationFile = !backupInfoFileExists ? backupInfoFile : backupInfoFileCopy;
                    storageCopyNP(
                        storageNewReadNP(storageRepoReadStanza, sourceFile),
                        storageNewWriteNP(storageRepoWriteStanza, destinationFile));
                }

                // If no files, copied, then the stanza was already valid
                if (sourceFile == NULL && destinationFile == NULL)
                    LOG_INFO("stanza already exists and is valid");
            }
        }
        // Else if both .info and corresponding .copy file are missing for one but not the other, then error
        else
        {
            THROW_FMT(FileMissingError, "%s\n"
                "HINT: this may be a symptom of repository corruption!",
                ((archiveInfoFileExists || archiveInfoFileCopyExists) ? "archive.info exists but backup.info is missing"
                : "backup.info exists but archive.info is missing"));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
