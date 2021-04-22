/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/check/common.h"
#include "command/control/common.h"
#include "command/stanza/common.h"
#include "command/stanza/create.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
void
cmdStanzaCreate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Verify that a stop was not issued before proceeding
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (cfgOptionBool(cfgOptForce))
            LOG_WARN("option --" CFGOPT_FORCE " is no longer supported");

        // Get the version and system information - validating it if the database is online
        PgControl pgControl = pgValidate();

        // For each repository configured, create the stanza
        for (unsigned int repoIdx = 0; repoIdx < cfgOptionGroupIdxTotal(cfgOptGrpRepo); repoIdx++)
        {
            LOG_INFO_FMT(
                CFGCMD_STANZA_CREATE " for stanza '%s' on repo%u", strZ(cfgOptionStr(cfgOptStanza)),
                cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));

            const Storage *storageRepoReadStanza = storageRepoIdx(repoIdx);
            const Storage *storageRepoWriteStanza = storageRepoIdxWrite(repoIdx);
            InfoArchive *infoArchive = NULL;
            InfoBackup *infoBackup = NULL;

            bool archiveInfoFileExists = storageExistsP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_STR);
            bool archiveInfoFileCopyExists = storageExistsP(storageRepoReadStanza, INFO_ARCHIVE_PATH_FILE_COPY_STR);
            bool backupInfoFileExists = storageExistsP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_STR);
            bool backupInfoFileCopyExists = storageExistsP(storageRepoReadStanza, INFO_BACKUP_PATH_FILE_COPY_STR);

            // If neither archive info nor backup info files exist and nothing else exists in the stanza directory
            // then create the stanza
            if (!archiveInfoFileExists && !archiveInfoFileCopyExists && !backupInfoFileExists && !backupInfoFileCopyExists)
            {
                bool archiveNotEmpty = strLstSize(storageListP(storageRepoReadStanza, STORAGE_REPO_ARCHIVE_STR)) > 0 ? true : false;
                bool backupNotEmpty = strLstSize(storageListP(storageRepoReadStanza, STORAGE_REPO_BACKUP_STR)) > 0 ? true : false;

                // If something else exists in the backup or archive directories for this stanza, then error
                if (archiveNotEmpty || backupNotEmpty)
                {
                    THROW_FMT(
                        PathNotEmptyError, "%s%s%snot empty", (backupNotEmpty ? "backup directory " : ""),
                        (backupNotEmpty && archiveNotEmpty ? "and/or " : ""), (archiveNotEmpty ? "archive directory " : ""));
                }

                // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent archive files
                String *cipherPassSub = cipherPassGen(cfgOptionIdxStrid(cfgOptRepoCipherType, repoIdx));

                // Create and save archive info
                infoArchive = infoArchiveNew(pgControl.version, pgControl.systemId, cipherPassSub);

                infoArchiveSaveFile(
                    infoArchive, storageRepoWriteStanza, INFO_ARCHIVE_PATH_FILE_STR,
                    cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                // If the repo is encrypted, generate a cipher passphrase for encrypting subsequent backup files
                cipherPassSub = cipherPassGen(cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx));

                // Create and save backup info
                infoBackup = infoBackupNew(pgControl.version, pgControl.systemId, pgControl.catalogVersion, cipherPassSub);

                infoBackupSaveFile(
                    infoBackup, storageRepoWriteStanza, INFO_BACKUP_PATH_FILE_STR,
                    cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
            }
            // Else if at least one archive and one backup info file exists, then ensure both are valid
            else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
            {
                // Error if there is a mismatch between the archive and backup info files or the database version/system Id matches
                // current database
                checkStanzaInfoPg(
                    storageRepoReadStanza, pgControl.version, pgControl.systemId,
                    cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx), cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                // The files are valid - upgrade
                const String *sourceFile = NULL;
                const String *destinationFile = NULL;

                // If the existing files are valid, then, if a file is missing, copy the existing one to the missing one to ensure
                // there is both a .info and .info.copy
                if (!archiveInfoFileExists || !archiveInfoFileCopyExists)
                {
                    sourceFile = archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;
                    destinationFile = !archiveInfoFileExists ? INFO_ARCHIVE_PATH_FILE_STR : INFO_ARCHIVE_PATH_FILE_COPY_STR;

                    // Using get and put instead of copy in case the storage is remote
                    storagePutP(
                        storageNewWriteP(storageRepoWriteStanza, destinationFile),
                        storageGetP(storageNewReadP(storageRepoReadStanza, sourceFile)));
                }

                if (!backupInfoFileExists || !backupInfoFileCopyExists)
                {
                    sourceFile = backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;
                    destinationFile = !backupInfoFileExists ? INFO_BACKUP_PATH_FILE_STR : INFO_BACKUP_PATH_FILE_COPY_STR;

                    // Using get and put instead of copy in case the storage is remote
                    storagePutP(
                        storageNewWriteP(storageRepoWriteStanza, destinationFile),
                        storageGetP(storageNewReadP(storageRepoReadStanza, sourceFile)));
                }

                // If no files copied, then the stanza was already valid
                if (sourceFile == NULL)
                {
                    LOG_INFO_FMT(
                        "stanza '%s' already exists on repo%u and is valid", strZ(cfgOptionStr(cfgOptStanza)),
                        cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));
                }
            }
            // Else if both .info and corresponding .copy file are missing for one but not the other, then error - the user will
            // have to make a conscious effort to determine if deleting the stanza on this repo is appropriate or other action is
            else
            {
                THROW_FMT(
                    FileMissingError,
                    "%s on repo%u\n"
                        "HINT: this may be a symptom of repository corruption!",
                    ((archiveInfoFileExists || archiveInfoFileCopyExists) ?
                        "archive.info exists but backup.info is missing" : "backup.info exists but archive.info is missing"),
                    cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
