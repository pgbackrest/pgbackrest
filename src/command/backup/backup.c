/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/archive/find.h"
#include "command/backup/backup.h"
#include "command/backup/common.h"
#include "command/backup/protocol.h"
#include "command/check/common.h"
#include "command/control/common.h"
#include "command/lock.h"
#include "command/stanza/common.h"
#include "common/compress/helper.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/filter/size.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/time.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "config/common.h"
#include "config/config.h"
#include "config/parse.h"
#include "db/helper.h"
#include "info/infoArchive.h"
#include "info/infoBackup.h"
#include "info/manifest/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"
#include "version.h"

/***********************************************************************************************************************************
Get the postgres database and storage objects
***********************************************************************************************************************************/
#define FUNCTION_LOG_BACKUP_DATA_TYPE                                                                                              \
    BackupData *
#define FUNCTION_LOG_BACKUP_DATA_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "BackupData", buffer, bufferSize)

typedef struct BackupData
{
    unsigned int pgIdxPrimary;                                      // cfgOptGrpPg index of the primary
    Db *dbPrimary;                                                  // Database connection to the primary
    const Storage *storagePrimary;                                  // Storage object for the primary
    const String *hostPrimary;                                      // Host name of the primary

    unsigned int pgIdxStandby;                                      // cfgOptGrpPg index of the standby
    Db *dbStandby;                                                  // Database connection to the standby
    const Storage *storageStandby;                                  // Storage object for the standby
    const String *hostStandby;                                      // Host name of the standby

    const InfoArchive *archiveInfo;                                 // Archive info
    const String *archiveId;                                        // Archive where backup WAL will be stored

    unsigned int timeline;                                          // Primary timeline
    unsigned int version;                                           // PostgreSQL version
    unsigned int walSegmentSize;                                    // PostgreSQL wal segment size
    PgPageSize pageSize;                                            // PostgreSQL page size
} BackupData;

static BackupData *
backupInit(const InfoBackup *const infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(infoBackup != NULL);

    // Initialize for offline backup
    BackupData *const result = memNew(sizeof(BackupData));
    *result = (BackupData){0};

    // Don't allow backup from standby when offline
    unsigned int backupStandby = cfgOptionSeq(cfgOptBackupStandby);
    const InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));

    if (!cfgOptionBool(cfgOptOnline) && backupStandby != CFGOPTVAL_BACKUP_STANDBY_N)
    {
        LOG_WARN(
            "option " CFGOPT_BACKUP_STANDBY " is enabled but backup is offline - backups will be performed from the primary");
        backupStandby = CFGOPTVAL_BACKUP_STANDBY_N;
    }

    // Get database info when online
    PgControl pgControl = {0};

    if (cfgOptionBool(cfgOptOnline))
    {
        const DbGetResult dbInfo = dbGet(backupStandby == CFGOPTVAL_BACKUP_STANDBY_N, true, backupStandby);

        // If no standby was found but using the primary is allowed then warn and proceed
        if (backupStandby == CFGOPTVAL_BACKUP_STANDBY_PREFER && dbInfo.standby == NULL)
            LOG_WARN("unable to find a standby to perform the backup, using primary instead");

        result->pgIdxPrimary = dbInfo.primaryIdx;
        result->dbPrimary = dbInfo.primary;

        if (dbInfo.standby != NULL)
        {
            ASSERT(dbInfo.standby != NULL);

            result->pgIdxStandby = dbInfo.standbyIdx;
            result->dbStandby = dbInfo.standby;
            result->storageStandby = storagePgIdx(result->pgIdxStandby);
            result->hostStandby = cfgOptionIdxStrNull(cfgOptPgHost, result->pgIdxStandby);
        }

        // Get pg_control info from the primary
        pgControl = dbPgControl(result->dbPrimary);
    }
    // Else get pg_control info directly from the file
    else
        pgControl = pgControlFromFile(storagePgIdx(result->pgIdxPrimary), cfgOptionStrNull(cfgOptPgVersionForce));

    // Add primary info
    result->storagePrimary = storagePgIdx(result->pgIdxPrimary);
    result->hostPrimary = cfgOptionIdxStrNull(cfgOptPgHost, result->pgIdxPrimary);

    result->timeline = pgControl.timeline;
    result->version = pgControl.version;
    result->walSegmentSize = pgControl.walSegmentSize;
    result->pageSize = pgControl.pageSize;

    // Validate pg_control info against the stanza
    if (result->version != infoPg.version || pgControl.systemId != infoPg.systemId)
    {
        THROW_FMT(
            BackupMismatchError,
            PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64 "\n"
            "HINT: is this the correct stanza?", strZ(pgVersionToStr(pgControl.version)), pgControl.systemId,
            strZ(pgVersionToStr(infoPg.version)), infoPg.systemId);
    }

    // If checksum page is not explicitly set then automatically enable it when checksums are available
    if (!cfgOptionTest(cfgOptChecksumPage))
    {
        // If online then use the value in pg_control to set checksum-page
        if (cfgOptionBool(cfgOptOnline))
        {
            cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, VARBOOL(pgControl.pageChecksumVersion != 0));
        }
        // Else set to false. An offline cluster is likely to have false positives so better if the user enables manually.
        else
            cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
    }
    // Else if checksums have been explicitly enabled but are not available then warn and reset. ??? We should be able to make this
    // determination when offline as well, but the integration tests don't write pg_control accurately enough to support it.
    else if (cfgOptionBool(cfgOptOnline) && pgControl.pageChecksumVersion == 0 && cfgOptionBool(cfgOptChecksumPage))
    {
        LOG_WARN(CFGOPT_CHECKSUM_PAGE " option set to true but checksums are not enabled on the cluster, resetting to false");
        cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
    }

    // Get archive info
    if (cfgOptionBool(cfgOptArchiveCheck))
    {
        result->archiveInfo = infoArchiveLoadFile(
            storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));
        result->archiveId = infoArchiveId(result->archiveInfo);
    }

    FUNCTION_LOG_RETURN(BACKUP_DATA, result);
}

