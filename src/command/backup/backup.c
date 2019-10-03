/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <string.h>
// #include <sys/stat.h>
#include <time.h>
// #include <unistd.h>
//
#include "command/control/common.h"
#include "command/backup/backup.h"
#include "command/backup/common.h"
// #include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
// #include "common/regExp.h"
// #include "common/user.h"
#include "common/type/convert.h"
#include "config/config.h"
// #include "config/exec.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
// #include "protocol/parallel.h"
#include "storage/helper.h"
// #include "storage/write.intern.h"
// #include "version.h"

/***********************************************************************************************************************************
Make a backup
***********************************************************************************************************************************/
void
cmdBackup(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Get the start timestamp which will later be written into the manifest to track total backup time
    time_t timestampStart = time(NULL);

    // Verify the repo is local
    repoIsLocalVerify();

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load backup.info
        InfoBackup *infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));

        // Get the primary pg storage !!! HACKED TO ONE FOR NOW UNTIL WE BRING IN THE DB LOGIC
        const Storage *storagePgPrimary = storagePgId(1);

        // Get control information from the primary and validate it against backup info
        PgControl pgControl = pgControlFromFile(storagePgPrimary);

        if (pgControl.version != infoPg.version || pgControl.systemId != infoPg.systemId)
        {
            THROW_FMT(
                BackupMismatchError,
                PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
                strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(infoPg.version)),
                infoPg.systemId);
        }

        // Validate the backup type
        BackupType type = backupType(cfgOptionStr(cfgOptType));
        const String *backupLabelPrior = NULL;
        unsigned int backupTimelinePrior = NULL;
        const String *backupCipherSubPass = NULL;

        if (type != backupTypeFull)
        {
            unsigned int backupTotal = infoBackupDataTotal(infoBackup);
            // InfoBackupData backupPrior = {};

            // $strBackupLastPath = $oBackupInfo->last(
            //     $strType eq CFGOPTVAL_BACKUP_TYPE_DIFF ? CFGOPTVAL_BACKUP_TYPE_FULL : CFGOPTVAL_BACKUP_TYPE_INCR);

            for (unsigned int backupIdx = backupTotal - 1; backupIdx < backupTotal; backupIdx--)
            {
                 InfoBackupData backupTest = infoBackupData(infoBackup, backupIdx);

                 // The prior backup for a diff must be full
                 if (type == backupTypeDiff && backupType(backupTest.backupType) != backupTypeFull)
                    continue;

                // The backups must come from the same cluster
                if (infoPg.id != backupTest.backupPgId)
                    continue;

                // This backup is a candidate for prior
                backupLabelPrior = strDup(backupTest.backupLabel);
                break;
            }

            // If there is a prior backup then check that options for the new backup are compatible
            if (backupLabelPrior != NULL)
            {
                const Manifest *manifestPrior = manifestLoadFile(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabelPrior)),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));
                const ManifestData *manifestPriorData = manifestData(manifestPrior);

                // Get the passphrase from the manifest (if set).  The same passphrase is used for the entire backup set.
                backupCipherSubPass = manifestCipherSubPass(manifestPrior);

                // Get archive segment timeline for determining if a timeline switch has occurred. Only defined for online backup.
                if (manifestPriorData->archiveStop != NULL)
                    backupTimelinePrior = cvtZToUInt(strPtr(strSubN(manifestPriorData->archiveStop, 0, 8)));

                LOG_INFO("last backup label = %s, version = 2.19dev", strPtr(backupLabelPrior) /* !!! ADD REAL VERSION */);

                // Warn if compress option changed
                if (cfgOptionBool(cfgOptCompress) != manifestPriorData->backupOptionCompress)
                {
                    LOG_WARN(
                        "%s backup cannot alter compress option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptCompress)), strPtr(backupLabelPrior));
                    // cfgOptionSet(cfgOptCompress, cfgSourceParam, VARBOOL(cfgOptionBool(cfgOptCompress)));
                }

                // Warn if hardlink option changed
                if (cfgOptionBool(cfgOptRepoHardlink) != manifestPriorData->backupOptionHardLink)
                {
                    LOG_WARN(
                        "%s backup cannot alter hardlink option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptRepoHardlink)), strPtr(backupLabelPrior));
                    // cfgOptionSet(cfgOptRepoHardlink, cfgSourceParam, VARBOOL(cfgOptionBool(cfgOptRepoHardlink)));
                }
            }
            else
            {
                LOG_WARN("no prior backup exists, %s backup has been changed to full", strPtr(cfgOptionStr(cfgOptType)));
                type = backupTypeFull;
                // cfgOptionSet(cfgOptType, cfgSourceParam, VARSTR(backupTypeStr(type)));
            }
        }

        // !!! BELOW NEEDED FOR PERL MIGRATION

        // Parameters that must be passed to Perl during migration
        StringList *paramList = strLstNew();
        strLstAdd(paramList, strNewFmt("%" PRIu64, (uint64_t)timestampStart));
        strLstAdd(paramList, strNewFmt("%u", infoPg.id));
        strLstAdd(paramList, pgVersionToStr(infoPg.version));
        strLstAdd(paramList, strNewFmt("%" PRIu64, infoPg.systemId));
        strLstAdd(paramList, strNewFmt("%u", pgControlVersion(infoPg.version)));
        strLstAdd(paramList, strNewFmt("%u", pgCatalogVersion(infoPg.version)));
        strLstAdd(paramList, backupLabelPrior ? backupLabelPrior : STRDEF("?"));
        strLstAdd(paramList, backupCipherSubPass ? backupCipherSubPass : STRDEF("?"));
        strLstAdd(paramList, strNewFmt("%u", backupTimelinePrior));
        cfgCommandParamSet(paramList);

        // Shutdown protocol so Perl can take locks
        protocolFree();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
