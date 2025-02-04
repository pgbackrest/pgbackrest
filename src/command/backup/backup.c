/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/archive/find.h"
#include "command/backup/backup.h"
#include "command/backup/common.h"
#include "command/backup/file.h"
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
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"
#include "version.h"

/**********************************************************************************************************************************
Generate a unique backup label that does not contain a timestamp from a previous backup
***********************************************************************************************************************************/
static String *
backupLabelCreate(const BackupType type, const String *const backupLabelPrior, const time_t timestamp)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, backupLabelPrior);
        FUNCTION_LOG_PARAM(TIME, timestamp);
    FUNCTION_LOG_END();

    ASSERT((type == backupTypeFull && backupLabelPrior == NULL) || (type != backupTypeFull && backupLabelPrior != NULL));
    ASSERT(timestamp > 0);

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *backupLabelLatest = NULL;

        // Get the newest backup
        const StringList *const backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        if (!strLstEmpty(backupList))
            backupLabelLatest = strLstGet(backupList, 0);

        // Get the newest history
        const StringList *const historyYearList = strLstSort(
            storageListP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY), .expression = STRDEF("^2[0-9]{3}$")),
            sortOrderDesc);

        if (!strLstEmpty(historyYearList))
        {
            // For full backup compare against all backups in the history. For other backup types find backups whose name begins
            // with full backup part of backupLabelLatest. This prevents label generation from failing when the last full backup is
            // removed and then an diff/incr is generated from the last remaining full backup.
            const String *const fileNameRegExp =
                (type == backupTypeFull) ?
                    backupRegExpP(.full = true, .differential = true, .incremental = true, .noAnchorEnd = true) :
                    strNewFmt("^%.*sF\\_" DATE_TIME_REGEX "(D|I)", DATE_TIME_LEN, strZ(backupLabelLatest));
            const StringList *const historyList = strLstSort(
                storageListP(
                    storageRepo(),
                    strNewFmt(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY "/%s", strZ(strLstGet(historyYearList, 0))),
                    .expression = strNewFmt("%s\\.manifest\\.%s$", strZ(fileNameRegExp), strZ(compressTypeStr(compressTypeGz)))),
                sortOrderDesc);

            if (!strLstEmpty(historyList))
            {
                const String *const historyLabelLatest = strLstGet(historyList, 0);

                if (backupLabelLatest == NULL || strCmp(historyLabelLatest, backupLabelLatest) > 0)
                {
                    // Strip off the compression and manifest extensions in case this ends up in an error message
                    backupLabelLatest = compressExtStrip(historyLabelLatest, compressTypeFromName(historyLabelLatest));
                    backupLabelLatest = strSubN(backupLabelLatest, 0, strSize(backupLabelLatest) - sizeof(BACKUP_MANIFEST_EXT) + 1);
                }
            }
        }

        // Now that we have the latest label check if the provided timestamp will give us an even later label
        result = backupLabelFormat(type, backupLabelPrior, timestamp);

        if (backupLabelLatest != NULL && strCmp(result, backupLabelLatest) <= 0)
        {
            // If that didn't give us a later label then add one second. It's possible that two backups (they would need to be
            // offline or halted online) have run very close together.
            result = backupLabelFormat(type, backupLabelPrior, timestamp + 1);

            // If the label is still not latest then error. There is probably a timezone change or massive clock skew.
            if (strCmp(result, backupLabelLatest) <= 0)
            {
                THROW_FMT(
                    ClockError,
                    "new backup label '%s' is not later than latest backup label '%s'\n"
                    "HINT: has the timezone changed?\n"
                    "HINT: is there clock skew?",
                    strZ(result), strZ(backupLabelLatest));
            }

            // If adding a second worked then sleep the remainder of the current second so we don't start early
            sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(result);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

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
    uint64_t checkpoint;                                            // Last checkpoint LSN
    time_t checkpointTime;                                          // Last checkpoint time
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
    StringId backupStandby = cfgOptionStrId(cfgOptBackupStandby);
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
    result->checkpoint = pgControl.checkpoint;
    result->checkpointTime = pgControl.checkpointTime;
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

/**********************************************************************************************************************************
Build block incremental maps
***********************************************************************************************************************************/
// Size map. Block size is increased when the block map would be larger than a single block. The break can be calculated with this
// formula: [block size in KiB] / (1024 / [block size in KiB] * [checksum size]) * 1073741824.
static const ManifestBlockIncrSizeMap manifestBlockIncrSizeMapDefault[] =
{
    {.fileSize = 914 * 1024 * 1024, .blockSize = 88 * 1024},
    {.fileSize = 740 * 1024 * 1024, .blockSize = 80 * 1024},
    {.fileSize = 585 * 1024 * 1024, .blockSize = 72 * 1024},
    {.fileSize = 448 * 1024 * 1024, .blockSize = 64 * 1024},
    {.fileSize = 329 * 1024 * 1024, .blockSize = 56 * 1024},
    {.fileSize = 228 * 1024 * 1024, .blockSize = 48 * 1024},
    {.fileSize = 146 * 1024 * 1024, .blockSize = 40 * 1024},
    {.fileSize = 96 * 1024 * 1024, .blockSize = 32 * 1024},
    {.fileSize = 43 * 1024 * 1024, .blockSize = 24 * 1024},
    {.fileSize = 11 * 1024 * 1024, .blockSize = 16 * 1024},
    {.fileSize = 16 * 1024, .blockSize = 8 * 1024},
};

// Age map
static const ManifestBlockIncrAgeMap manifestBlockIncrAgeMapDefault[] =
{
    {.fileAge = 4 * 7 * SEC_PER_DAY, .blockMultiplier = 0},
    {.fileAge = 2 * 7 * SEC_PER_DAY, .blockMultiplier = 4},
    {.fileAge = 7 * SEC_PER_DAY, .blockMultiplier = 2},
};

// Checksum size map
static const ManifestBlockIncrChecksumSizeMap manifestBlockIncrChecksumSizeMapDefault[] =
{
    {.blockSize = 4 * 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 6},
    {.blockSize = 2 * 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 5},
    {.blockSize = 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 4},
    {.blockSize = 512 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 3},
    {.blockSize = 128 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 2},
    {.blockSize = 32 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 1},
};

// All maps
static const ManifestBlockIncrMap manifestBlockIncrMap =
{
    .sizeMap = manifestBlockIncrSizeMapDefault,
    .sizeMapSize = LENGTH_OF(manifestBlockIncrSizeMapDefault),
    .ageMap = manifestBlockIncrAgeMapDefault,
    .ageMapSize = LENGTH_OF(manifestBlockIncrAgeMapDefault),
    .checksumSizeMap = manifestBlockIncrChecksumSizeMapDefault,
    .checksumSizeMapSize = LENGTH_OF(manifestBlockIncrChecksumSizeMapDefault),
};

// Convert map size
static unsigned int
backupBlockIncrMapSize(const ConfigOption optionId, const unsigned int optionKeyIdx, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    int64_t result;

    TRY_BEGIN()
    {
        result = cfgParseSize(value);

        // Error if value is out of range (no need for an error message since that will be generated in the catch block)
        if (result <= 0 || result > UINT_MAX)
            THROW(OptionInvalidValueError, "");
    }
    CATCH_ANY()
    {
        THROW_FMT(
            OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(value),
            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
    }
    TRY_END();

    ASSERT(result > 0 && result < UINT_MAX);

    FUNCTION_TEST_RETURN(UINT, (unsigned int)result);
}

// Convert map checksum size
static unsigned int
backupBlockIncrMapChecksumSize(const ConfigOption optionId, const unsigned int optionKeyIdx, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    TRY_BEGIN()
    {
        result = varUIntForce(value);
    }
    CATCH_ANY()
    {
    }
    TRY_END();

    if (result < BLOCK_INCR_CHECKSUM_SIZE_MIN)
    {
        THROW_FMT(
            OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(varStr(value)),
            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

static ManifestBlockIncrMap
backupBlockIncrMap(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_AUDIT_HELPER();

    ManifestBlockIncrMap result = manifestBlockIncrMap;

    if (cfgOptionBool(cfgOptRepoBlock))
    {
        // Build size map
        const KeyValue *const manifestBlockIncrSizeKv = cfgOptionKvNull(cfgOptRepoBlockSizeMap);

        if (manifestBlockIncrSizeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrSizeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrSizeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrSizeMap manifestBuildBlockIncrSizeMap =
                {
                    .fileSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockSizeMap), varStr(mapKey)),
                    .blockSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockSizeMap),
                        varStr(kvGet(manifestBlockIncrSizeKv, mapKey))),
                };

                lstAdd(map, &manifestBuildBlockIncrSizeMap);
            }

            lstSort(map, sortOrderDesc);

            result.sizeMap = lstGet(map, 0);
            result.sizeMapSize = lstSize(map);
        }

        // Build age map
        const KeyValue *const manifestBlockIncrAgeKv = cfgOptionKvNull(cfgOptRepoBlockAgeMap);

        if (manifestBlockIncrAgeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrAgeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrAgeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrAgeMap manifestBuildBlockIncrAgeMap =
                {
                    .fileAge = (unsigned int)(varUIntForce(mapKey) * SEC_PER_DAY),
                    .blockMultiplier = varUIntForce(kvGet(manifestBlockIncrAgeKv, mapKey)),
                };

                lstAdd(map, &manifestBuildBlockIncrAgeMap);
            }

            lstSort(map, sortOrderDesc);

            result.ageMap = lstGet(map, 0);
            result.ageMapSize = lstSize(map);
        }

        // Build checksum size map
        const KeyValue *const manifestBlockIncrChecksumSizeKv = cfgOptionKvNull(cfgOptRepoBlockChecksumSizeMap);

        if (manifestBlockIncrChecksumSizeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrChecksumSizeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrChecksumSizeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *const mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrChecksumSizeMap manifestBuildBlockIncrChecksumSizeMap =
                {
                    .blockSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockChecksumSizeMap), varStr(mapKey)),
                    .checksumSize = backupBlockIncrMapChecksumSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockChecksumSizeMap),
                        kvGet(manifestBlockIncrChecksumSizeKv, mapKey)),
                };

                lstAdd(map, &manifestBuildBlockIncrChecksumSizeMap);
            }

            lstSort(map, sortOrderDesc);

            result.checksumSizeMap = lstGet(map, 0);
            result.checksumSizeMapSize = lstSize(map);
        }
    }

    FUNCTION_TEST_RETURN_TYPE(ManifestBlockIncrMap, result);
}

