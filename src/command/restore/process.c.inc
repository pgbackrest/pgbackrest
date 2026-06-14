/***********************************************************************************************************************************
Generate a list of queues that determine the order of file processing
***********************************************************************************************************************************/
// Comparator to order ManifestFile objects by size then name
static const Manifest *restoreProcessQueueComparatorManifest = NULL;

static int
restoreProcessQueueComparator(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    // Unpack files
    const ManifestFile file1 = manifestFileUnpack(restoreProcessQueueComparatorManifest, *(const ManifestFilePack *const *)item1);
    const ManifestFile file2 = manifestFileUnpack(restoreProcessQueueComparatorManifest, *(const ManifestFilePack *const *)item2);

    // Zero length files should be ordered at the end
    if (file1.size == 0)
    {
        if (file2.size != 0)
            FUNCTION_TEST_RETURN(INT, 1);
    }
    else if (file2.size == 0)
        FUNCTION_TEST_RETURN(INT, -1);

    // If the bundle id differs that is enough to determine order
    if (file1.bundleId < file2.bundleId)
        FUNCTION_TEST_RETURN(INT, -1);
    else if (file1.bundleId > file2.bundleId)
        FUNCTION_TEST_RETURN(INT, 1);

    // If the bundle ids are 0
    if (file1.bundleId == 0)
    {
        // If the size differs then that's enough to determine order
        if (file1.size < file2.size)
            FUNCTION_TEST_RETURN(INT, 1);
        else if (file1.size > file2.size)
            FUNCTION_TEST_RETURN(INT, -1);

        // If size is the same then use name to generate a deterministic ordering (names must be unique)
        ASSERT(!strEq(file1.name, file2.name));
        FUNCTION_TEST_RETURN(INT, strCmp(file2.name, file1.name));
    }

    // If the reference differs that is enough to determine order
    if (file1.reference == NULL)
    {
        if (file2.reference != NULL)
            FUNCTION_TEST_RETURN(INT, 1);
    }
    else if (file2.reference == NULL)
        FUNCTION_TEST_RETURN(INT, -1);
    else
    {
        const int backupLabelCmp = strCmp(file2.reference, file1.reference) * -1;

        if (backupLabelCmp != 0)
            FUNCTION_TEST_RETURN(INT, backupLabelCmp);
    }

    // Finally order by bundle offset
    ASSERT(file1.bundleOffset != file2.bundleOffset);

    if (file1.bundleOffset < file2.bundleOffset)
        FUNCTION_TEST_RETURN(INT, -1);

    FUNCTION_TEST_RETURN(INT, 1);
}

static uint64_t
restoreProcessQueue(const Manifest *const manifest, List **const queueList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(LIST, queueList);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(manifest != NULL);

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create list of process queues (use void * instead of List * to avoid Coverity false positive)
        *queueList = lstNewP(sizeof(void *));

        // Generate the list of processing queues (there is always at least one)
        StringList *const targetList = strLstNew();
        strLstAddZ(targetList, MANIFEST_TARGET_PGDATA "/");

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

            if (target->tablespaceId != 0)
                strLstAddFmt(targetList, "%s/", strZ(target->name));
        }

        // Generate the processing queues
        MEM_CONTEXT_BEGIN(lstMemContext(*queueList))
        {
            for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            {
                List *const queue = lstNewP(sizeof(ManifestFile *), .comparator = restoreProcessQueueComparator);
                lstAdd(*queueList, &queue);
            }
        }
        MEM_CONTEXT_END();

        // Now put all files into the processing queues
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFilePack *const filePack = manifestFilePackGet(manifest, fileIdx);
            const ManifestFile file = manifestFileUnpack(manifest, filePack);

            // Find the target that contains this file
            unsigned int targetIdx = 0;

            do
            {
                // A target should always be found
                CHECK(FormatError, targetIdx < strLstSize(targetList), "backup target not found");

                if (strBeginsWith(file.name, strLstGet(targetList, targetIdx)))
                    break;

                targetIdx++;
            }
            while (1);

            // Add file to queue
            lstAdd(*(List **)lstGet(*queueList, targetIdx), &filePack);

            // Add size to total
            result += file.size;
        }

        // Sort the queues
        restoreProcessQueueComparatorManifest = manifest;

        for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            lstSort(*(List **)lstGet(*queueList, targetIdx), sortOrderAsc);

        // Move process queues to prior context
        lstMove(*queueList, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT64, result);
}

