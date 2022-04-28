/***********************************************************************************************************************************
Annotate Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "command/annotate/annotate.h"
#include "command/backup/backup.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.h"
#include "common/crypto/cipherBlock.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
void
cmdAnnotate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Initialize the repo index
        unsigned int repoIdxMin = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxMin = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxMin;
        }

        // Check the backup label format
        const String *backupLabel = cfgOptionStr(cfgOptSet);
        if (!regExpMatchOne(backupRegExpP(.full = true, .differential = true, .incremental = true), backupLabel))
            THROW_FMT(OptionInvalidValueError, "'%s' is not a valid backup label format", strZ(backupLabel));

        // Track the number of backup manifests to update
        unsigned int foundManifests = 0;

        // Look through the repositories to find the requested backup set manifest
        String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            const Storage *storageRepo = storageRepoIdx(repoIdx);

            if (storageExistsP(storageRepo, manifestFileName))
            {
                LOG_INFO_FMT("backup set '%s' to annotate found in %s", strZ(backupLabel), cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

                // Load the backup.info to get the cipher pass
                const CipherType repoCipherType = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx);

                InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
                    storageRepo, INFO_BACKUP_PATH_FILE_STR, repoCipherType, cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                // Load the manifest file
                const String *cipherPassBackup = infoPgCipherPass(infoBackupPg(infoBackup));
                Manifest *manifest = manifestLoadFile(storageRepo, manifestFileName, repoCipherType, cipherPassBackup);
                foundManifests++;

                // Update annotations
                manifestAnnotationSet(manifest, cfgOptionKv(cfgOptAnnotation));

                // Write the updated manifest
                IoWrite *write = storageWriteIo(storageNewWriteP(storageRepoWrite(), manifestFileName));
                cipherBlockFilterGroupAdd(ioWriteFilterGroup(write), repoCipherType, cipherModeEncrypt, cipherPassBackup);
                manifestSave(manifest, write);
            }
        }

        if (foundManifests <= 0)
            THROW(BackupSetInvalidError, "no backup manifest to update found");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