/**********************************************************************************************************************************
Get time from the database or locally depending on online
***********************************************************************************************************************************/
static time_t
backupTime(const BackupData *const backupData, const bool waitRemainder)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(BOOL, waitRemainder);
    FUNCTION_LOG_END();

    // Offline backups will just grab the time from the local system since the value of copyStart is not important in this context.
    // No worries about causing a delta backup since switching online will do that anyway.
    time_t result = time(NULL);

    // When online get the time from the database server
    if (cfgOptionBool(cfgOptOnline))
    {
        // Get time from the database
        TimeMSec timeMSec = dbTimeMSec(backupData->dbPrimary);
        result = (time_t)(timeMSec / MSEC_PER_SEC);

        // Sleep the remainder of the second when requested (this is so copyStart is not subject to one second resolution issues)
        if (waitRemainder)
        {
            unsigned int retry = 0;

            // Just to be safe we'll loop until PostgreSQL reports that we have slept long enough
            do
            {
                // Error if the clock has not advanced after several attempts
                if (retry == 3)
                    THROW_FMT(KernelError, PG_NAME " clock has not advanced to the next second after %u tries", retry);

                // Sleep remainder of current second
                sleepMSec(((TimeMSec)(result + 1) * MSEC_PER_SEC) - timeMSec);

                // Check time again to be sure we slept long enough
                timeMSec = dbTimeMSec(backupData->dbPrimary);

                // Increment retry to prevent an infinite loop
                retry++;
            }
            while ((time_t)(timeMSec / MSEC_PER_SEC) <= result);
        }
    }

    FUNCTION_LOG_RETURN(TIME, result);
}

/***********************************************************************************************************************************
Create an incremental backup if type is not full and a compatible prior backup exists
***********************************************************************************************************************************/
// Helper to find a compatible prior backup
static Manifest *
backupBuildIncrPrior(const InfoBackup *const infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    Manifest *result = NULL;

    // No incremental if backup type is full
    const BackupType type = (BackupType)cfgOptionStrId(cfgOptType);

    if (type != backupTypeFull)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
            const String *backupLabelPrior = NULL;
            const unsigned int backupTotal = infoBackupDataTotal(infoBackup);

            for (unsigned int backupIdx = backupTotal - 1; backupIdx < backupTotal; backupIdx--)
            {
                const InfoBackupData backupPrior = infoBackupData(infoBackup, backupIdx);

                // The prior backup for a diff must be full
                if (type == backupTypeDiff && backupPrior.backupType != backupTypeFull)
                    continue;

                // The backups must come from the same cluster ??? This should enable delta instead
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
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabelPrior)),
                    cfgOptionStrId(cfgOptRepoCipherType), infoPgCipherPass(infoBackupPg(infoBackup)));
                const ManifestData *const manifestPriorData = manifestData(result);

                LOG_INFO_FMT(
                    "last backup label = %s, version = %s", strZ(manifestData(result)->backupLabel),
                    strZ(manifestData(result)->backrestVersion));

                // Warn if compress-type option changed
                if (compressTypeEnum(cfgOptionStrId(cfgOptCompressType)) != manifestPriorData->backupOptionCompressType)
                {
                    LOG_WARN_FMT(
                        "%s backup cannot alter " CFGOPT_COMPRESS_TYPE " option to '%s', reset to value in %s",
                        strZ(cfgOptionDisplay(cfgOptType)), strZ(cfgOptionDisplay(cfgOptCompressType)), strZ(backupLabelPrior));

                    // Set the compression type back to whatever was in the prior backup. This is not strictly needed since we could
                    // store compression type on a per file basis, but it seems simplest and safest for now.
                    cfgOptionSet(
                        cfgOptCompressType, cfgSourceParam, VARSTR(compressTypeStr(manifestPriorData->backupOptionCompressType)));

                    // There's a small chance that the prior manifest is old enough that backupOptionCompressLevel was not recorded.
                    // There's an even smaller chance that the user will also alter compression-type in this scenario right after
                    // upgrading to a newer version. Because we judge this combination of events to be nearly impossible just check
                    // here so no test coverage is needed.
                    CHECK(
                        FormatError, manifestPriorData->backupOptionCompressLevel != NULL,
                        "compress-level missing in prior manifest");

                    // Set the compression level back to whatever was in the prior backup
                    cfgOptionSet(
                        cfgOptCompressLevel, cfgSourceParam, VARINT64(varUInt(manifestPriorData->backupOptionCompressLevel)));
                }

                // If not defined this backup was done in a version prior to page checksums being introduced. Just set checksum-page
                // to false and move on without a warning. Page checksums will start on the next full backup.
                if (manifestData(result)->backupOptionChecksumPage == NULL)
                {
                    cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
                }
                // Don't allow the checksum-page option to change in a diff or incr backup. This could be confusing as only certain
                // files would be checksummed and the list could be incomplete during reporting.
                else
                {
                    const bool checksumPagePrior = varBool(manifestData(result)->backupOptionChecksumPage);

                    // Warn if an incompatible setting was explicitly requested
                    if (checksumPagePrior != cfgOptionBool(cfgOptChecksumPage))
                    {
                        LOG_WARN_FMT(
                            "%s backup cannot alter '" CFGOPT_CHECKSUM_PAGE "' option to '%s', reset to '%s' from %s",
                            strZ(cfgOptionDisplay(cfgOptType)), strZ(cfgOptionDisplay(cfgOptChecksumPage)),
                            cvtBoolToConstZ(checksumPagePrior), strZ(manifestData(result)->backupLabel));
                    }

                    cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, VARBOOL(checksumPagePrior));
                }

                manifestMove(result, memContextPrior());
            }
            else
            {
                LOG_WARN_FMT("no prior backup exists, %s backup has been changed to full", strZ(cfgOptionDisplay(cfgOptType)));
                cfgOptionSet(cfgOptType, cfgSourceParam, VARUINT64(backupTypeFull));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupBuildIncr(
    const InfoBackup *const infoBackup, Manifest *const manifest, Manifest *const manifestPrior, const String *const archiveStart)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST, manifestPrior);
        FUNCTION_LOG_PARAM(STRING, archiveStart);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    bool result = false;

    // Build the incremental if there is a prior manifest -- except when backup type is full, which indicates a full/incr backup
    // and is handled elsewhere
    if (manifestPrior != NULL && cfgOptionStrId(cfgOptType) != backupTypeFull)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Move the manifest to this context so it will be freed when we are done
            manifestMove(manifestPrior, MEM_CONTEXT_TEMP());

            // Build incremental manifest
            manifestBuildIncr(manifest, manifestPrior, (BackupType)cfgOptionStrId(cfgOptType), archiveStart);

            // Set the cipher subpass from prior manifest since we want a single subpass for the entire backup set
            manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestPrior));

            // Incremental was built
            result = true;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get size of files to be copied in a manifest
***********************************************************************************************************************************/
static uint64_t
backupManifestCopySize(Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    uint64_t result = 0;

    for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
    {
        const ManifestFile file = manifestFile(manifest, fileIdx);

        if (file.copy)
            result += file.size;
    }

    FUNCTION_LOG_RETURN(UINT64, result);
}

/***********************************************************************************************************************************
Get the last full backup time in order to set the limit for full/incr preliminary copy
***********************************************************************************************************************************/
static time_t
backupFullIncrLimit(const InfoBackup *const infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Default to one day if no full backup can be found
    time_t result = SEC_PER_DAY;

    // Get the limit from the last full backup if it exists
    for (unsigned int backupIdx = infoBackupDataTotal(infoBackup) - 1; backupIdx + 1 > 0; backupIdx--)
    {
        InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);

        if (backupData.backupType == backupTypeFull)
        {
            result = backupData.backupTimestampStop - backupData.backupTimestampStart;
            break;
        }
    }

    // Round up to the nearest minute (ensures we do not have a zero limit). This is a bit imprecise since an interval exactly
    // divisible by a minute will be rounded up another minute, but it seems fine for this purpose.
    result = (result / SEC_PER_MIN + 1) * SEC_PER_MIN;

    FUNCTION_LOG_RETURN(TIME, result);
}

/***********************************************************************************************************************************
Check for a backup that can be resumed and merge into the manifest if found
***********************************************************************************************************************************/
// Recursive helper for backupResumeClean()
static void
backupResumeCleanRecurse(
    StorageIterator *const storageItr, Manifest *const manifest, const Manifest *const manifestResume,
    const CompressType compressType, const bool delta, const bool resume, const String *const backupParentPath,
    const String *const manifestParentName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, storageItr);           // Storage info
        FUNCTION_LOG_PARAM(MANIFEST, manifest);                     // New manifest
        FUNCTION_LOG_PARAM(MANIFEST, manifestResume);               // Resumed manifest
        FUNCTION_LOG_PARAM(ENUM, compressType);                     // Backup compression type
        FUNCTION_LOG_PARAM(BOOL, delta);                            // Is this a delta backup?
        FUNCTION_LOG_PARAM(BOOL, resume);                           // Should resume checking be done (not needed for full)?
        FUNCTION_LOG_PARAM(STRING, backupParentPath);               // Path to the current level of the backup being cleaned
        FUNCTION_LOG_PARAM(STRING, manifestParentName);             // Parent manifest name used to construct manifest name
    FUNCTION_LOG_END();

    ASSERT(storageItr != NULL);
    ASSERT(manifest != NULL);
    ASSERT(manifestResume != NULL);
    ASSERT(backupParentPath != NULL);

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Skip backup.manifest.copy -- it must be preserved to allow resume again if this process throws an error before
            // writing the manifest for the first time
            if (manifestParentName == NULL && strEqZ(info.name, BACKUP_MANIFEST_FILE INFO_COPY_EXT))
                continue;

            // Build the name used to lookup files in the manifest
            const String *manifestName =
                manifestParentName != NULL ? strNewFmt("%s/%s", strZ(manifestParentName), strZ(info.name)) : info.name;

            // Build the backup path used to remove files/links/paths that are invalid
            const String *const backupPath = strNewFmt("%s/%s", strZ(backupParentPath), strZ(info.name));

            // Add/resume resumed based on resume flag
            const char *resumeZ = resume ? " resumed" : "";

            // Process file types
            switch (info.type)
            {
                // Check paths
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypePath:
                {
                    // If the path was not found in the new manifest then remove it
                    if (manifestPathFindDefault(manifest, manifestName, NULL) == NULL)
                    {
                        LOG_DETAIL_FMT("remove path '%s' from%s backup", strZ(storagePathP(storageRepo(), backupPath)), resumeZ);
                        storagePathRemoveP(storageRepoWrite(), backupPath, .recurse = true);
                    }
                    // Else recurse into the path
                    else
                    {
                        backupResumeCleanRecurse(
                            storageNewItrP(storageRepo(), backupPath, .sortOrder = sortOrderAsc), manifest, manifestResume,
                            compressType, delta, resume, backupPath, manifestName);
                    }

                    break;
                }

                // Check files
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeFile:
                {
                    // If the file is compressed then strip off the extension before doing the lookup
                    const CompressType fileCompressType = compressTypeFromName(manifestName);

                    if (fileCompressType != compressTypeNone)
                        manifestName = compressExtStrip(manifestName, fileCompressType);

                    // If the file is block incremental then strip off the extension before doing the lookup
                    const bool blockIncr = strEndsWithZ(manifestName, BACKUP_BLOCK_INCR_EXT);

                    if (blockIncr)
                        manifestName = strSubN(manifestName, 0, strSize(manifestName) - (sizeof(BACKUP_BLOCK_INCR_EXT) - 1));

                    // Check if the file can be resumed or must be removed
                    const char *removeReason = NULL;

                    if (fileCompressType != compressType && !blockIncr)
                        removeReason = "mismatched compression type";
                    else if (!manifestFileExists(manifest, manifestName))
                        removeReason = "missing in manifest";
                    else
                    {
                        ManifestFile file = manifestFileFind(manifest, manifestName);
                        ASSERT(file.reference == NULL);

                        if (!manifestFileExists(manifestResume, manifestName))
                            removeReason = zNewFmt("missing in%s manifest", resumeZ);
                        else
                        {
                            const ManifestFile fileResume = manifestFileFind(manifestResume, manifestName);
                            ASSERT(fileResume.reference == NULL);

                            if (fileResume.checksumSha1 == NULL)
                                removeReason = zNewFmt("no checksum in%s manifest", resumeZ);
                            else if (file.size != fileResume.size)
                                removeReason = "mismatched size";
                            else if (!delta && file.timestamp != fileResume.timestamp)
                                removeReason = "mismatched timestamp";
                            else if (file.size == 0)
                                // ??? don't resume zero size files because Perl wouldn't -- can be removed after the migration)
                                removeReason = "zero size";
                            else
                            {
                                ASSERT(file.copy);
                                ASSERT(file.bundleId == 0);
                                ASSERT(file.blockIncrMapSize == 0);

                                file.sizeRepo = fileResume.sizeRepo;
                                file.checksumSha1 = fileResume.checksumSha1;
                                file.checksumRepoSha1 = fileResume.checksumRepoSha1;
                                file.blockIncrSize = fileResume.blockIncrSize;
                                file.blockIncrChecksumSize = fileResume.blockIncrChecksumSize;
                                file.blockIncrMapSize = fileResume.blockIncrMapSize;
                                file.checksumPage = fileResume.checksumPage;
                                file.checksumPageError = fileResume.checksumPageError;
                                file.checksumPageErrorList = fileResume.checksumPageErrorList;

                                file.resume = resume;
                                file.delta = delta;
                                file.copy = resume | delta;

                                manifestFileUpdate(manifest, &file);
                            }
                        }
                    }

                    // Remove the file if it could not be resumed
                    if (removeReason != NULL)
                    {
                        LOG_DETAIL_FMT(
                            "remove file '%s' from%s backup (%s)", strZ(storagePathP(storageRepo(), backupPath)), resumeZ,
                            removeReason);
                        storageRemoveP(storageRepoWrite(), backupPath);
                    }

                    break;
                }

                // Remove links. We could check that the link has not changed and preserve it but it doesn't seem worth the extra
                // testing. The link will be recreated during the backup if needed.
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeLink:
                    storageRemoveP(storageRepoWrite(), backupPath);
                    break;

                // Remove special files
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeSpecial:
                    LOG_WARN_FMT("remove special file '%s' from%s backup", strZ(storagePathP(storageRepo(), backupPath)), resumeZ);
                    storageRemoveP(storageRepoWrite(), backupPath);
                    break;
            }
        }

        // Reset the memory context occasionally so we don't use too much memory or slow down processing
        MEM_CONTEXT_TEMP_RESET(1000);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

