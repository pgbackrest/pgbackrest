/* https://github.com/pgbackrest/pgbackrest/issues/1032
Add a command to verify the contents of the repository. By the default the command will:

    * Verify that backup/archive.info and their copies are valid
        - May have to expose infoBackupNewLoad so can load the backu.info and then the backup.inco.copy and compare the 2 - need to know if one is corrupt and alwaysfailiing to the other.
        - want to WARN if only one exists
        - If backup.info/copy AND/OR archive.info/copy is missing then do not error. ONLY error if encrypted since we can't read the archive dir or backup dirs
        - checkStanzaInfoPg() makes sure the archive and backup info files (not copies just that one or the other) exist and are valid for the database version - BUT the db being available is not allowed because restore doesn't require it - so maybe just check archive and backup info DB sections match each other. BUT this throws an error if can't open one or the other - so would need to catch this and do LOG_ERROR() - maybe prefix with VERIFY-ERROR - or just take out the conditions and check for what we need OR maybe call checkStanzaInfo
        - We should probably check archive and backup history lists match (expire checks that the backup.info history contains at least the same history as archive.info (it can have more)) since we need to get backup db-id needs to be able to translate exactly to the archiveId (i.e. db-id=2, db-version=9.6 in backu.info must translate to archiveId 9.6-2.
        - If archive-copy set the WAL can be in the backup dirs so just because archive.info is missing, don't want to error

    * Reconstruct backup.info and WARN on mismatches
        - then load the backup.info via infoBackupLoadFileReconstruct (which also errors if backup.info and info.copy are missing and WARNs on additions/deletions) so need to catch again or not bother to even call it, then 3) call checkStanzaInfo() to check that the archive and backup info files pg data match each other. This will WARN on additions/deletions, so calling that function should cover it.

    * Check for missing WAL and backup files
        - This would be where we'd need to start the parallel executor, right? YES
        - At this point, we would have valid info files, so, starting with the WAL, get start/stop list for each archiveId so maybe a structure with archiveID and then a start/stop list for gaps? e.g.
            [0] 9.4-1
                [0] start, stop
                                <--- gap
                [1] start, stop
            [1] 10-2
                [0] start, stop

            BUT what does "missing WAL" mean - there can be gaps so "missing" is only if it is expected to be there for a backup to be consistent
        - The filename always include the WAL segment (timeline?) so all I need is the filename. BUT if I have to verify the checksums, so should I do it in the verifyFile function
        - Is there a better way to get the list of WAL files than in 3 steps below?
                // Get a list of archive directories (e.g. 9.4-1, 10-2, etc) sorted by the db-id (number after the dash).
                StringList *listArchiveDisk = strLstSort(
                    strLstComparatorSet(
                        storageListP(storageRepo(), STORAGE_REPO_ARCHIVE_STR, .expression = STRDEF(REGEX_ARCHIVE_DIR_DB_VERSION)),
                        archiveIdComparator),
                    sortOrderAsc);

                // Get all major archive paths (timeline and first 32 bits of LSN)
                StringList *walPathList =
                    strLstSort(
                        storageListP(
                            storageRepo(), strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strPtr(archiveId)),
                            .expression = STRDEF(WAL_SEGMENT_DIR_REGEXP)),
                        sortOrderAsc);

                // Look for files in the archive directory CSHANG -- MAKE USE THE EXPRESSION BELOW IS FOR THE FILE NAME
                StringList *walFileList =
                    strLstSort(
                        storageListP(
                            storageRepo(),
                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath)),
                            .expression = STRDEF("^[0-F]{24}.*$")),
                        sortOrderAsc);

    * Verify all manifests/copies are valid
        - Both the manifest and manifest.copy exist, loadable and are identical. WARN if any condition is false

    * Verify the checksum of all WAL/backup files
        - Pass the checksum to verifyFie - this needs to be stripped off from file in WAL but for backup it must be read from the manifest (which we need to read in the jobCallback
        - Skip checking size for WAL file but should check for size equality with the backup files - if one or the other doesn't match, then corrupt
        - in verifyFile() function be sure to add IoSync filter and then do IoReadDrain and then look at the results of the filters. See restore/file.c lines 85 though 87 (although need more filters e.g. decrypt, decompress, size, sha, sync).
                IoFilterGroup *filterGroup = ioWriteFilterGroup(storageReadIo(repoRead));

                // Add decryption filter
                if (cipherPass != NULL)
                {
                    ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));
                    compressible = false;
                }

                // Add decompression filter
                if (repoFileCompressType != compressTypeNone)
                {
                    ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));
                    compressible = false;
                }

                // Add sha1 filter
                ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

                // Add size filter
                ioFilterGroupAdd(filterGroup, ioSizeNew());

                // Ad IoSync

        - For the first pass we only check that all the files in the manifest are on disk. Future we may also WARN if there are files that are on disk but not in the manifest.
        - verifyJobResult() would check the status of the file (bad) and can either add to a corrupt list or add gaps in the WAL (probaby former) - I need to know what result I was coming from so that is the jobKey - maybe we use the filename as the key - does it begin with Archive then it is WAL, else it's a backup (wait, what about manifest) - maybe we can make the key a variant instead of a string. Need list of backups start/stop to be put into jobData and use this for final reconciliation.
        - Would this be doing mostly what restoreFile() does without the actualy copy? But we have to "sort-of" copy it (you mentioned a technique where we can just throw it away and not write to disk) to get the correct size and checksum...
        - If a corrupt WAL file then warn/log that we have a "corrupt/missing" file and remove it from the range list so it will cause a "gap" then when checking backup it will log another error that the backup that relies on that particular WAL is not valid

    * Ignore files with references when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if ask for an incremental backup verify, then the manifest may have references to files in another (prior) backup so go check them.

The command can be modified with the following options:

    --set - only verify the specified backup and associated WAL (including PITR)
    --no-pitr only verify the WAL required to make the backup consistent.
    --fast - check only that the file is present and has the correct size.

We would like these additional features, but are willing to release the first version without them:

    * WARN on extra files
    * Check that history files exist for timelines after the first (the first timeline will not have a history file if it is the oldest timeline ever written to the repo)
    * Check that copies of the manifests exist in backup.history

Questions/Concerns
- How much crossover with the Check command are we anticipating?
- The command can be run locally or remotely, so from any pgbackrest host, right? YES
- Data.pm: ALL OF THESE ARE NO
    * Will we be verifying links? Does the cmd_verify need to be in Data.pm CFGOPT_LINK_ALL, CFGOPT_LINK_MAP
    * Same question for tablespaces
- How to check WAL for PITR (e.g. after end of last backup?)? ==> If doing async archive and process max = 8 then could be 8 missing. But we don't have access to process-max and we don't know if they had asynch archiving, so if we see gaps in the last 5 minutes of the WAL stream and the last backup stop WAL is there, then we'll just ignore that PITR has gaps. MUST always assume is we don't have access to the configuration.
*/
/**********************************************************************************************************************************/
// typedef enum
// {
//     verifyWal,
//     verifyBackup,
// } VerifyState;
//
// typedef struct VerifyData
// {
//     bool fast;                                                      // Has the fast option been requested
//     VerifyState state;

        StringList *archiveIdList;
        StringList *walPathList;
        StringList *walFileList;

        StringList *backupList;
        StringList *backupFileList;
