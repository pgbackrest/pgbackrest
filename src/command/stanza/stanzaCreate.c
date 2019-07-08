/***********************************************************************************************************************************
Stanza Create Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "command/stanza/stanzaCreate.h"
#include "common/debug.h"
#include "common/encode.h"
#include "common/encode/base64.h"
#include "common/io/handleWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/info.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Callback function for StorageInfoList
***********************************************************************************************************************************/


/***********************************************************************************************************************************
Render the information for the stanza based on the command parameters.
***********************************************************************************************************************************/
// static String *
// infoRender(void)
// {
//     FUNCTION_LOG_VOID(logLevelDebug);
//
//     String *result = NULL;
//
//     MEM_CONTEXT_TEMP_BEGIN()
//     {
//
//
//         memContextSwitch(MEM_CONTEXT_OLD());
//         result = strDup(resultStr);
//         memContextSwitch(MEM_CONTEXT_TEMP());
//     }
//     MEM_CONTEXT_TEMP_END();
//
//     FUNCTION_LOG_RETURN(STRING, result);
// }

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
// * storageInfoList
// * how to handle --force
// * is it possible to reconstruct - maybe only if backup.info exists - which means it would have to be the truthsayer
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
            // CSHANG Need to call storageInfoList because need to know if anything is in the directory at all - but if just passing a path and is S3 but the path is missing, it seems to throw a [111] Connection refused when running test:
            // TEST_RESULT_VOID(
            //     storageInfoListNP(s3, strNew("/cshang"), testStorageInfoListCallback, (void *)memContextCurrent()), "info cshang list files");
            // Maybe it is where I was running the test which was after the /path/to "info list files" test. If I replace
            //         TEST_RESULT_VOID(
            //             storageInfoListNP(s3, strNew("/path/to"), testStorageInfoListCallback, (void *)memContextCurrent()), "info list files");
            // with
            //         TEST_RESULT_VOID(
            //             storageInfoListNP(s3, strNew("/cshang"), testStorageInfoListCallback, (void *)memContextCurrent()), "info list files");
            // then it just hangs for a long time until the timeout which then returns ASSERT: [025]: timeout after 540 seconds waiting for process to complete

            String *cipherPassSub = NULL;

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
        // Else if both info files exist (in some form), then if valid just return
        else if ((archiveInfoFileExists || archiveInfoFileCopyExists) && (backupInfoFileExists || backupInfoFileCopyExists))
        {
            infoArchive = infoArchiveNewLoad(
                storageRepoStanzaRead, STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData archiveInfo = infoPgData(infoArchivePg(infoArchive), infoPgDataCurrentId(infoArchivePg(infoArchive)));

            if (pgControl.version != archiveInfo.version || pgControl.systemId != archiveInfo.systemId)
            {
                THROW_FMT(ArchiveMismatchError,
                    "database version = %s, system-id %" PRIu64 " does not match archive version = %s, system-id = %" PRIu64 "\n"
                    "HINT: is this the correct stanza?",
                    strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(archiveInfo.version)),
                    archiveInfo.systemId);
            }

            infoBackup = infoBackupNewLoad(
                storageRepoStanzaRead, STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE),
                cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            InfoPgData backupInfo = infoPgData(infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

            if (pgControl.version != backupInfo.version || pgControl.systemId != backupInfo.systemId)
            {
                THROW_FMT(BackupMismatchError,
                    "database version = %s, system-id %" PRIu64 " does not match backup version = %s, system-id = %" PRIu64 "\n"
                    "HINT: is this the correct stanza?",
                    strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(backupInfo.version)), backupInfo.systemId);
            }

            if (pgControl.controlVersion != backupInfo.controlVersion || pgControl.catalogVersion != backupInfo.catalogVersion)
            {
                THROW_FMT(BackupMismatchError,
                    "database control-version = %" PRIu32 ", catalog-version %" PRIu32
                    " does not match backup control-version = %" PRIu32 ", catalog-version = %" PRIu32 "\n"
                    "HINT: this may be a symptom of database or repository corruption!",
                    pgControl.controlVersion, pgControl.catalogVersion, backupInfo.controlVersion, backupInfo.catalogVersion);
            }

            // If the existing files are valid, resave to ensure there are two files (info and a copy)
            infoArchiveSave(
                infoArchive, storageRepoWrite(), STRDEF(STORAGE_REPO_ARCHIVE "/" INFO_ARCHIVE_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
            infoBackupSave(
                infoBackup, storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" INFO_BACKUP_FILE), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));
        }
        else
        {
            THROW_FMT(FileMissingError, "%s - the repository is corrupted\n"
                "HINT: delete the stanza and run stanza-create again",
                ((archiveInfoFileExists || archiveInfoFileCopyExists) ? "archive.info exists but backup.info is missing"
                : "backup.info exists but archive.info is missing"));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