// Helper to clean invalid paths/files/links out of the resumable backup path
static void
backupResumeClean(Manifest *const manifest, const Manifest *const manifestResume, const bool resume, const bool delta)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST, manifestResume);
        FUNCTION_LOG_PARAM(BOOL, resume);
        FUNCTION_LOG_PARAM(BOOL, delta);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);
    ASSERT(manifestResume != NULL);
    ASSERT(manifestData(manifest)->backupType == backupTypeFull);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Set the backup label to the resumed backup
        manifestBackupLabelSet(manifest, manifestData(manifestResume)->backupLabel);

        // Copy cipher subpass since it was used to encrypt the resumable files
        manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestResume));

        // Clean resumed backup
        const String *const backupPath = strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(manifestData(manifest)->backupLabel));

        backupResumeCleanRecurse(
            storageNewItrP(storageRepo(), backupPath, .sortOrder = sortOrderAsc), manifest, manifestResume,
            compressTypeEnum(cfgOptionStrId(cfgOptCompressType)), delta, resume, backupPath, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

// Helper to find a resumable backup
static const Manifest *
backupResumeFind(const Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Only the last backup can be resumed
        const StringList *const backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        if (!strLstEmpty(backupList))
        {
            const String *const backupLabel = strLstGet(backupList, 0);
            const String *const manifestFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

            // Resumable backups do not have backup.manifest
            if (!storageExistsP(storageRepo(), manifestFile))
            {
                const bool resume = cfgOptionBool(cfgOptResume) && cfgOptionStrId(cfgOptType) == backupTypeFull;
                bool usable = false;
                const String *reason = STRDEF("resume only valid for full backup");
                Manifest *manifestResume = NULL;

                if (cfgOptionStrId(cfgOptType) == backupTypeFull)
                {
                    reason = STRDEF("partially deleted by prior resume or invalid");

                    // Resumable backups must have backup.manifest.copy
                    if (storageExistsP(storageRepo(), strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile))))
                    {
                        reason = strNewZ("resume is disabled");

                        // Attempt to read the manifest file in the resumable backup to see if it can be used. If any error at all
                        // occurs then the backup will be considered unusable and a resume will not be attempted.
                        if (resume)
                        {
                            TRY_BEGIN()
                            {
                                manifestResume = manifestLoadFile(
                                    storageRepo(), manifestFile, cfgOptionStrId(cfgOptRepoCipherType), cipherPassBackup);
                            }
                            CATCH_ANY()
                            {
                                reason = strNewFmt("unable to read %s" INFO_COPY_EXT, strZ(manifestFile));
                            }
                            TRY_END();

                            if (manifestResume != NULL)
                            {
                                const ManifestData *manifestResumeData = manifestData(manifestResume);

                                // Check pgBackRest version. This allows the resume implementation to be changed with each version
                                // of pgBackRest at the expense of users losing a resumable back after an upgrade, which seems worth
                                // the cost.
                                if (!strEq(manifestResumeData->backrestVersion, manifestData(manifest)->backrestVersion))
                                {
                                    reason = strNewFmt(
                                        "new " PROJECT_NAME " version '%s' does not match resumable " PROJECT_NAME " version '%s'",
                                        strZ(manifestData(manifest)->backrestVersion), strZ(manifestResumeData->backrestVersion));
                                }
                                // Check backup type because new backup label must be the same type as resume backup label
                                else if (manifestResumeData->backupType != cfgOptionStrId(cfgOptType))
                                {
                                    reason = strNewFmt(
                                        "new backup type '%s' does not match resumable backup type '%s'",
                                        strZ(cfgOptionDisplay(cfgOptType)), strZ(strIdToStr(manifestResumeData->backupType)));
                                }
                                // Check prior backup label ??? Do we really care about the prior backup label?
                                else if (!strEq(manifestResumeData->backupLabelPrior, manifestData(manifest)->backupLabelPrior))
                                {
                                    reason = strNewFmt(
                                        "new prior backup label '%s' does not match resumable prior backup label '%s'",
                                        manifestResumeData->backupLabelPrior ?
                                            strZ(manifestResumeData->backupLabelPrior) : "<undef>",
                                        manifestData(manifest)->backupLabelPrior ?
                                            strZ(manifestData(manifest)->backupLabelPrior) : "<undef>");
                                }
                                // Check compression. Compression can't be changed between backups so resume won't work either.
                                else if (
                                    manifestResumeData->backupOptionCompressType !=
                                    compressTypeEnum(cfgOptionStrId(cfgOptCompressType)))
                                {
                                    reason = strNewFmt(
                                        "new compression '%s' does not match resumable compression '%s'",
                                        strZ(cfgOptionDisplay(cfgOptCompressType)),
                                        strZ(compressTypeStr(manifestResumeData->backupOptionCompressType)));
                                }
                                else
                                    usable = true;
                            }
                        }
                    }
                }

                // If the backup is usable then return the manifest
                if (usable)
                {
                    result = manifestMove(manifestResume, memContextPrior());
                }
                // Else warn and remove the unusable backup
                else
                {
                    LOG_FMT(
                        resume ? logLevelWarn : logLevelInfo, 0, "backup '%s' cannot be resumed: %s", strZ(backupLabel),
                        strZ(reason));

                    storagePathRemoveP(
                        storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabel)), .recurse = true);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupResume(Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifestResume = backupResumeFind(manifest, cipherPassBackup);

        // If a resumable backup was found set the label and cipher subpass
        if (manifestResume)
        {
            // Resuming
            result = true;

            LOG_WARN_FMT(
                "resumable backup %s of same type exists -- invalid files will be removed then the backup will resume",
                strZ(manifestData(manifestResume)->backupLabel));

            backupResumeClean(manifest, manifestResume, true, cfgOptionBool(cfgOptDelta));
        }
        // Else generate a new label for the backup
        else
        {
            manifestBackupLabelSet(
                manifest,
                backupLabelCreate(
                    manifestData(manifest)->backupType, manifestData(manifest)->backupLabelPrior,
                    manifestData(manifest)->backupTimestampStart));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Start the backup
***********************************************************************************************************************************/
typedef struct BackupStartResult
{
    String *lsn;
    String *walSegmentName;
    Pack *dbList;
    Pack *tablespaceList;
} BackupStartResult;

static BackupStartResult
backupStart(const BackupData *const backupData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    BackupStartResult result = {.lsn = NULL};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If this is an offline backup
        if (!cfgOptionBool(cfgOptOnline))
        {
            // Check if Postgres is running and if so only continue when forced
            if (storageExistsP(backupData->storagePrimary, PG_FILE_POSTMTRPID_STR))
            {
                if (cfgOptionBool(cfgOptForce))
                {
                    LOG_WARN(
                        "--no-" CFGOPT_ONLINE " passed and " PG_FILE_POSTMTRPID " exists but --" CFGOPT_FORCE " was passed so"
                        " backup will continue though it looks like " PG_NAME " is running and the backup will probably not be"
                        " consistent");
                }
                else
                {
                    THROW(
                        PgRunningError,
                        "--no-" CFGOPT_ONLINE " passed but " PG_FILE_POSTMTRPID " exists - looks like " PG_NAME " is running. Shut"
                        " down " PG_NAME " and try again, or use --force.");
                }
            }
        }
        // Else start the backup normally
        else
        {
            // Check database configuration
            checkDbConfig(backupData->version, backupData->pgIdxPrimary, backupData->dbPrimary, false);

            // Start backup
            LOG_INFO_FMT(
                "execute %sexclusive backup start: backup begins after the %s checkpoint completes",
                backupData->version >= PG_VERSION_96 ? "non-" : "",
                cfgOptionBool(cfgOptStartFast) ? "requested immediate" : "next regular");

            const DbBackupStartResult dbBackupStartResult = dbBackupStart(
                backupData->dbPrimary, cfgOptionBool(cfgOptStartFast), cfgOptionBool(cfgOptStopAuto),
                cfgOptionBool(cfgOptArchiveCheck));

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.lsn = strDup(dbBackupStartResult.lsn);
                result.walSegmentName = strDup(dbBackupStartResult.walSegmentName);
                result.dbList = dbList(backupData->dbPrimary);
                result.tablespaceList = dbTablespaceList(backupData->dbPrimary);
            }
            MEM_CONTEXT_PRIOR_END();

            LOG_INFO_FMT("backup start archive = %s, lsn = %s", strZ(result.walSegmentName), strZ(result.lsn));

            // Wait for replay on the standby to catch up
            if (backupData->dbStandby != NULL)
            {
                LOG_INFO_FMT("wait for replay on the standby to reach %s", strZ(result.lsn));
                dbReplayWait(backupData->dbStandby, result.lsn, backupData->timeline, cfgOptionUInt64(cfgOptArchiveTimeout));
                LOG_INFO_FMT("replay on the standby reached %s", strZ(result.lsn));
            }

            // Check that WAL segments are being archived. If archiving is not working then the backup will eventually fail so
            // better to catch it as early as possible. A segment to check may not be available on older versions of PostgreSQL or
            // if archive-check is false.
            if (dbBackupStartResult.walSegmentCheck != NULL)
            {
                LOG_INFO_FMT(
                    "check archive for %ssegment %s",
                    strEq(result.walSegmentName, dbBackupStartResult.walSegmentCheck) ? "" : "prior ",
                    strZ(dbBackupStartResult.walSegmentCheck));

                walSegmentFindOne(
                    storageRepo(), backupData->archiveId, dbBackupStartResult.walSegmentCheck,
                    cfgOptionUInt64(cfgOptArchiveTimeout));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Stop the backup
***********************************************************************************************************************************/
// Helper to write a file from a string to the repository and update the manifest
static void
backupFilePut(
    const BackupData *const backupData, Manifest *const manifest, const String *const name, const time_t timestamp,
    const String *const content)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, name);
        FUNCTION_LOG_PARAM(TIME, timestamp);
        FUNCTION_LOG_PARAM(STRING, content);
    FUNCTION_LOG_END();

    // Skip files with no content
    if (content != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Create file
            bool repoChecksum = false;
            const String *const manifestName = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(name));
            const CompressType compressType = compressTypeEnum(cfgOptionStrId(cfgOptCompressType));

            StorageWrite *const write = storageNewWriteP(
                storageRepoWrite(),
                backupFileRepoPathP(
                    manifestData(manifest)->backupLabel, .manifestName = manifestName,
                    .compressType = compressTypeEnum(cfgOptionStrId(cfgOptCompressType))),
                .compressible = true);

            IoFilterGroup *const filterGroup = ioWriteFilterGroup(storageWriteIo(write));

            // Add SHA1 filter
            ioFilterGroupAdd(filterGroup, cryptoHashNew(hashTypeSha1));

            // Add compression
            if (compressType != compressTypeNone)
            {
                ioFilterGroupAdd(
                    ioWriteFilterGroup(storageWriteIo(write)), compressFilterP(compressType, cfgOptionInt(cfgOptCompressLevel)));

                repoChecksum = true;
            }

            // Add encryption filter if required
            if (manifestCipherSubPass(manifest) != NULL)
            {
                ioFilterGroupAdd(
                    ioWriteFilterGroup(storageWriteIo(write)),
                    cipherBlockNewP(
                        cipherModeEncrypt, cfgOptionStrId(cfgOptRepoCipherType), BUFSTR(manifestCipherSubPass(manifest))));

                repoChecksum = true;
            }

            // Capture checksum of file stored in the repo if filters that modify the output have been applied
            if (repoChecksum)
                ioFilterGroupAdd(filterGroup, cryptoHashNew(hashTypeSha1));

            // Add size filter last to calculate repo size
            ioFilterGroupAdd(filterGroup, ioSizeNew());

            // Write file
            storagePutP(write, BUFSTR(content));

            // Use base path to set ownership and mode
            const ManifestPath *const basePath = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);

            // Add to manifest
            ManifestFile file =
            {
                .name = manifestName,
                .mode = basePath->mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
                .user = basePath->user,
                .group = basePath->group,
                .size = strSize(content),
                .sizeOriginal = strSize(content),
                .sizeRepo = pckReadU64P(ioFilterGroupResultP(filterGroup, SIZE_FILTER_TYPE)),
                .timestamp = timestamp,
                .checksumSha1 = bufPtr(pckReadBinP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE, .idx = 0))),
            };

            if (repoChecksum)
                file.checksumRepoSha1 = bufPtr(pckReadBinP(ioFilterGroupResultP(filterGroup, CRYPTO_HASH_FILTER_TYPE, .idx = 1)));

            manifestFileAdd(manifest, &file);

            LOG_DETAIL_FMT("wrote '%s' file returned from backup stop function", strZ(name));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/*--------------------------------------------------------------------------------------------------------------------------------*/
typedef struct BackupStopResult
{
    String *lsn;
    String *walSegmentName;
    time_t timestamp;
} BackupStopResult;

static BackupStopResult
backupStop(BackupData *const backupData, Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    BackupStopResult result = {.lsn = NULL};

    if (cfgOptionBool(cfgOptOnline))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Stop the backup
            LOG_INFO_FMT(
                "execute %sexclusive backup stop and wait for all WAL segments to archive",
                backupData->version >= PG_VERSION_96 ? "non-" : "");

            const DbBackupStopResult dbBackupStopResult = dbBackupStop(backupData->dbPrimary);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.timestamp = backupTime(backupData, false);
                result.lsn = strDup(dbBackupStopResult.lsn);
                result.walSegmentName = strDup(dbBackupStopResult.walSegmentName);
            }
            MEM_CONTEXT_PRIOR_END();

            LOG_INFO_FMT("backup stop archive = %s, lsn = %s", strZ(result.walSegmentName), strZ(result.lsn));

            // Save files returned by stop backup
            backupFilePut(backupData, manifest, STRDEF(PG_FILE_BACKUPLABEL), result.timestamp, dbBackupStopResult.backupLabel);
            backupFilePut(backupData, manifest, STRDEF(PG_FILE_TABLESPACEMAP), result.timestamp, dbBackupStopResult.tablespaceMap);
        }
        MEM_CONTEXT_TEMP_END();
    }
    else
        result.timestamp = backupTime(backupData, false);

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Convert page checksum error pack to a VariantList
***********************************************************************************************************************************/
// Helper to output pages and page ranges
static void
backupJobResultPageChecksumOut(VariantList *const result, const unsigned int pageBegin, const unsigned int pageEnd)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT_LIST, result);
        FUNCTION_TEST_PARAM(UINT, pageBegin);
        FUNCTION_TEST_PARAM(UINT, pageEnd);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    // Output a single page
    if (pageBegin == pageEnd)
    {
        varLstAdd(result, varNewUInt64(pageBegin));
    }
    // Else output a page range
    else
    {
        VariantList *errorListSub = varLstNew();
        varLstAdd(errorListSub, varNewUInt64(pageBegin));
        varLstAdd(errorListSub, varNewUInt64(pageEnd));
        varLstAdd(result, varNewVarLst(errorListSub));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static VariantList *
backupJobResultPageChecksum(PackRead *const checksumPageResult)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(PACK_READ, checksumPageResult);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    VariantList *result = NULL;

    // If there is an error result array
    if (!pckReadNullP(checksumPageResult))
    {
        result = varLstNew();
        pckReadArrayBeginP(checksumPageResult);

        bool first = false;
        unsigned int pageBegin = 0;
        unsigned int pageEnd = 0;

        // Combine results into a more compact form
        while (pckReadNext(checksumPageResult))
        {
            const unsigned int pageId = pckReadId(checksumPageResult) - 1;
            pckReadObjBeginP(checksumPageResult, .id = pageId + 1);

            // If first error then just store page
            if (!first)
            {
                pageBegin = pageId;
                pageEnd = pageId;
                first = true;
            }
            // Expand list when the page is in sequence
            else if (pageId == pageEnd + 1)
            {
                pageEnd = pageId;
            }
            // Else output the page or page range
            else
            {
                backupJobResultPageChecksumOut(result, pageBegin, pageEnd);

                // Start again with a single page range
                pageBegin = pageId;
                pageEnd = pageId;
            }

            pckReadObjEndP(checksumPageResult);
        }

        // Check that the array was not empty
        CHECK(FormatError, first, "page checksum result array is empty");

        // Output last page or page range
        backupJobResultPageChecksumOut(result, pageBegin, pageEnd);

        pckReadArrayEndP(checksumPageResult);
    }

    FUNCTION_LOG_RETURN(VARIANT_LIST, result);
}

