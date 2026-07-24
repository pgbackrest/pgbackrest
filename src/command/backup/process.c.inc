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

                // Store percentComplete as an integer (used to update progress in the lock file)
                percentComplete = cvtPctToUInt(*sizeProgress, sizeTotal);

                strCatFmt(logProgress, "%s, %s", strZ(strSizeFormat(copySize)), strZ(strNewPct(*sizeProgress, sizeTotal)));

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
                                const char *const plural = errorTotalMin > 1 ? "s" : "";

                                // ??? Update formatting after migration
                                LOG_WARN_FMT(
                                    "invalid page checksum%s found in file %s at page%s %s", plural, strZ(fileLog), plural,
                                    strZ(error));
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
Build the processing queues and generate jobs
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

// Comparator to order files in a backup processing queue. Unbundled files (too large to bundle) sort first, largest first, so the
// longest copies start early and processes are less likely to stall at the end of the backup. Bundled files follow, ordered to
// mirror the layout of the prior backups they reference so files keep the same relative order from one backup to the next. That
// lets block incremental map reads (and reads of whole files in prior bundles) run in sequence rather than seeking back and forth.
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

    // A file is bundled when bundling is enabled and it is within the bundle limit; larger files are stored unbundled
    const bool file1Bundle = backupProcessQueueComparatorBundle && file1.size <= backupProcessQueueComparatorBundleLimit;
    const bool file2Bundle = backupProcessQueueComparatorBundle && file2.size <= backupProcessQueueComparatorBundleLimit;

    // Order unbundled files (each copied as its own job) before bundled files
    if (file1Bundle != file2Bundle)
        FUNCTION_TEST_RETURN(INT, file1Bundle ? 1 : -1);

    // Order unbundled files by size descending so larger files are processed first. This helps prevent stalls at the end of the
    // backup where one process is still copying a large file after the others are idle. This only matters when process-max > 1.
    if (!file1Bundle)
    {
        if (file1.size < file2.size)
            FUNCTION_TEST_RETURN(INT, 1);
        else if (file1.size > file2.size)
            FUNCTION_TEST_RETURN(INT, -1);

        // If size is equal then use name to generate a deterministic ordering (names are unique)
        FUNCTION_TEST_RETURN(INT, strCmp(file1.name, file2.name));
    }

    // The rest are bundled files. Order first by reference (the prior backup the file is based on) so files based on the same prior
    // backup are grouped and processed in the order those backups were created. New files have no reference and sort last.
    if (file1.reference == NULL)
    {
        if (file2.reference != NULL)
            FUNCTION_TEST_RETURN(INT, 1);
    }
    else if (file2.reference == NULL)
        FUNCTION_TEST_RETURN(INT, -1);
    else
    {
        const int backupLabelCmp = strCmp(file1.reference, file2.reference);

        if (backupLabelCmp != 0)
            FUNCTION_TEST_RETURN(INT, backupLabelCmp);

        // Then by bundle id so files that shared a bundle in the prior backup stay grouped. Bundle id and offset only apply to
        // files based on the same prior backup so they are ordered here rather than for new files, which have no prior location.
        if (file1.bundleId < file2.bundleId)
            FUNCTION_TEST_RETURN(INT, -1);
        else if (file1.bundleId > file2.bundleId)
            FUNCTION_TEST_RETURN(INT, 1);

        // Then by offset within that bundle so prior maps are read in the order they are stored
        if (file1.bundleOffset < file2.bundleOffset)
            FUNCTION_TEST_RETURN(INT, -1);
        else if (file1.bundleOffset > file2.bundleOffset)
            FUNCTION_TEST_RETURN(INT, 1);
    }

    // Order new files eligible for bundling by time ascending so files of a similar age are bundled together. A full backup has no
    // references so all its bundled files use this ordering.
    if (file1.timestamp < file2.timestamp)
        FUNCTION_TEST_RETURN(INT, -1);
    else if (file1.timestamp > file2.timestamp)
        FUNCTION_TEST_RETURN(INT, 1);

    // If everything above is equal then use name to generate a deterministic ordering (names must be unique)
    FUNCTION_TEST_RETURN(INT, strCmp(file1.name, file2.name));
}

// Helper to generate the backup queues
static uint64_t
backupProcessQueue(const BackupData *const backupData, Manifest *const manifest, BackupJobData *const jobData)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(VOID, jobData);
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
        if (!pgControlFound && cfgOptionBool(cfgOptOnline))
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
            lstSort(*(List **)lstGet(jobData->queueList, queueIdx), sortOrderAsc);

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

                    if (file.blockIncrMapSize != 0 && !file.resume)
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

/***********************************************************************************************************************************
Process the backup manifest
***********************************************************************************************************************************/
static void
backupProcess(const BackupData *const backupData, Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

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
        if ((backupType == backupTypeFull && !jobData.bundle) || hardLink)
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
        sizeTotal = backupProcessQueue(backupData, manifest, &jobData);

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
        uint64_t sizeProgress = 0;

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
        const char *const compressExt = strZ(compressExtStr(jobData.compressType));

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile file = manifestFile(manifest, fileIdx);

            // If the file has a reference, then it was not copied since it can be retrieved from the referenced backup. However,
            // if hardlinking is enabled the link will need to be created.
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
                // Else log the reference. With delta, it is possible that references may have been removed if a file needed to be
                // recopied.
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

                // Always sync the path if it exists or if the backup is full (without bundling) or hardlinked. In the latter cases
                // the directory should always exist so we want to error if it does not.
                if ((backupType == backupTypeFull && !jobData.bundle) || hardLink || storagePathExistsP(storageRepo(), path))
                    storagePathSyncP(storageRepoWrite(), path);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