// {uncrustify_off - order required for C includes}
#include "command/backup/label.c.inc"
#include "command/backup/option.c.inc"
#include "command/backup/incr.c.inc"
#include "command/backup/resume.c.inc"
#include "command/backup/db.c.inc"
#include "command/backup/complete.c.inc"
#include "command/backup/process.c.inc"
// {uncrustify_on}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdBackup(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the repo option was not provided and more than one repo is configured, then log the default repo chosen
        if (!cfgOptionTest(cfgOptRepo) && cfgOptionGroupIdxTotal(cfgOptGrpRepo) > 1)
        {
            LOG_INFO_FMT(
                "repo option not specified, defaulting to %s",
                cfgOptionGroupName(cfgOptGrpRepo, cfgOptionGroupIdxDefault(cfgOptGrpRepo)));
        }

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Load backup.info
        InfoBackup *const infoBackup = infoBackupLoadFileReconstruct(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType), cfgOptionStrNull(cfgOptRepoCipherPass));
        const InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
        const String *const cipherPassBackup = infoPgCipherPass(infoBackupPg(infoBackup));

        // Get pg storage and database objects
        BackupData *const backupData = backupInit(infoBackup);

        // Get the start timestamp which will later be written into the manifest to track total backup time
        const time_t timestampStart = backupTime(backupData, false);

        // Check if there is a prior manifest when backup type is diff/incr
        Manifest *const manifestPrior = backupBuildIncrPrior(infoBackup);

        // Start the backup
        const BackupStartResult backupStartResult = backupStart(backupData);

        // Build the manifest
        const ManifestBlockIncrMap blockIncrMap = backupBlockIncrMap();

        Manifest *const manifest = manifestNewBuild(
            backupData->storagePrimary, infoPg.version, infoPg.catalogVersion, timestampStart, cfgOptionBool(cfgOptOnline),
            cfgOptionBool(cfgOptChecksumPage), cfgOptionBool(cfgOptRepoBundle), cfgOptionBool(cfgOptRepoBlock), &blockIncrMap,
            strLstNewVarLst(cfgOptionLst(cfgOptExclude)), backupStartResult.tablespaceList);

        // Validate the manifest using the copy start time
        manifestBuildValidate(
            manifest, cfgOptionBool(cfgOptDelta), backupTime(backupData, true),
            compressTypeEnum(cfgOptionStrId(cfgOptCompressType)));

        // Build an incremental backup if type is not full (manifestPrior will be freed in this call)
        if (!backupBuildIncr(infoBackup, manifest, manifestPrior, backupStartResult.walSegmentName))
            manifestCipherSubPassSet(manifest, cipherPassGen(cfgOptionStrId(cfgOptRepoCipherType)));

        // Set delta if it is not already set and the manifest requires it
        if (!cfgOptionBool(cfgOptDelta) && varBool(manifestData(manifest)->backupOptionDelta))
            cfgOptionSet(cfgOptDelta, cfgSourceParam, BOOL_TRUE_VAR);

        // Resume a backup when possible
        if (!backupResume(manifest, cipherPassBackup))
        {
            manifestBackupLabelSet(
                manifest,
                backupLabelCreate(
                    (BackupType)cfgOptionStrId(cfgOptType), manifestData(manifest)->backupLabelPrior, timestampStart));
        }

        // Save the manifest before processing starts
        backupManifestSaveCopy(manifest, cipherPassBackup, false);

        // Process the backup manifest
        backupProcess(backupData, manifest, cipherPassBackup);

        // Check that the clusters are alive and correctly configured after the backup
        backupDbPing(backupData, true);

        // The standby db object and protocol won't be used anymore so free them
        if (backupData->dbStandby != NULL)
        {
            dbFree(backupData->dbStandby);
            protocolHelperFree(protocolRemoteGet(protocolStorageTypePg, backupData->pgIdxStandby, false));
        }

        // Stop the backup
        const BackupStopResult backupStopResult = backupStop(backupData, manifest);

        // Complete manifest
        manifestBuildComplete(
            manifest, backupStartResult.lsn, backupStartResult.walSegmentName, backupStopResult.timestamp, backupStopResult.lsn,
            backupStopResult.walSegmentName, infoPg.id, infoPg.systemId, backupStartResult.dbList,
            cfgOptionBool(cfgOptArchiveCheck), cfgOptionBool(cfgOptArchiveCopy), cfgOptionUInt(cfgOptBufferSize),
            cfgOptionUInt(cfgOptCompressLevel), cfgOptionUInt(cfgOptCompressLevelNetwork), cfgOptionBool(cfgOptRepoHardlink),
            cfgOptionUInt(cfgOptProcessMax), backupData->dbStandby != NULL,
            cfgOptionTest(cfgOptAnnotation) ? cfgOptionKv(cfgOptAnnotation) : NULL);

        // The primary db object won't be used anymore so free it
        dbFree(backupData->dbPrimary);

        // Check and copy WAL segments required to make the backup consistent
        backupArchiveCheckCopy(backupData, manifest, cipherPassBackup);

        // The primary protocol connection won't be used anymore so free it. This needs to happen after backupArchiveCheckCopy() so
        // the backup lock is held on the remote which allows conditional archiving based on the backup lock. Any further access to
        // the primary storage object may result in an error (likely eof).
        protocolHelperFree(protocolRemoteGet(protocolStorageTypePg, backupData->pgIdxPrimary, false));

        // Complete the backup
        LOG_INFO_FMT("new backup label = %s", strZ(manifestData(manifest)->backupLabel));
        backupComplete(infoBackup, manifest);

        // Backup info
        LOG_INFO_FMT(
            "%s backup size = %s, file total = %u", zNewStrId(manifestData(manifest)->backupType),
            strZ(strSizeFormat(infoBackupDataByLabel(infoBackup, manifestData(manifest)->backupLabel)->backupInfoSizeDelta)),
            manifestFileTotal(manifest));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