/***********************************************************************************************************************************
Log the results of a job and throw errors
***********************************************************************************************************************************/
// Helper function to determine if a file should be zeroed
static bool
restoreFileZeroed(const String *const manifestName, RegExp *const zeroExp)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, manifestName);
        FUNCTION_TEST_PARAM(REGEXP, zeroExp);
    FUNCTION_TEST_END();

    ASSERT(manifestName != NULL);

    FUNCTION_TEST_RETURN(
        BOOL,
        zeroExp == NULL ? false : regExpMatch(zeroExp, manifestName) && !strEndsWith(manifestName, STRDEF("/" PG_FILE_PGVERSION)));
}

// Helper function to construct the absolute pg path for any file. Add a temp extension to pg_control so a partially restored
// cluster cannot be started.
static String *
restoreFilePgPath(const Manifest *const manifest, const String *const manifestName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, manifestName);
    FUNCTION_TEST_END();

    ASSERT(manifest != NULL);
    ASSERT(manifestName != NULL);

    String *const pathPg = manifestPathPg(manifestName);
    String *const result = strNewFmt(
        "%s/%s%s", strZ(manifestTargetBase(manifest)->path), strZ(pathPg),
        strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL) ? "." STORAGE_FILE_TEMP_EXT : "");

    strFree(pathPg);

    FUNCTION_TEST_RETURN(STRING, result);
}

