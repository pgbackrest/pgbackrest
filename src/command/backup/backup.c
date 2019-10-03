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
#include "common/type/json.h" // !!! TRY TO REMOVE
#include "config/config.h"
// #include "config/exec.h"
#include "db/helper.h"
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
Get the postgres database and storage objects
***********************************************************************************************************************************/
#define FUNCTION_LOG_BACKUP_PG_TYPE                                                                                                \
    BackupPg
#define FUNCTION_LOG_BACKUP_PG_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(&value, "BackupPg", buffer, bufferSize)

typedef struct BackupPg
{
    const Storage *storagePrimary;
    const Db *dbStandby;
} BackupPg;

static BackupPg
backupPgGet(const InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    // !!! PRETTY BADLY FAKED FOR NOW, SHOULD BE pgGet() KINDA THING
    BackupPg result = {.storagePrimary = storagePgId(1)};

    // Get control information from the primary and validate it against backup info
    InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
    PgControl pgControl = pgControlFromFile(result.storagePrimary);

    if (pgControl.version != infoPg.version || pgControl.systemId != infoPg.systemId)
    {
        THROW_FMT(
            BackupMismatchError,
            PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
            strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(infoPg.version)),
            infoPg.systemId);
    }

    // If backup from standby option is set but a standby was not configured in the config file or on the command line, then turn
    // off backup-standby and warn that backups will be performed from the prinary.
    if (result.dbStandby == NULL && cfgOptionBool(cfgOptBackupStandby))
    {
        cfgOptionSet(cfgOptBackupStandby, cfgSourceParam, BOOL_FALSE_VAR);
        LOG_WARN(
            "option " CFGOPT_BACKUP_STANDBY " is enabled but standby is not properly configured - backups will be performed from"
            " the primary");
    }

    FUNCTION_LOG_RETURN(BACKUP_PG, result);
}

/***********************************************************************************************************************************
Check for a prior backup and promote to full if diff/incr and a prior backup does not exist
***********************************************************************************************************************************/
static const Manifest *
backupPrior(const InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        BackupType type = backupType(cfgOptionStr(cfgOptType));
        InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
        const String *backupLabelPrior = NULL;

        if (type != backupTypeFull)
        {
            unsigned int backupTotal = infoBackupDataTotal(infoBackup);

            for (unsigned int backupIdx = backupTotal - 1; backupIdx < backupTotal; backupIdx--)
            {
                 InfoBackupData backupPrior = infoBackupData(infoBackup, backupIdx);

                 // The prior backup for a diff must be full
                 if (type == backupTypeDiff && backupType(backupPrior.backupType) != backupTypeFull)
                    continue;

                // The backups must come from the same cluster
                if (infoPg.id != backupPrior.backupPgId)
                    continue;

                // This backup is a candidate for prior
                backupLabelPrior = strDup(backupPrior.backupLabel);
                break;
            }

            // If there is a prior backup then check that options for the new backup are compatible
            if (backupLabelPrior != NULL)
            {
                result = manifestLoadFile(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabelPrior)),
                    cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));
                const ManifestData *manifestPriorData = manifestData(result);

                LOG_INFO(
                    "last backup label = %s, version = %s", strPtr(backupLabelPrior), strPtr(manifestPriorData->backrestVersion));

                // Warn if compress option changed
                if (cfgOptionBool(cfgOptCompress) != manifestPriorData->backupOptionCompress)
                {
                    LOG_WARN(
                        "%s backup cannot alter compress option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptCompress)), strPtr(backupLabelPrior));
                    cfgOptionSet(cfgOptCompress, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionCompress));
                }

                // Warn if hardlink option changed
                if (cfgOptionBool(cfgOptRepoHardlink) != manifestPriorData->backupOptionHardLink)
                {
                    LOG_WARN(
                        "%s backup cannot alter hardlink option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptRepoHardlink)), strPtr(backupLabelPrior));
                    cfgOptionSet(cfgOptRepoHardlink, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionHardLink));
                }
            }
            else
            {
                LOG_WARN("no prior backup exists, %s backup has been changed to full", strPtr(cfgOptionStr(cfgOptType)));
                cfgOptionSet(cfgOptType, cfgSourceParam, VARSTR(backupTypeStr(type)));
            }
        }

        manifestMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