//     // Also need a list of WAL or WAL directories to check and an index to indicate where we are in the list
//     // Also need a list of Backup directories to check and an index to indicate where we are in the list
// } VerifyData;

static ProtocolParallelJob *
verifyCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    ASSERT(data != NULL); -- or can it be - like when there is no more data

    // No special logic based on the client, we'll just get the next job
    (void)clientIdx;

    // Get a new job if there are any left
    VerifyData *jobData = data;
if archiveIDlist ==Null
    go get a list of archiveid.
    done.

if (walPathList size == 0)
    go read the walpath
    add all walPathlist
    remove archiveid from list

if walPathlist > 0
    go read the walPathlist[i]
    add all walFilelist
    Here we check for gaps? Then when check for backup consistency, we warn if a specific backup can't do pitr or can do pitr
    remove walPathlist[i] from list

maybe have some info log (like "checking archinve id 9.4-1" and then detail level logging could in addition list the archive files being checked).

    if (walFilelist)
    {
        process the 1 walFile from the list
        remove that from the list
        const String *walSegment = strLstGet(jobData->walSegmentList, jobData->walSegmentIdx);
        jobData->walSegmentIdx++;

        ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR);
        protocolCommandParamAdd(command, VARSTR(walSegment));

        FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
    }
else // OR maybe just have a verifyFile - may be that it just takes a file name and it doesn't know what type of file it is - like pass it the compress type, encryption, etc - whatever it needs to read the file. Make the verifyFile dumb. There is a funtion to get the compression type. BUT can't test compression on the files in backup because need to read the manifest to determine that.
    if (backupList)
        {
            next backup thing to process

            FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
        }

    FUNCTION_TEST_RETURN(NULL);
}