static uint64_t
restoreJobResult(
    const Manifest *const manifest, ProtocolParallelJob *const job, RegExp *const zeroExp, const uint64_t sizeTotal,
    uint64_t sizeRestored, unsigned int *const currentPercentComplete)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, job);
        FUNCTION_LOG_PARAM(REGEXP, zeroExp);
        FUNCTION_LOG_PARAM(UINT64, sizeTotal);
        FUNCTION_LOG_PARAM(UINT64, sizeRestored);
        FUNCTION_LOG_PARAM_P(UINT, currentPercentComplete);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    // The job was successful
    if (protocolParallelJobErrorCode(job) == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            PackRead *const jobResult = protocolParallelJobResult(job);
            unsigned int percentComplete = 0;

            while (!pckReadNullP(jobResult))
            {
                const ManifestFile file = manifestFileFind(manifest, pckReadStrP(jobResult));
                const bool zeroed = restoreFileZeroed(file.name, zeroExp);
                const RestoreResult result = (RestoreResult)pckReadU32P(jobResult);
                const uint64_t blockIncrDeltaSize = pckReadU64P(jobResult);

                String *const log = strCatZ(strNew(), "restore");

                // Note if file was zeroed (i.e. selective restore)
                if (zeroed)
                    strCatZ(log, " zeroed");

                // Add filename
                strCatFmt(log, " file %s", strZ(restoreFilePgPath(manifest, file.name)));

                // If preserved add details to explain why it was not copied or zeroed
                if (result == restoreResultPreserve)
                {
                    strCatZ(log, " - ");

                    // On force we match on size and modification time
                    if (cfgOptionBool(cfgOptForce))
                    {
                        strCatFmt(
                            log, "exists and matches size %" PRIu64 " and modification time %" PRIu64, file.size,
                            (uint64_t)file.timestamp);
                    }
                    // Else a checksum delta or file is zero-length
                    else
                    {
                        strCatZ(log, "exists and ");

                        // No need to copy zero-length files
                        if (file.size == 0)
                        {
                            strCatZ(log, "is zero size");
                        }
                        // The file matched the manifest checksum so did not need to be copied
                        else
                            strCatZ(log, "matches backup");
                    }
                }

                // Add bundle info
                strCatZ(log, " (");

                if (file.bundleId != 0)
                {
                    ASSERT(varUInt64(protocolParallelJobKey(job)) == file.bundleId);

                    strCatZ(log, "bundle ");

                    if (file.reference != NULL)
                        strCatFmt(log, "%s/", strZ(file.reference));

                    strCatFmt(log, "%" PRIu64 "/%" PRIu64 ", ", file.bundleId, file.bundleOffset);
                }

                // Add block incremental delta size, i.e. amount of the file that block incremental updated
                if (file.blockIncrMapSize != 0 && result != restoreResultPreserve)
                {
                    strCatZ(log, "bi ");

                    if (blockIncrDeltaSize != file.size)
                        strCatFmt(log, "%s/", strZ(strSizeFormat(blockIncrDeltaSize)));
                }

                // Add size and percent complete
                sizeRestored += file.size;

                // Store percentComplete as an integer (used to update progress in the lock file)
                percentComplete = cvtPctToUInt(sizeRestored, sizeTotal);

                strCatFmt(log, "%s, %s)", strZ(strSizeFormat(file.size)), strZ(strNewPct(sizeRestored, sizeTotal)));

                // If not zero-length add the checksum
                if (file.size != 0 && !zeroed)
                    strCatFmt(log, " checksum %s", strZ(strNewEncode(encodingHex, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE))));

                LOG_DETAIL_PID(protocolParallelJobProcessId(job), strZ(log));
            }

            // Update currentPercentComplete and lock file when the change is significant enough
            if (percentComplete - *currentPercentComplete > 10)
            {
                *currentPercentComplete = percentComplete;
                cmdLockWriteP(
                    .percentComplete = VARUINT(*currentPercentComplete), .sizeComplete = VARUINT64(sizeRestored),
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

    FUNCTION_LOG_RETURN(UINT64, sizeRestored);
}

/***********************************************************************************************************************************
Return new restore jobs as requested
***********************************************************************************************************************************/
typedef struct RestoreJobData
{
    unsigned int repoIdx;                                           // Internal repo idx
    Manifest *manifest;                                             // Backup manifest
    List *queueList;                                                // List of processing queues
    RegExp *zeroExp;                                                // Identify files that should be sparse zeroed
    const String *cipherSubPass;                                    // Passphrase used to decrypt files in the backup
    const String *rootReplaceUser;                                  // User to replace invalid users when root
    const String *rootReplaceGroup;                                 // Group to replace invalid group when root
} RestoreJobData;

// Helper to calculate the next queue to scan based on the client index
static int
restoreJobQueueNext(const unsigned int clientIdx, int queueIdx, const unsigned int queueTotal)
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

// Callback to fetch restore jobs for the parallel executor
static ProtocolParallelJob *
restoreJobCallback(void *const data, const unsigned int clientIdx)
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
        RestoreJobData *const jobData = data;

        // Determine where to begin scanning the queue (we'll stop when we get back here)
        PackWrite *param = NULL;
        int queueIdx = (int)(clientIdx % lstSize(jobData->queueList));
        const int queueEnd = queueIdx;

        // Create restore job
        do
        {
            List *const queue = *(List **)lstGet(jobData->queueList, (unsigned int)queueIdx);
            bool fileAdded = false;
            const String *fileName = NULL;
            uint64_t bundleId = 0;
            const String *reference = NULL;

            while (!lstEmpty(queue))
            {
                const ManifestFile file = manifestFileUnpack(jobData->manifest, *(ManifestFilePack **)lstGet(queue, 0));

                // Break if bundled files have already been added and 1) the bundleId has changed or 2) the reference has changed
                if (fileAdded && (bundleId != file.bundleId || !strEq(reference, file.reference)))
                    break;

                // Add common parameters before first file
                if (param == NULL)
                {
                    param = protocolPackNew();

                    if (file.bundleId != 0)
                    {
                        bundleId = file.bundleId;
                        reference = file.reference;
                    }
                    else
                        fileName = file.name;

                    pckWriteStrP(
                        param,
                        backupFileRepoPathP(
                            file.reference != NULL ? file.reference : manifestData(jobData->manifest)->backupLabel,
                            .manifestName = file.name, .bundleId = file.bundleId,
                            .compressType = manifestData(jobData->manifest)->backupOptionCompressType,
                            .blockIncr = file.blockIncrMapSize != 0));
                    pckWriteU32P(param, jobData->repoIdx);
                    pckWriteU32P(param, manifestData(jobData->manifest)->backupOptionCompressType);
                    pckWriteTimeP(param, manifestData(jobData->manifest)->backupTimestampCopyStart);
                    pckWriteBoolP(param, cfgOptionBool(cfgOptDelta));
                    pckWriteBoolP(param, cfgOptionBool(cfgOptDelta) && cfgOptionBool(cfgOptForce));
                    pckWriteBoolP(param, file.bundleId != 0 && manifestData(jobData->manifest)->bundleRaw);
                    pckWriteStrP(param, jobData->cipherSubPass);
                    pckWriteStrLstP(param, manifestReferenceList(jobData->manifest));

                    fileAdded = true;
                }

                pckWriteStrP(param, restoreFilePgPath(jobData->manifest, file.name));
                pckWriteBinP(param, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE));
                pckWriteU64P(param, file.size);
                pckWriteTimeP(param, file.timestamp);
                pckWriteModeP(param, file.mode);
                pckWriteBoolP(param, restoreFileZeroed(file.name, jobData->zeroExp));
                pckWriteStrP(param, restoreManifestOwnerReplace(file.user, jobData->rootReplaceUser));
                pckWriteStrP(param, restoreManifestOwnerReplace(file.group, jobData->rootReplaceGroup));

                // If block incremental then modify offset and size to where the map is stored since we need to read that first.
                if (file.blockIncrMapSize != 0)
                {
                    pckWriteBoolP(param, true);
                    pckWriteU64P(param, file.bundleOffset + file.sizeRepo - file.blockIncrMapSize);
                    pckWriteU64P(param, file.blockIncrMapSize);
                }
                // Else write bundle offset/size
                else if (file.bundleId != 0)
                {
                    pckWriteBoolP(param, true);
                    pckWriteU64P(param, file.bundleOffset);
                    pckWriteU64P(param, file.sizeRepo);
                }
                // Else restore as a whole file
                else
                    pckWriteBoolP(param, false);

                // Block incremental
                pckWriteU64P(param, file.blockIncrMapSize);

                if (file.blockIncrMapSize != 0)
                {
                    pckWriteU64P(param, file.blockIncrSize);
                    pckWriteU64P(param, file.blockIncrChecksumSize);
                }

                pckWriteStrP(param, file.name);

                // Remove job from the queue
                lstRemoveIdx(queue, 0);

                // Break if the file is not bundled
                if (bundleId == 0)
                    break;
            }

            if (fileAdded)
            {
                // Assign job to result
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = protocolParallelJobNew(
                        bundleId != 0 ? VARUINT64(bundleId) : VARSTR(fileName), PROTOCOL_COMMAND_RESTORE_FILE, param);
                }
                MEM_CONTEXT_PRIOR_END();

                break;
            }

            queueIdx = restoreJobQueueNext(clientIdx, queueIdx, lstSize(jobData->queueList));
        }
        while (queueIdx != queueEnd);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(PROTOCOL_PARALLEL_JOB, result);
}