/***********************************************************************************************************************************
Check for a halted backup that can be resumed
***********************************************************************************************************************************/
static const Manifest *
backupHalted(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        // Loop through all backups in the repo in reverse
        for (unsigned int backupIdx = 0; backupIdx < strLstSize(backupList); backupIdx++)
        {
            const String *backupLabel = strLstGet(backupList, backupIdx);
            const String *manifestFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabel));

            // Halted backups have a copy of the manifest but no main
            if (storageExistsNP(storageRepo(), strNewFmt("%s" INFO_COPY_EXT, strPtr(manifestFile))) &&
                !storageExistsNP(storageRepo(), manifestFile))
            {
                // bool usable = false;
                const String *reason = STRDEF("resume is disabled");

                // Attempt to read the manifest file in the halted backup to see if it can be used.  If any error at all occurs then
                // the backup will be considered unusable and a resume will not be attempted.
                if (cfgOptionBool(cfgOptResume))
                {
        //         $strBackupPath = storageRepo()->pathGet(STORAGE_REPO_BACKUP . "/${strAbortedBackup}");
        //
        //             $strReason = "unable to read ${strBackupPath}/" . FILE_MANIFEST;
        //
        //             eval
        //             {
        //                 # Load the aborted manifest
        //                 $oAbortedManifest = new pgBackRest::Manifest("${strBackupPath}/" . FILE_MANIFEST,
        //                     {strCipherPass => $oBackupInfo->cipherPassSub()});
        //
        //                 # Key and values that do not match
        //                 my $strKey;
        //                 my $strValueNew;
        //                 my $strValueAborted;
        //
        //                 # Check version
        //                 if ($oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION) ne PROJECT_VERSION)
        //                 {
        //                     $strKey =  INI_KEY_VERSION;
        //                     $strValueNew = PROJECT_VERSION;
        //                     $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_VERSION);
        //                 }
        //                 # Check format
        //                 elsif ($oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT) ne REPOSITORY_FORMAT)
        //                 {
        //                     $strKey =  INI_KEY_FORMAT;
        //                     $strValueNew = REPOSITORY_FORMAT;
        //                     $strValueAborted = $oAbortedManifest->get(INI_SECTION_BACKREST, INI_KEY_FORMAT);
        //                 }
        //                 # Check backup type
        //                 elsif ($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE) ne cfgOption(CFGOPT_TYPE))
        //                 {
        //                     $strKey =  MANIFEST_KEY_TYPE;
        //                     $strValueNew = cfgOption(CFGOPT_TYPE);
        //                     $strValueAborted = $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_TYPE);
        //                 }
        //                 # Check prior label
        //                 elsif ($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>') ne
        //                        (defined($strBackupLastPath) ? $strBackupLastPath : '<undef>'))
        //                 {
        //                     $strKey =  MANIFEST_KEY_PRIOR;
        //                     $strValueNew = defined($strBackupLastPath) ? $strBackupLastPath : '<undef>';
        //                     $strValueAborted =
        //                         $oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_PRIOR, undef, false, '<undef>');
        //                 }
        //                 # Check compression
        //                 elsif ($oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS) !=
        //                        cfgOption(CFGOPT_COMPRESS))
        //                 {
        //                     $strKey = MANIFEST_KEY_COMPRESS;
        //                     $strValueNew = cfgOption(CFGOPT_COMPRESS);
        //                     $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_COMPRESS);
        //                 }
        //                 # Check hardlink
        //                 elsif ($oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK) !=
        //                        cfgOption(CFGOPT_REPO_HARDLINK))
        //                 {
        //                     $strKey = MANIFEST_KEY_HARDLINK;
        //                     $strValueNew = cfgOption(CFGOPT_REPO_HARDLINK);
        //                     $strValueAborted = $oAbortedManifest->boolGet(MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_HARDLINK);
        //                 }
        //
        //                 # If key is defined then something didn't match
        //                 if (defined($strKey))
        //                 {
        //                     $strReason = "new ${strKey} '${strValueNew}' does not match aborted ${strKey} '${strValueAborted}'";
        //                 }
        //                 # Else the backup can be resumed
        //                 else
        //                 {
        //                     $bUsable = true;
        //                 }
        //
        //                 return true;
        //             }
        //             or do
        //             {
        //                 $bUsable = false;
        //             }
        //         }
        //
        //         # If the backup is usable then set the backup label
        //         if ($bUsable)
        //         {
        //             $strBackupLabel = $strAbortedBackup;
        //
        //             # If the repo is encrypted, set the backup set passphrase from this manifest
        //             if (defined($oBackupInfo->cipherPassSub()))
        //             {
        //                 $strCipherPassBackupSet = $oAbortedManifest->cipherPassSub();
        //             }
        //
        //             # Get the archive segment timeline for determining if a timeline switch has occurred. Only defined for prior online
        //             # backup.
        //             if ($oAbortedManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP))
        //             {
        //                 $strTimelineAborted = substr($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_STOP), 0, 8);
        //             }
        //             elsif ($oAbortedManifest->test(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START))
        //             {
        //                 $strTimelineAborted = substr($oAbortedManifest->get(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START), 0, 8);
        //             }
                }
                else
                {
                    LOG_WARN("halted backup %s cannot be resumed: %s", strPtr(backupLabel), strPtr(reason));
                    // &log(TEST, TEST_BACKUP_NORESUME);

                    storagePathRemoveP(
                        storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(backupLabel)), .recurse = true);
                }

                break;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

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
        InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));

        // Get pg storage and database objects
        backupPgGet(infoBackup);

        // Get the prior manifest if one exists
        const Manifest *manifestPrior = backupPrior(infoBackup);

        // Check for a halted backup
        const Manifest *manifestHalted = backupHalted();
        (void)manifestHalted;

        // !!! BELOW NEEDED FOR PERL MIGRATION

        // Parameters that must be passed to Perl during migration
        KeyValue *paramKv = kvNew();
        kvPut(paramKv, VARSTRDEF("timestampStart"), VARUINT64((uint64_t)timestampStart));
        kvPut(paramKv, VARSTRDEF("pgId"), VARUINT(infoPg.id));
        kvPut(paramKv, VARSTRDEF("pgVersion"), VARSTR(pgVersionToStr(infoPg.version)));
        kvPut(paramKv, VARSTRDEF("pgSystemId"), VARUINT64(infoPg.systemId));
        kvPut(paramKv, VARSTRDEF("pgControlVersion"), VARUINT(pgControlVersion(infoPg.version)));
        kvPut(paramKv, VARSTRDEF("pgCatalogVersion"), VARUINT(pgCatalogVersion(infoPg.version)));
        kvPut(paramKv, VARSTRDEF("backupLabelPrior"), manifestPrior ? VARSTR(manifestData(manifestPrior)->backupLabel) : NULL);

        StringList *paramList = strLstNew();
        strLstAdd(paramList, jsonFromVar(varNewKv(paramKv), 0));
        cfgCommandParamSet(paramList);

        // Do this so Perl does not need to reconstruct backup.info
        infoBackupSaveFile(
            infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        // Shutdown protocol so Perl can take locks
        protocolFree();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