/***********************************************************************************************************************************
Log the results of a job and throw errors
***********************************************************************************************************************************/
static void
backupJobResult(
    Manifest *const manifest, const String *const host, const Storage *const storagePg, StringList *const fileRemove,
    ProtocolParallelJob *const job, const bool bundle, const PgPageSize pageSize, const uint64_t sizeTotal,
    uint64_t *const sizeProgress, unsigned int *const currentPercentComplete)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, host);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
        FUNCTION_LOG_PARAM(STRING_LIST, fileRemove);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, job);
        FUNCTION_LOG_PARAM(BOOL, bundle);
        FUNCTION_LOG_PARAM(ENUM, pageSize);
        FUNCTION_LOG_PARAM(UINT64, sizeTotal);
        FUNCTION_LOG_PARAM_P(UINT64, sizeProgress);
        FUNCTION_LOG_PARAM_P(UINT, currentPercentComplete);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);
    ASSERT(storagePg != NULL);
    ASSERT(fileRemove != NULL);
    ASSERT(job != NULL);

    // The job was successful
    if (protocolParallelJobErrorCode(job) == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const unsigned int processId = protocolParallelJobProcessId(job);
            const uint64_t bundleId =
                varType(protocolParallelJobKey(job)) == varTypeUInt64 ? varUInt64(protocolParallelJobKey(job)) : 0;
            PackRead *const jobResult = protocolParallelJobResult(job);
            unsigned int percentComplete = 0;

            while (!pckReadNullP(jobResult))
            {
                ManifestFile file = manifestFileFind(manifest, pckReadStrP(jobResult));
                const BackupCopyResult copyResult = (BackupCopyResult)pckReadU32P(jobResult);
                const bool repoInvalid = pckReadBoolP(jobResult);
                const uint64_t copySize = pckReadU64P(jobResult);
                const uint64_t bundleOffset = pckReadU64P(jobResult);
                const uint64_t blockIncrMapSize = pckReadU64P(jobResult);
                const uint64_t repoSize = pckReadU64P(jobResult);
                const Buffer *const copyChecksum = pckReadBinP(jobResult);
                const Buffer *const repoChecksum = pckReadBinP(jobResult);
                PackRead *const checksumPageResult = pckReadPackReadP(jobResult);

                // Increment backup copy progress. Use the original size since the size may have changed during the copy but for the
                // purpose of reporting progress we need to increment by the original size used to generate the total size.
                *sizeProgress += file.sizeOriginal;

                // Create log file name
                const String *const fileName = storagePathP(storagePg, manifestPathPg(file.name));
                const String *const fileLog = host == NULL ? fileName : strNewFmt("%s:%s", strZ(host), strZ(fileName));

                // Format log progress
                String *const logProgress = strNew();

                if (bundleId != 0 && copyResult != backupCopyResultNoOp && copyResult != backupCopyResultTruncate)
                    strCatFmt(logProgress, "bundle %" PRIu64 "/%" PRIu64 ", ", bundleId, bundleOffset);

                // Log original manifest size if copy size differs
                if (copySize != file.sizeOriginal)
                    strCatFmt(logProgress, "%s->", strZ(strSizeFormat(file.sizeOriginal)));

                // Store percentComplete as an integer
                percentComplete = sizeTotal == 0 ? 10000 : (unsigned int)(((double)*sizeProgress / (double)sizeTotal) * 10000);

                strCatFmt(
                    logProgress, "%s, %u.%02u%%", strZ(strSizeFormat(copySize)), percentComplete / 100, percentComplete % 100);

                // Format log checksum
                const String *const logChecksum =
                    copySize != 0 ? strNewFmt(" checksum %s", strZ(strNewEncode(encodingHex, copyChecksum))) : EMPTY_STR;

                // If the file is in a prior backup and nothing changed, just log it
                if (copyResult == backupCopyResultNoOp)
                {
                    LOG_DETAIL_PID_FMT(
                        processId, "match file from prior backup %s (%s)%s", strZ(fileLog), strZ(logProgress), strZ(logChecksum));
                }
                // Else if the repo matched the expect checksum, just log it
                else if (copyResult == backupCopyResultChecksum)
                {
                    LOG_DETAIL_PID_FMT(
                        processId, "checksum resumed file %s (%s)%s", strZ(fileLog), strZ(logProgress), strZ(logChecksum));
                }
                // Else if the file was removed during backup add it to the list of files to be removed from the manifest when the
                // backup is complete. It can't be removed right now because that will invalidate the pointers that are being used
                // for processing.
                else if (copyResult == backupCopyResultSkip)
                {
                    LOG_DETAIL_PID_FMT(processId, "skip file removed by database %s", strZ(fileLog));
                    strLstAdd(fileRemove, file.name);
                }
                // Else file was copied so update manifest
                else
                {
                    LOG_DETAIL_PID_FMT(
                        processId, "%s file %s (%s)%s", copyResult != backupCopyResultTruncate ? "backup" : "store truncated",
                        strZ(fileLog), strZ(logProgress), strZ(logChecksum));

                    // If the repo file was invalid warn that there might be corruption in the repository
                    if (repoInvalid)
                    {
                        ASSERT(copyResult == backupCopyResultCopy);

                        LOG_WARN_FMT(
                            "resumed backup file %s did not have expected checksum %s. The file was recopied and backup will"
                            " continue but this may be an issue unless the resumed backup path in the repository is known to be"
                            " corrupted.\n"
                            "NOTE: this does not indicate a problem with the PostgreSQL page checksums.",
                            strZ(file.name), strZ(strNewEncode(encodingHex, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE))));
                    }

                    // If the file had page checksums calculated during the copy
                    ASSERT((!file.checksumPage && checksumPageResult == NULL) || (file.checksumPage && checksumPageResult != NULL));

                    bool checksumPageError = false;
                    const VariantList *checksumPageErrorList = NULL;

                    if (checksumPageResult != NULL)
                    {
                        checksumPageErrorList = backupJobResultPageChecksum(checksumPageResult);

                        // If the checksum was valid
                        if (!pckReadBoolP(checksumPageResult))
                        {
                            checksumPageError = true;

                            if (!pckReadBoolP(checksumPageResult))
                            {
                                checksumPageErrorList = NULL;

                                // ??? Update formatting after migration
                                LOG_WARN_FMT(
                                    "page misalignment in file %s: file size %" PRIu64 " is not divisible by page size %u",
                                    strZ(fileLog), copySize, pageSize);
                            }
                            else
                            {
                                // Format the page checksum errors
                                CHECK(FormatError, checksumPageErrorList != NULL, "page checksum error list is missing");
                                CHECK(FormatError, !varLstEmpty(checksumPageErrorList), "page checksum error list is empty");

                                String *const error = strNew();
                                unsigned int errorTotalMin = 0;

                                for (unsigned int errorIdx = 0; errorIdx < varLstSize(checksumPageErrorList); errorIdx++)
                                {
                                    const Variant *const errorItem = varLstGet(checksumPageErrorList, errorIdx);

                                    // Add a comma if this is not the first item
                                    if (errorIdx != 0)
                                        strCatZ(error, ", ");

                                    // If an error range
                                    if (varType(errorItem) == varTypeVariantList)
                                    {
                                        const VariantList *const errorItemList = varVarLst(errorItem);
                                        ASSERT(varLstSize(errorItemList) == 2);

                                        strCatFmt(
                                            error, "%" PRIu64 "-%" PRIu64, varUInt64(varLstGet(errorItemList, 0)),
                                            varUInt64(varLstGet(errorItemList, 1)));
                                        errorTotalMin += 2;
                                    }
                                    // Else a single error
                                    else
                                    {
                                        ASSERT(varType(errorItem) == varTypeUInt64);

                                        strCatFmt(error, "%" PRIu64, varUInt64(errorItem));
                                        errorTotalMin++;
                                    }
                                }

                                // Make message plural when appropriate
                                const String *const plural = errorTotalMin > 1 ? STRDEF("s") : EMPTY_STR;

                                // ??? Update formatting after migration
                                LOG_WARN_FMT(
                                    "invalid page checksum%s found in file %s at page%s %s", strZ(plural), strZ(fileLog),
                                    strZ(plural), strZ(error));
                            }
                        }
                    }

                    // Update file info and remove any reference to the file's existence in a prior backup
                    file.size = copySize;
                    file.sizeRepo = repoSize;
                    file.checksumSha1 = bufPtrConst(copyChecksum);
                    file.checksumRepoSha1 = repoChecksum != NULL ? bufPtrConst(repoChecksum) : NULL;
                    file.reference = NULL;
                    file.checksumPageError = checksumPageError;
                    file.checksumPageErrorList =
                        checksumPageErrorList != NULL ? jsonFromVar(varNewVarLst(checksumPageErrorList)) : NULL;
                    // Truncated file is not put in bundle
                    file.bundleId = copyResult != backupCopyResultTruncate ? bundleId : 0;
                    file.bundleOffset = bundleOffset;
                    file.blockIncrMapSize = blockIncrMapSize;

                    manifestFileUpdate(manifest, &file);
                }
            }

            // Update currentPercentComplete and lock file when the change is significant enough
            if (percentComplete - *currentPercentComplete > 10)
            {
                *currentPercentComplete = percentComplete;
                cmdLockWriteP(
                    .percentComplete = VARUINT(*currentPercentComplete), .sizeComplete = VARUINT64(*sizeProgress),
                    .size = VARUINT64(sizeTotal));
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Free the job
        protocolParallelJobFree(job);
    }
    // Else the job errored
    else
        THROW_CODE(protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Save a copy of the backup manifest during processing to preserve checksums for a possible resume. Only save the final copy when
resume is disabled since an incremental copy will not be used in a future backup unless resume is enabled beforehand.
***********************************************************************************************************************************/
static void
backupManifestSaveCopy(Manifest *const manifest, const String *const cipherPassBackup, const bool final)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
        FUNCTION_LOG_PARAM(BOOL, final);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    if (final || cfgOptionBool(cfgOptResume))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Open file for write
            IoWrite *const write = storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(),
                    strNewFmt(
                        STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(manifestData(manifest)->backupLabel))));

            // Add encryption filter if required
            cipherBlockFilterGroupAdd(
                ioWriteFilterGroup(write), cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt, cipherPassBackup);

            // Save file
            manifestSave(manifest, write);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check that the clusters are alive and correctly configured during the backup
