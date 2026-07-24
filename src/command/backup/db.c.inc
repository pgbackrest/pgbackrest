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
                "execute backup start: backup begins after the %s checkpoint completes",
                cfgOptionBool(cfgOptStartFast) ? "requested immediate" : "next regular");

            const DbBackupStartResult dbBackupStartResult = dbBackupStart(
                backupData->dbPrimary, cfgOptionBool(cfgOptStartFast), cfgOptionBool(cfgOptArchiveCheck));

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
                "execute backup stop and wait for all WAL segments to archive");

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