verifyJobResult(...)
{
    // Check that the job was successful
    if (protocolParallelJobErrorCode(job) == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            THISMAYBEASTRUCT result = varSOMETHING(protocolParallelJobResult(job));
// CSHANG This is just an example - we need to do logging based on INFO for what we're checking, DETAIL for each files checked, then BACKUP success, WAL success would also be INFO, WARN logLevelWarn or ERROR logLevelError
            LOG_PID(logLevelInfo, protocolParallelJobProcessId(job), 0, strPtr(log));
        }
        MEM_CONTEXT_TEMP_END();

        // Free the job
        protocolParallelJobFree(job);
    }
    else
        THROW_CODE(protocolParallelJobErrorCode(job), strPtr(protocolParallelJobErrorMessage(job)));
}

verifyFileCompare(String *fileName ==> or INFO_BACKUP_PATH_FILE_STR)
{
            // Verify that backup/archive.info and their copies are valid
            StorageRead *read = storageNewReadP(storageRepo(), backupInfoFile);
            IoRead *backupInfoRead = storageReadIo(read);
            cipherBlockFilterGroupAdd(ioReadFilterGroup(backupInfoRead), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherModeDecrypt, cfgOptionStrNull(cfgOptRepoCipherPass));
            ioFilterGroupAdd(ioReadFilterGroup(backupInfoRead), cryptoHashNew(HASH_TYPE_SHA1_STR));
            // CSHANG This needs to be inside a TRY block - might not get to the sha1sum if we can't load the file
            InfoBackup *infoBackup = infoBackupNewLoad(backupInfoRead);

            // This checksum is what we should use to check the main file against the copy to make sure the 2 files are identical
            String *sha1sum = varStr(ioFilterGroupResult(backupInfoRead, CRYPTO_HASH_FILTER_TYPE_STR));
}

void
cmdVerify(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        String *backupInfoFile = strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strPtr(stanzaListName), INFO_BACKUP_FILE);
// CSHANG START This will be in a function verifyFileCompare
        // Verify that backup/archive.info and their copies are valid
        StorageRead *read = storageNewReadP(storageRepo(), backupInfoFile);
        IoRead *backupInfoRead = storageReadIo(read);
        cipherBlockFilterGroupAdd(ioReadFilterGroup(backupInfoRead), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherModeDecrypt, cfgOptionStrNull(cfgOptRepoCipherPass));
        ioFilterGroupAdd(ioReadFilterGroup(backupInfoRead), cryptoHashNew(HASH_TYPE_SHA1_STR));
        // CSHANG This needs to be inside a TRY block - might not get to the sha1sum if we can't load the file
        InfoBackup *infoBackup = infoBackupNewLoad(backupInfoRead);

        // This checksum is what we should use to check the main file against the copy to make sure the 2 files are identical
        String *sha1sum = varStr(ioFilterGroupResult(backupInfoRead, CRYPTO_HASH_FILTER_TYPE_STR));
// CSHANG END

// CSHANG jobData should be preprapared list of things to check - e.g. list of backups, list of archive ids
        // Create the parallel executor
        ProtocolParallel *parallelExec = protocolParallelNew(
            (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2, verifyJobCallback, &jobData);

        // If a fast option has been requested, then only create one process to handle, else create as many as process-max
        unsigned int numProcesses = cfgOptionTest(cfgOptFast) ? 1 : cfgOptionUInt(cfgOptProcessMax);

        for (unsigned int processIdx = 1; processIdx <= numProcesses; processIdx++)
            protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 1, processIdx));

        do
        {
            unsigned int completed = protocolParallelProcess(parallelExec);

            for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
            {

                    // Get the job and job key
                    // CSHANG Here the processId would be used for logging
                    ProtocolParallelJob *job = protocolParallelResult(parallelExec);
                    unsigned int processId = protocolParallelJobProcessId(job);
                    const String *walSegment = varStr(protocolParallelJobKey(job));

                    // The job was successful
                    if (protocolParallelJobErrorCode(job) == 0)
                    {
            }
        }
        while (!protocolParallelDone(parallelExec));

        // HERE we will need to do the final reconciliation - checking backup required WAL against, valid WAL

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