***********************************************************************************************************************************/
static void
backupDbPing(const BackupData *const backupData, const bool force)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(BOOL, force);
    FUNCTION_LOG_END();

    ASSERT(backupData != NULL);

    if (cfgOptionBool(cfgOptOnline))
    {
        dbPing(backupData->dbPrimary, force);

        if (backupData->dbStandby != NULL)
            dbPing(backupData->dbStandby, force);
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Process the backup manifest
***********************************************************************************************************************************/
typedef struct BackupJobData
{
    const Manifest *const manifest;                                 // Backup manifest
    const String *const backupLabel;                                // Backup label (defines the backup path)
    const bool backupStandby;                                       // Backup from standby
    RegExp *standbyExp;                                             // Identify files that may be copied from the standby
    const CipherType cipherType;                                    // Cipher type
    const String *const cipherSubPass;                              // Passphrase used to encrypt files in the backup
    const PgPageSize pageSize;                                      // Page size
    const CompressType compressType;                                // Backup compression type
    const int compressLevel;                                        // Compress level if backup is compressed
    const bool delta;                                               // Is this a checksum delta backup?
    const bool bundle;                                              // Bundle files?
    uint64_t bundleSize;                                            // Target bundle size
    uint64_t bundleLimit;                                           // Limit on files to bundle
    uint64_t bundleId;                                              // Bundle id
    const bool blockIncr;                                           // Block incremental?
    size_t blockIncrSizeSuper;                                      // Super block size

    List *queueList;                                                // List of processing queues
} BackupJobData;

// Identify files that must be copied from the primary
static bool
backupProcessFilePrimary(RegExp *const standbyExp, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(REGEXP, standbyExp);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(standbyExp != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(
        BOOL, strEqZ(name, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL) || !regExpMatch(standbyExp, name));
}

// Comparator to order ManifestFile objects by size, date, and name
static const Manifest *backupProcessQueueComparatorManifest = NULL;
static bool backupProcessQueueComparatorBundle;
static uint64_t backupProcessQueueComparatorBundleLimit;

static int
backupProcessQueueComparator(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    // Unpack files
    const ManifestFile file1 = manifestFileUnpack(backupProcessQueueComparatorManifest, *(const ManifestFilePack *const *)item1);
    const ManifestFile file2 = manifestFileUnpack(backupProcessQueueComparatorManifest, *(const ManifestFilePack *const *)item2);

    // If the size differs then that's enough to determine order
    if (!backupProcessQueueComparatorBundle || file1.size > backupProcessQueueComparatorBundleLimit ||
        file2.size > backupProcessQueueComparatorBundleLimit)
    {
        if (file1.size < file2.size)
            FUNCTION_TEST_RETURN(INT, -1);
        else if (file1.size > file2.size)
            FUNCTION_TEST_RETURN(INT, 1);
    }

    // If bundling order by time ascending so that older files are bundled with older files and newer with newer
    if (backupProcessQueueComparatorBundle)
    {
        if (file1.timestamp > file2.timestamp)
            FUNCTION_TEST_RETURN(INT, -1);
        else if (file1.timestamp < file2.timestamp)
            FUNCTION_TEST_RETURN(INT, 1);
    }

    // If size/time is the same then use name to generate a deterministic ordering (names must be unique)
    FUNCTION_TEST_RETURN(INT, strCmp(file1.name, file2.name));
}

// Helper to generate the backup queues
static uint64_t
backupProcessQueue(
    const BackupData *const backupData, Manifest *const manifest, BackupJobData *const jobData, const bool preliminary)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(VOID, jobData);
        FUNCTION_LOG_PARAM(BOOL, preliminary);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(manifest != NULL);

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create list of process queues (use void * instead of List * to avoid Coverity false positive)
        jobData->queueList = lstNewP(sizeof(void *));

        // Generate the list of targets
        StringList *const targetList = strLstNew();
        strLstAddZ(targetList, MANIFEST_TARGET_PGDATA "/");

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

            if (target->tablespaceId != 0)
                strLstAddFmt(targetList, "%s/", strZ(target->name));
        }

        // Generate the processing queues (there is always at least one)
        const unsigned int queueOffset = jobData->backupStandby ? 1 : 0;

        MEM_CONTEXT_BEGIN(lstMemContext(jobData->queueList))
        {
            for (unsigned int queueIdx = 0; queueIdx < strLstSize(targetList) + queueOffset; queueIdx++)
            {
                List *const queue = lstNewP(sizeof(ManifestFile *), .comparator = backupProcessQueueComparator);
                lstAdd(jobData->queueList, &queue);
            }
        }
        MEM_CONTEXT_END();

        // Now put all files into the processing queues
        uint64_t fileTotal = 0;
        bool pgControlFound = false;

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFilePack *const filePack = manifestFilePackGet(manifest, fileIdx);
            const ManifestFile file = manifestFileUnpack(manifest, filePack);

            // Only process files that need to be copied
            if (!file.copy)
            {
                // If bundling log zero-length files as stored since they will never be copied
                if (file.size == 0 && jobData->bundle)
                {
                    LOG_DETAIL_FMT(
                        "store zero-length file %s", strZ(storagePathP(backupData->storagePrimary, manifestPathPg(file.name))));
                }

                continue;
            }

            // Is pg_control in the backup?
            if (strEq(file.name, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)))
                pgControlFound = true;

            // Files that must be copied from the primary are always put in queue 0 when backup from standby
            if (jobData->backupStandby && backupProcessFilePrimary(jobData->standbyExp, file.name))
            {
                lstAdd(*(List **)lstGet(jobData->queueList, 0), &filePack);
            }
            // Else find the correct queue by matching the file to a target
            else
            {
                // Find the target that contains this file
                unsigned int targetIdx = 0;

                do
                {
                    CHECK(AssertError, targetIdx < strLstSize(targetList), "backup target not found");

                    if (strBeginsWith(file.name, strLstGet(targetList, targetIdx)))
                        break;

                    targetIdx++;
                }
                while (1);

                // Add file to queue
                lstAdd(*(List **)lstGet(jobData->queueList, targetIdx + queueOffset), &filePack);
            }

            // Add size to total
            result += file.sizeOriginal;

            // Increment total files
            fileTotal++;
        }

        // pg_control should always be in an online backup
        if (!preliminary && !pgControlFound && cfgOptionBool(cfgOptOnline))
        {
            THROW(
                FileMissingError,
                PG_FILE_PGCONTROL " must be present in all online backups\n"
                "HINT: is something wrong with the clock or filesystem timestamps?");
        }

        // If there are no files to backup then we'll exit with an error. This could happen if the database is down and backup is
        // called with --no-online twice in a row.
        if (fileTotal == 0)
            THROW(FileMissingError, "no files have changed since the last backup - this seems unlikely");

        // Sort the queues
        backupProcessQueueComparatorManifest = manifest;
        backupProcessQueueComparatorBundle = jobData->bundle;
        backupProcessQueueComparatorBundleLimit = jobData->bundleLimit;

        for (unsigned int queueIdx = 0; queueIdx < lstSize(jobData->queueList); queueIdx++)
            lstSort(*(List **)lstGet(jobData->queueList, queueIdx), sortOrderDesc);

        // Move process queues to prior context
        lstMove(jobData->queueList, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT64, result);
}

