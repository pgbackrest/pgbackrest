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
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

/**********************************************************************************************************************************/
FN_EXTERN void
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
        const String *const backupLabel = cfgOptionStr(cfgOptSet);

        if (!regExpMatchOne(backupRegExpP(.full = true, .differential = true, .incremental = true), backupLabel))
            THROW_FMT(OptionInvalidValueError, "'%s' is not a valid backup label format", strZ(backupLabel));

        // Track the number of backup sets to update in the backup info file and any errors that may occur
        unsigned int backupTotalProcessed = 0;
        unsigned int errorTotal = 0;

        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            TRY_BEGIN()
            {
                // Attempt to load the backup info file
                const CipherType repoCipherType = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx);

                InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
                    storageRepoIdx(repoIdx), INFO_BACKUP_PATH_FILE_STR, repoCipherType,
                    cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));

                if (infoBackupLabelExists(infoBackup, backupLabel))
                {
                    // Backup label found in backup.info
                    backupTotalProcessed++;

                    LOG_INFO_FMT(
                        "backup set '%s' to annotate found in %s",
                        strZ(backupLabel), cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

                    // Update annotations
                    infoBackupDataAnnotationSet(infoBackup, backupLabel, cfgOptionKv(cfgOptAnnotation));

                    // Write the updated backup info
                    infoBackupSaveFile(
                        infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, repoCipherType,
                        cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
                }
            }
            CATCH_ANY()
            {
                LOG_ERROR_FMT(errorTypeCode(errorType()), "%s: %s", cfgOptionGroupName(cfgOptGrpRepo, repoIdx), errorMessage());
                errorTotal++;
            }
            TRY_END();
        }

        // Error if any errors encountered on one or more repos
        if (errorTotal > 0)
            THROW_FMT(CommandError, CFGCMD_ANNOTATE " command encountered %u error(s), check the log file for details", errorTotal);

        if (backupTotalProcessed == 0)
            THROW(BackupSetInvalidError, "no backup set to annotate found");
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
