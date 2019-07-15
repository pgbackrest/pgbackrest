/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/stanza/common.h"
#include "command/stanza/stanzaCreate.h"
#include "common/debug.h"
#include "common/encode.h"
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
        InfoArchive *infoArchive = NULL;
        InfoBackup *infoBackup = NULL;

        // ??? Temporary until can communicate with PG: Get control info from the pgControlFile
        // CSHANG pgControlFromFile does not reach out to a remote db. May need to do get first but would still need to know the path to the control file - but we should be able to get that from the pg1-path - but that's where the dbObjectGet would come into play.
        PgControl pgControl = pgControlFromFile(cfgOptionStr(cfgOptPgPath));

        bool archiveInfoFileExists = storageExistsNP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR);
        bool archiveInfoFileCopyExists = storageExistsNP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_COPY_STR);
        bool backupInfoFileExists = storageExistsNP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR);
        bool backupInfoFileCopyExists = storageExistsNP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_COPY_STR);

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
                infoArchive, storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));

            // Create and save backup info
            infoBackup = infoBackupNew(
                pgControl.version, pgControl.systemId, pgControl.controlVersion, pgControl.catalogVersion,
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherPassSub);
            infoBackupSave(
                infoBackup, storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
        }
        // Else if at least one archive and one backup info file exists, then ensure both are valid
        else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
        {
            infoArchive = infoArchiveNewLoad(
                storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

            infoBackup = infoBackupNewLoad(
                storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

            // Error if there is a mismatch between the archive and backup info files
            infoValidate(&archiveInfo, &backupInfo);

            // The archive and backup info files match so check if the versions or system ids match the current database,
            // if not, then an upgrade may be necessary
            if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
            {
                THROW(FileInvalidError, "backup and archive info files already exist but do not match the database\n"
                    "HINT: is this the correct stanza?\n"
                    "HINT: did an error occur during stanza-upgrade?");
            }
            // Else the files are valid
            else
            {
                const String *sourceFile = NULL;
                const String *destinationFile = NULL;

                // If the existing files are valid, then, if a file is missing, copy the existing one to the missing one to ensure
                // there is both a .info and .info.copy
                if (!archiveInfoFileExists || !archiveInfoFileCopyExists)
                {
                    sourceFile = archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;
                    destinationFile = !archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;
                    storageCopyNP(
                        storageNewReadNP(storageRepoReadStanza, sourceFile),
                        storageNewWriteNP(storageRepoWriteStanza, destinationFile));
                }

                if (!backupInfoFileExists || !backupInfoFileCopyExists)
                {
                    sourceFile = backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;
                    destinationFile = !backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;
                    storageCopyNP(
                        storageNewReadNP(storageRepoReadStanza, sourceFile),
                        storageNewWriteNP(storageRepoWriteStanza, destinationFile));
                }

                // If no files copied, then the stanza was already valid
                if (sourceFile == NULL)
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