// Helper to calculate the next queue to scan based on the client index
static int
backupJobQueueNext(const unsigned int clientIdx, int queueIdx, const unsigned int queueTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, clientIdx);
        FUNCTION_TEST_PARAM(INT, queueIdx);
        FUNCTION_TEST_PARAM(UINT, queueTotal);
    FUNCTION_TEST_END();

    // Move (forward or back) to the next queue
    queueIdx += clientIdx % 2 ? -1 : 1;

    // Deal with wrapping on either end
    if (queueIdx < 0)
        FUNCTION_TEST_RETURN(INT, (int)queueTotal - 1);
    else if (queueIdx == (int)queueTotal)
        FUNCTION_TEST_RETURN(INT, 0);

    FUNCTION_TEST_RETURN(INT, queueIdx);
}

// Callback to fetch backup jobs for the parallel executor
static ProtocolParallelJob *
backupJobCallback(void *const data, const unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    ProtocolParallelJob *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get a new job if there are any left
        BackupJobData *const jobData = data;

        // Determine where to begin scanning the queue (we'll stop when we get back here). When copying from the primary during
        // backup from standby only queue 0 will be used.
        const unsigned int queueOffset = jobData->backupStandby && clientIdx > 0 ? 1 : 0;
        int queueIdx =
            jobData->backupStandby && clientIdx == 0 ? 0 : (int)(clientIdx % (lstSize(jobData->queueList) - queueOffset));
        const int queueEnd = queueIdx;

        // Create backup job
        PackWrite *param = NULL;
        uint64_t fileTotal = 0;
        uint64_t fileSize = 0;

        do
        {
            List *const queue = *(List **)lstGet(jobData->queueList, (unsigned int)queueIdx + queueOffset);
            unsigned int fileIdx = 0;
            bool bundle = jobData->bundle;
            const String *fileName = NULL;

            while (fileIdx < lstSize(queue))
            {
                const ManifestFile file = manifestFileUnpack(jobData->manifest, *(ManifestFilePack **)lstGet(queue, fileIdx));

                // Continue if the next file would make the bundle too large. There may be a smaller one that will fit.
                if (fileTotal > 0 && fileSize + file.size >= jobData->bundleSize)
                {
                    fileIdx++;
                    continue;
                }

                // Is this file a block incremental?
                const bool blockIncr = jobData->blockIncr && file.blockIncrSize > 0;

                // Add common parameters before first file
                if (param == NULL)
                {
                    param = protocolPackNew();

                    if (bundle && file.size <= jobData->bundleLimit)
                    {
                        pckWriteStrP(param, backupFileRepoPathP(jobData->backupLabel, .bundleId = jobData->bundleId));
                        pckWriteU64P(param, jobData->bundleId);
                        pckWriteBoolP(param, manifestData(jobData->manifest)->bundleRaw);
                    }
                    else
                    {
                        CHECK(AssertError, fileTotal == 0, "cannot bundle file");

                        pckWriteStrP(
                            param,
                            backupFileRepoPathP(
                                jobData->backupLabel, .manifestName = file.name, .compressType = jobData->compressType,
                                .blockIncr = blockIncr));
                        pckWriteU64P(param, 0);

                        fileName = file.name;
                        bundle = false;
                    }

                    // Provide the backup reference
                    pckWriteU64P(param, strLstSize(manifestReferenceList(jobData->manifest)) - 1);

                    pckWriteU32P(param, jobData->compressType);
                    pckWriteI32P(param, jobData->compressLevel);
                    pckWriteU64P(param, jobData->cipherSubPass == NULL ? cipherTypeNone : cipherTypeAes256Cbc);
                    pckWriteStrP(param, jobData->cipherSubPass);
                    pckWriteU32P(param, jobData->pageSize);
                    pckWriteStrP(param, cfgOptionStrNull(cfgOptPgVersionForce));
                }

                pckWriteStrP(param, manifestPathPg(file.name));
                pckWriteBoolP(param, file.delta);
                pckWriteBoolP(param, !strEq(file.name, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)));
                pckWriteU64P(param, file.size);
                pckWriteU64P(param, file.sizeOriginal);
                pckWriteBoolP(param, !backupProcessFilePrimary(jobData->standbyExp, file.name));
                pckWriteBinP(param, file.checksumSha1 != NULL ? BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE) : NULL);
                pckWriteBoolP(param, file.checksumPage);
                pckWriteBoolP(param, cfgOptionBool(cfgOptPageHeaderCheck));

                // If block incremental then provide the location of the prior map when available
                if (blockIncr)
                {
                    pckWriteU64P(param, file.blockIncrSize);
                    pckWriteU64P(param, file.blockIncrChecksumSize);
                    pckWriteU64P(param, jobData->blockIncrSizeSuper);

                    if (file.blockIncrMapSize != 0 && file.reference != NULL)
                    {
                        pckWriteStrP(
                            param,
                            backupFileRepoPathP(
                                file.reference, .manifestName = file.name, .bundleId = file.bundleId, .blockIncr = true));
                        pckWriteU64P(param, file.bundleOffset + file.sizeRepo - file.blockIncrMapSize);
                        pckWriteU64P(param, file.blockIncrMapSize);
                    }
                    else
                        pckWriteNullP(param);
                }
                else
                    pckWriteU64P(param, 0);

                pckWriteStrP(param, file.name);
                pckWriteBinP(param, file.checksumRepoSha1 != NULL ? BUF(file.checksumRepoSha1, HASH_TYPE_SHA1_SIZE) : NULL);
                pckWriteU64P(param, file.sizeRepo);
                pckWriteBoolP(param, file.resume);
                pckWriteBoolP(param, file.reference != NULL);

                fileTotal++;
                fileSize += file.sizeOriginal;

                // Remove job from the queue
                lstRemoveIdx(queue, fileIdx);

                // Break if not bundling or bundle size has been reached
                if (!bundle || fileSize >= jobData->bundleSize)
                    break;
            }

            if (fileTotal > 0)
            {
                // Assign job to result
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = protocolParallelJobNew(
                        bundle ? VARUINT64(jobData->bundleId) : VARSTR(fileName), PROTOCOL_COMMAND_BACKUP_FILE, param);

                    if (bundle)
                        jobData->bundleId++;
                }
                MEM_CONTEXT_PRIOR_END();

                break;
            }

            // Don't get next queue when copying from primary during backup from standby since the primary only has one queue
            if (!jobData->backupStandby || clientIdx > 0)
                queueIdx = backupJobQueueNext(clientIdx, queueIdx, lstSize(jobData->queueList) - queueOffset);
        }
        while (queueIdx != queueEnd);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PROTOCOL_PARALLEL_JOB, result);
}

static void
backupProcess(
    const BackupData *const backupData, Manifest *const manifest, const bool preliminary, const String *const cipherPassBackup,
    const uint64_t copySizePrelim, const uint64_t copySizeFinal)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(BOOL, preliminary);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(backupData != NULL);
    ASSERT(manifest != NULL);
    ASSERT(copySizePrelim == 0 || copySizeFinal == 0);

    uint64_t sizeTotal = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get backup info
        const BackupType backupType = manifestData(manifest)->backupType;
        const String *const backupLabel = manifestData(manifest)->backupLabel;
        const String *const backupPathExp = strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabel));
        const bool hardLink = cfgOptionBool(cfgOptRepoHardlink) && storageFeature(storageRepoWrite(), storageFeatureHardLink);
        const bool backupStandby = backupData->dbStandby != NULL;

        BackupJobData jobData =
        {
            .manifest = manifest,
            .backupLabel = backupLabel,
            .backupStandby = backupData->dbStandby != NULL,
            .compressType = compressTypeEnum(cfgOptionStrId(cfgOptCompressType)),
            .compressLevel = cfgOptionInt(cfgOptCompressLevel),
            .cipherType = cfgOptionStrId(cfgOptRepoCipherType),
            .cipherSubPass = manifestCipherSubPass(manifest),
            .pageSize = backupData->pageSize,
            .delta = cfgOptionBool(cfgOptDelta),
            .bundle = cfgOptionBool(cfgOptRepoBundle),
            .bundleId = 1,
            .blockIncr = cfgOptionBool(cfgOptRepoBlock),

            // Build expression to identify files that can be copied from the standby when standby backup is supported
            .standbyExp = regExpNew(
                strNewFmt(
                    "^((" MANIFEST_TARGET_PGDATA "/(" PG_PATH_BASE "|" PG_PATH_GLOBAL "|%s|" PG_PATH_PGMULTIXACT "))"
                    "|" MANIFEST_TARGET_PGTBLSPC ")/",
                    strZ(pgXactPath(backupData->version)))),
        };

        if (jobData.bundle)
        {
            jobData.bundleSize = cfgOptionUInt64(cfgOptRepoBundleSize);
            jobData.bundleLimit = cfgOptionUInt64(cfgOptRepoBundleLimit);
        }

        if (jobData.blockIncr)
        {
            // Set super block size based on the backup type
            jobData.blockIncrSizeSuper =
                backupType == backupTypeFull ?
                    (size_t)cfgOptionUInt64(cfgOptRepoBlockSizeSuperFull) : (size_t)cfgOptionUInt64(cfgOptRepoBlockSizeSuper);
        }

        // If this is a full backup or hard-linked and paths are supported then create all paths explicitly so that empty paths will
        // exist in the repo. Also create tablespace symlinks when symlinks are available. This makes it possible for the user to
        // make a copy of the backup path and get a valid cluster.
        if (!preliminary && ((backupType == backupTypeFull && !jobData.bundle) || hardLink))
        {
            // Create paths when available
            if (storageFeature(storageRepoWrite(), storageFeaturePath))
            {
                for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
                {
                    storagePathCreateP(
                        storageRepoWrite(),
                        strNewFmt("%s/%s", strZ(backupPathExp), strZ(manifestPath(manifest, pathIdx)->name)));
                }
            }

            // Create tablespace symlinks when available
            if (storageFeature(storageRepoWrite(), storageFeatureSymLink))
            {
                for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
                {
                    const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

                    if (target->tablespaceId != 0)
                    {
                        const String *const link = storagePathP(
                            storageRepo(),
                            strNewFmt("%s/" MANIFEST_TARGET_PGDATA "/%s", strZ(backupPathExp), strZ(target->name)));
                        const String *const linkDestination = strNewFmt(
                            "../../" MANIFEST_TARGET_PGTBLSPC "/%u", target->tablespaceId);

                        storageLinkCreateP(storageRepoWrite(), linkDestination, link);
                    }
                }
            }
        }

        // Generate processing queues
        sizeTotal = backupProcessQueue(backupData, manifest, &jobData, preliminary) + copySizePrelim + copySizeFinal;

        // Create the parallel executor
        ProtocolParallel *const parallelExec = protocolParallelNew(
            cfgOptionUInt64(cfgOptProtocolTimeout) / 2, backupJobCallback, &jobData);

        // First client is always on the primary
        protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypePg, backupData->pgIdxPrimary, 1));

        // Create the rest of the clients on the primary or standby depending on the value of backup-standby. Note that standby
        // backups don't count the primary client in process-max.
        const unsigned int processMax = cfgOptionUInt(cfgOptProcessMax) + (jobData.backupStandby ? 1 : 0);
        const unsigned int pgIdx = jobData.backupStandby ? backupData->pgIdxStandby : backupData->pgIdxPrimary;

        for (unsigned int processIdx = 2; processIdx <= processMax; processIdx++)
            protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypePg, pgIdx, processIdx));

        // Maintain a list of files that need to be removed from the manifest when the backup is complete
        StringList *const fileRemove = strLstNew();

        // Determine how often the manifest will be saved (every one percent or threshold size, whichever is greater)
        uint64_t manifestSaveLast = 0;
        uint64_t manifestSaveSize = sizeTotal / 100;

        if (manifestSaveSize < cfgOptionUInt64(cfgOptManifestSaveThreshold))
            manifestSaveSize = cfgOptionUInt64(cfgOptManifestSaveThreshold);

        // Process jobs
        uint64_t sizeProgress = copySizePrelim;

        // Initialize percent complete and bytes completed/total
        unsigned int currentPercentComplete = 0;
        cmdLockWriteP(
            .percentComplete = VARUINT(currentPercentComplete), .sizeComplete = VARUINT64(sizeProgress),
            .size = VARUINT64(sizeTotal));

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            do
            {
                const unsigned int completed = protocolParallelProcess(parallelExec);

                for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                {
                    ProtocolParallelJob *const job = protocolParallelResult(parallelExec);

                    backupJobResult(
                        manifest,
                        backupStandby && protocolParallelJobProcessId(job) > 1 ? backupData->hostStandby : backupData->hostPrimary,
                        protocolParallelJobProcessId(job) > 1 ? storagePgIdx(pgIdx) : backupData->storagePrimary,
                        fileRemove, job, jobData.bundle, jobData.pageSize, sizeTotal, &sizeProgress, &currentPercentComplete);
                }

                // A keep-alive is required here for the remote holding open the backup connection
                protocolKeepAlive();

                // Check that the clusters are alive and correctly configured during the backup
                backupDbPing(backupData, false);

                // Save the manifest periodically to preserve checksums for resume
                if (sizeProgress - manifestSaveLast >= manifestSaveSize)
                {
                    backupManifestSaveCopy(manifest, cipherPassBackup, false);
                    manifestSaveLast = sizeProgress;
                }

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);
            }
            while (!protocolParallelDone(parallelExec));
        }
        MEM_CONTEXT_TEMP_END();

#ifdef DEBUG
        // Ensure that all processing queues are empty
        for (unsigned int queueIdx = 0; queueIdx < lstSize(jobData.queueList); queueIdx++)
            ASSERT(lstEmpty(*(List **)lstGet(jobData.queueList, queueIdx)));
#endif

        // Remove files from the manifest that were removed during the backup. This must happen after processing to avoid
        // invalidating pointers by deleting items from the list.
        for (unsigned int fileRemoveIdx = 0; fileRemoveIdx < strLstSize(fileRemove); fileRemoveIdx++)
            manifestFileRemove(manifest, strLstGet(fileRemove, fileRemoveIdx));

        // Log references or create hardlinks for all files
        if (!preliminary)
        {
            const char *const compressExt = strZ(compressExtStr(jobData.compressType));

            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile file = manifestFile(manifest, fileIdx);

                // If the file has a reference, then it was not copied since it can be retrieved from the referenced backup.
                // However, if hardlinking is enabled the link will need to be created.
                if (file.reference != NULL)
                {
                    // If hardlinking is enabled then create a hardlink for files that have not changed since the last backup
                    if (hardLink)
                    {
                        LOG_DETAIL_FMT("hardlink %s to %s", strZ(file.name), strZ(file.reference));

                        const String *const linkName = storagePathP(
                            storageRepo(), strNewFmt("%s/%s%s", strZ(backupPathExp), strZ(file.name), compressExt));
                        const String *const linkDestination = storagePathP(
                            storageRepo(),
                            strNewFmt(STORAGE_REPO_BACKUP "/%s/%s%s", strZ(file.reference), strZ(file.name), compressExt));

                        storageLinkCreateP(storageRepoWrite(), linkDestination, linkName, .linkType = storageLinkHard);
                    }
                    // Else log the reference. With delta, it is possible that references may have been removed if a file needed to
                    // be recopied.
                    else
                        LOG_DETAIL_FMT("reference %s to %s", strZ(file.name), strZ(file.reference));
                }
            }

            // Sync backup paths if required
            if (storageFeature(storageRepoWrite(), storageFeaturePathSync))
            {
                for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
                {
                    const String *const path = strNewFmt("%s/%s", strZ(backupPathExp), strZ(manifestPath(manifest, pathIdx)->name));

                    // Always sync the path if it exists or if the backup is full (without bundling) or hardlinked. In the latter
                    // cases the directory should always exist so we want to error if it does not.
                    if ((backupType == backupTypeFull && !jobData.bundle) || hardLink || storagePathExistsP(storageRepo(), path))
                        storagePathSyncP(storageRepoWrite(), path);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check and copy WAL segments required to make the backup consistent
***********************************************************************************************************************************/
static void
backupArchiveCheckCopy(const BackupData *const backupData, Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    // If archive logs are required to complete the backup, then check them. This is the default, but can be overridden if the
    // archive logs are going to a different server. Be careful of disabling this option because there is no way to verify that the
    // backup will be consistent - at least not here.
    if (cfgOptionBool(cfgOptArchiveCheck))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const uint64_t lsnStart = pgLsnFromStr(manifestData(manifest)->lsnStart);
            const uint64_t lsnStop = pgLsnFromStr(manifestData(manifest)->lsnStop);

            LOG_INFO_FMT(
                "check archive for segment(s) %s:%s",
                strZ(pgLsnToWalSegment(backupData->timeline, lsnStart, backupData->walSegmentSize)),
                strZ(pgLsnToWalSegment(backupData->timeline, lsnStop, backupData->walSegmentSize)));

            // Save the backup manifest before getting archive logs in case of failure
            backupManifestSaveCopy(manifest, cipherPassBackup, false);

            // Use base path to set ownership and mode
            const ManifestPath *const basePath = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);

            // Loop through all the segments in the lsn range
            const StringList *const walSegmentList = pgLsnRangeToWalSegmentList(
                backupData->timeline, lsnStart, lsnStop, backupData->walSegmentSize);
            WalSegmentFind *const find = walSegmentFindNew(
                storageRepo(), backupData->archiveId, strLstSize(walSegmentList) == 1, cfgOptionUInt64(cfgOptArchiveTimeout));

            for (unsigned int walSegmentIdx = 0; walSegmentIdx < strLstSize(walSegmentList); walSegmentIdx++)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    // Find the actual wal segment file (including checksum compression extension) in the archive
                    const String *const walSegment = strLstGet(walSegmentList, walSegmentIdx);
                    const String *const archiveFile = walSegmentFind(find, walSegment);

                    if (cfgOptionBool(cfgOptArchiveCopy))
                    {
                        // Copy can be a pretty expensive operation so log it
                        LOG_DETAIL_FMT("copy segment %s to backup", strZ(walSegment));

                        // Get compression type of the WAL segment and backup
                        const CompressType archiveCompressType = compressTypeFromName(archiveFile);
                        const CompressType backupCompressType = compressTypeEnum(cfgOptionStrId(cfgOptCompressType));

                        // Open the archive file
                        StorageRead *const read = storageNewReadP(
                            storageRepo(),
                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(backupData->archiveId), strZ(archiveFile)));
                        IoFilterGroup *const filterGroup = ioReadFilterGroup(storageReadIo(read));

                        // Decrypt with archive key if encrypted
                        cipherBlockFilterGroupAdd(
                            filterGroup, cfgOptionStrId(cfgOptRepoCipherType), cipherModeDecrypt,
                            infoArchiveCipherPass(backupData->archiveInfo));

                        // Compress/decompress if archive and backup do not have the same compression settings
                        if (archiveCompressType != backupCompressType)
                        {
                            if (archiveCompressType != compressTypeNone)
                                ioFilterGroupAdd(filterGroup, decompressFilterP(archiveCompressType));

                            if (backupCompressType != compressTypeNone)
                            {
                                ioFilterGroupAdd(
                                    filterGroup, compressFilterP(backupCompressType, cfgOptionInt(cfgOptCompressLevel)));
                            }
                        }

                        // Encrypt with backup key if encrypted
                        cipherBlockFilterGroupAdd(
                            filterGroup, cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt, manifestCipherSubPass(manifest));

                        // Add size filter last to calculate repo size
                        ioFilterGroupAdd(filterGroup, ioSizeNew());

                        // Copy the file
                        const String *const manifestName = strNewFmt(
                            MANIFEST_TARGET_PGDATA "/%s/%s", strZ(pgWalPath(manifestData(manifest)->pgVersion)), strZ(walSegment));

                        storageCopyP(
                            read,
                            storageNewWriteP(
                                storageRepoWrite(),
                                backupFileRepoPathP(
                                    manifestData(manifest)->backupLabel, manifestName, 0,
                                    compressTypeEnum(cfgOptionStrId(cfgOptCompressType)), false)));

                        // Add to manifest
                        ManifestFile file =
                        {
                            .name = manifestName,
                            .mode = basePath->mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
                            .user = basePath->user,
                            .group = basePath->group,
                            .size = backupData->walSegmentSize,
                            .sizeOriginal = backupData->walSegmentSize,
                            .sizeRepo = pckReadU64P(ioFilterGroupResultP(filterGroup, SIZE_FILTER_TYPE)),
                            .timestamp = manifestData(manifest)->backupTimestampStop,
                            .checksumSha1 = bufPtr(bufNewDecode(encodingHex, strSubN(archiveFile, 25, 40))),
                        };

                        manifestFileAdd(manifest, &file);
                    }
                }
                MEM_CONTEXT_TEMP_END();

                // A keep-alive is required here for the remote holding the backup lock
                protocolKeepAlive();
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Save and update all files required to complete the backup
***********************************************************************************************************************************/
static void
backupComplete(InfoBackup *const infoBackup, Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const backupLabel = manifestData(manifest)->backupLabel;

        // Validation and final save of the backup manifest. Validate in strict mode to catch as many potential issues as possible.
        // -------------------------------------------------------------------------------------------------------------------------
        manifestValidate(manifest, true);

        backupManifestSaveCopy(manifest, infoPgCipherPass(infoBackupPg(infoBackup)), true);

        storageCopy(
            storageNewReadP(
                storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(backupLabel))),
            storageNewWriteP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel))));

        // Copy a compressed version of the manifest to history. If the repo is encrypted then the passphrase to open the manifest
        // is required. We can't just do a straight copy since the destination needs to be compressed and that must happen before
        // encryption in order to be efficient. Compression will always be gz for compatibility and since it is always available.
        // -------------------------------------------------------------------------------------------------------------------------
        StorageRead *const manifestRead = storageNewReadP(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel)));

        cipherBlockFilterGroupAdd(
            ioReadFilterGroup(storageReadIo(manifestRead)), cfgOptionStrId(cfgOptRepoCipherType), cipherModeDecrypt,
            infoPgCipherPass(infoBackupPg(infoBackup)));

        StorageWrite *const manifestWrite = storageNewWriteP(
            storageRepoWrite(),
            strNewFmt(
                STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY "/%s/%s.manifest%s", strZ(strSubN(backupLabel, 0, 4)),
                strZ(backupLabel), strZ(compressExtStr(compressTypeGz))));

        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(manifestWrite)), compressFilterP(compressTypeGz, 9));

        cipherBlockFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(manifestWrite)), cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt,
            infoPgCipherPass(infoBackupPg(infoBackup)));

        storageCopyP(manifestRead, manifestWrite);

        // Sync history path if required
        if (storageFeature(storageRepoWrite(), storageFeaturePathSync))
            storagePathSyncP(storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY));

        // Create a symlink to the most recent backup if supported. This link is purely informational for the user and is never used
        // by us since symlinks are not supported on all storage types.
        // -------------------------------------------------------------------------------------------------------------------------
        backupLinkLatest(backupLabel, cfgOptionGroupIdxDefault(cfgOptGrpRepo));

        // Add manifest and save backup.info (infoBackupSaveFile() is responsible for proper syncing)
        // -------------------------------------------------------------------------------------------------------------------------
        infoBackupDataAdd(infoBackup, manifest);

        infoBackupSaveFile(
            infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));

        // Save archive.info/copy so the timestamps will be updated to prevent lifecycle settings from removing the files early
        // -------------------------------------------------------------------------------------------------------------------------
        infoArchiveSaveFile(
            infoArchiveLoadFile(
                storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
                cfgOptionStrNull(cfgOptRepoCipherPass)),
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

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

        // Build block incremental maps using defaults and/or user-specified options
        const ManifestBlockIncrMap blockIncrMap = backupBlockIncrMap();

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
        Manifest *manifestPrior = backupBuildIncrPrior(infoBackup);

        // Perform preliminary copy of full/incr backup
        uint64_t copySizePrelim = 0;

        if (cfgOptionStrId(cfgOptType) == backupTypeFull && cfgOptionBool(cfgOptBackupFullIncr))
        {
            ASSERT(manifestPrior == NULL);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Build the manifest
                Manifest *const manifestPrelim = manifestNewBuild(
                    backupData->storagePrimary, infoPg.version, infoPg.catalogVersion, timestampStart,
                    cfgOptionBool(cfgOptOnline), cfgOptionBool(cfgOptChecksumPage), cfgOptionBool(cfgOptRepoBundle),
                    cfgOptionBool(cfgOptRepoBlock), &blockIncrMap, strLstNewVarLst(cfgOptionLst(cfgOptExclude)),
                    dbTablespaceList(backupData->dbPrimary));

                // Calculate the expected size of the final copy
                uint64_t copySizeFinal = backupManifestCopySize(manifestPrelim);

                // Remove files that do not need to be considered for the preliminary copy because they were modified after the
                // calculated limit time and are therefore likely to be modified during the backup
                time_t timestampCopyStart = backupData->checkpointTime - backupFullIncrLimit(infoBackup);

                manifestBuildFullIncr(
                    manifestPrelim, timestampCopyStart,
                    cfgOptionBool(cfgOptRepoBundle) ? cfgOptionUInt64(cfgOptRepoBundleLimit) : 0);

                // Calculate the expected size of the preliminary copy
                copySizePrelim = backupManifestCopySize(manifestPrelim);

                // If not delta, then reduce final copy size by the prelim copy size
                if (!cfgOptionBool(cfgOptDelta))
                    copySizeFinal -= copySizePrelim;

                // Perform preliminary copy if there are any files to copy
                if (manifestFileTotal(manifestPrelim) > 0)
                {
                    // Report limit of files to be copied in the preliminary copy
                    LOG_INFO_FMT(
                        "full/incr backup preliminary copy of files last modified before %s",
                        strZ(strNewTimeP("%Y-%m-%d %H:%M:%S", timestampCopyStart)));

                    // Wait for replay on the standby to catch up
                    const String *const checkpointLsn = pgLsnToStr(backupData->checkpoint);

                    if (backupData->dbStandby != NULL)
                    {
                        LOG_INFO_FMT("wait for replay on the standby to reach %s", strZ(checkpointLsn));
                        dbReplayWait(
                            backupData->dbStandby, checkpointLsn, backupData->timeline, cfgOptionUInt64(cfgOptArchiveTimeout));
                        LOG_INFO_FMT("replay on the standby reached %s", strZ(checkpointLsn));
                    }

                    // Validate the manifest using the copy start time
                    manifestBuildValidate(
                        manifestPrelim, cfgOptionBool(cfgOptDelta), timestampCopyStart,
                        compressTypeEnum(cfgOptionStrId(cfgOptCompressType)));

                    // Set cipher passphrase (if any)
                    manifestCipherSubPassSet(manifestPrelim, cipherPassGen(cfgOptionStrId(cfgOptRepoCipherType)));

                    // Resume a backup when possible
                    backupResume(manifestPrelim, cipherPassBackup);

                    // Save the manifest before processing starts
                    backupManifestSaveCopy(manifestPrelim, cipherPassBackup, false);

                    // Process the backup manifest
                    backupProcess(backupData, manifestPrelim, true, cipherPassBackup, 0, copySizeFinal);

                    // Move manifest to prior context
                    manifestPrior = manifestMove(manifestPrelim, memContextPrior());
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Start the backup
        const BackupStartResult backupStartResult = backupStart(backupData);

        // Build the manifest
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

        // For a full backup with a preliminary copy do the equivalent of a resume cleanup
        if (cfgOptionStrId(cfgOptType) == backupTypeFull && manifestPrior != NULL)
        {
            LOG_INFO("full/incr backup cleanup");
            manifestDeltaCheck(manifest, manifestPrior);
            backupResumeClean(manifest, manifestPrior, false, varBool(manifestData(manifest)->backupOptionDelta));
            LOG_INFO("full/incr backup final copy");
        }
        // Else normal resume
        else
            backupResume(manifest, cipherPassBackup);

        // Save the manifest before processing starts
        backupManifestSaveCopy(manifest, cipherPassBackup, false);

        // Process the backup manifest
        backupProcess(backupData, manifest, false, cipherPassBackup, copySizePrelim, 0);

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
            "%s backup size = %s, file total = %u", strZ(strIdToStr(manifestData(manifest)->backupType)),
            strZ(strSizeFormat(infoBackupDataByLabel(infoBackup, manifestData(manifest)->backupLabel)->backupInfoSizeDelta)),
            manifestFileTotal(manifest));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
