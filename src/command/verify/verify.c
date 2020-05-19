/* https://github.com/pgbackrest/pgbackrest/issues/1032
Add a command to verify the contents of the repository. By the default the command will:

    * Verify that backup/archive.info and their copies are valid
        - what does this mean? When I load the info file, I don't know which one the system is reading so as long as it can read one, why do I need to check the contents of both? Or do we just want to WARN if only one exists?
        - checkStanzaInfoPg() makes sure the archive and backup info files (not copies just that one or the other) exist and are valid for the database version. BUT this throws an error if can't open one or the other - which I guess is OK because you can't really continue with Verify if either one of these files don't exist, right?
        - OR we just 1) load the archive.info file (errors if neither info or info.copy exist), 2) load the backup.info via infoBackupLoadFileReconstruct (which also errors if backup.info and info.copy are missing and WARNs on additions/deletions), then 3) call checkStanzaInfo() to check that the archive and backup info files pg data match each other, then I guess 4) Check that the version and system id match the current database (this is in checkStanzaInfoPg but that function loads both archive and backup info files and if we want to do a recontruct, this seems wasteful)
        - We should probably check archive and backup history lists match (expire checks that the backup.info history contains at least the same history as archive.info (it can have more)) since we need to get backup db-id needs to be able to translate exactly to the archiveId (i.e. db-id=2, db-version=9.6 in backu.info must translate to archiveId 9.6-2.

    * Reconstruct backup.info and WARN on mismatches
        - infoBackupLoadFileReconstruct() - loads the backup info and updates it by adding valid backups from the repo or removing backups no longer in the repo. This will WARN on additions/deletions, so calling that function should cover it.

    * Check for missing WAL and backup files
        - This would be where we'd need to start the parallel executor, right?
        - At this point, we would have valid info files, so, starting with the WAL, get start/stop list for each archiveId so maybe a structure with archiveID and then a start/stop list for gaps? e.g.
            [0] 9.4-1
                [0] start, stop
                                <--- gap
                [1] start, stop
            [1] 10-2
                [0] start, stop

            BUT what does "missing WAL" mean - there can be gaps so "missing" is only if it is expected to be there for a backup to be consistent, no?
        - The filename always include the WAL segment (timeline?) so all I need is the filename. BUT if I have to verify the checksums, so should I do it while getting the list of WAL?
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

                // Look for files in the archive directory
                StringList *walSubPathList =
                    strLstSort(
                        storageListP(
                            storageRepo(),
                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strPtr(archiveId), strPtr(walPath)),
                            .expression = STRDEF("^[0-F]{24}.*$")),
                        sortOrderAsc);

    * Verify all manifests/copies are valid
        - Again, do we mean both the manifest and manifest.copy have to exist (I think not) or only warn if can read it but one of them is missing?
        - And what do we really mean by "valid"?

    * Verify the checksum of all WAL/backup files
        - Would this be doing mostly what restoreFile() does without the actualy copy? But we have to "sort-of" copy it (you mentioned a technique where we can just throw it away and not write to disk) to get the correct size and checksum...

    * Ignore files with references when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if ask for an incremental backup verify, then the manifest may have references to files in another (prior) backup so go check them.

The command can be modified with the following options:

    --set - only verify the specified backup and associated WAL (including PITR)
    --no-pitr only verify the WAL required to make the backup consistent.
    --fast - check only that the file is present and has the correct size.

We would like these additional features, but are willing to release the first version without them:

    WARN on extra files
    Check that history files exist for timelines after the first (the first timeline will not have a history file if it is the oldest timeline ever written to the repo)
    Check that copies of the manifests exist in backup.history

Questions/Concerns
- How much crossover with the Check command are we anticipating?
- The command can be run locally or remotely, so from any pgbackrest host, right?
- What is the relationship between the local.c, protocol.c and the file.c functions? Can you have one of these but not the other - like is one just for the ability to run the command remotely from any pgbackrest host and the other for forking/spawning child process jobs?
- Data.pm:
    * Will we be verifying links? Does the cmd_verify need to be in Data.pm CFGOPT_LINK_ALL, CFGOPT_LINK_MAP
    * Same question for tablespaces
- We don't have a checksum for WAL files so we're just checking for existence and gaps, right?
- How to check WAL for PITR (e.g. after end of last backup?)? When async archiving, the WAL steam could have gaps as WAL might still be in the process of being pushed, so do we accept gaps? do we WARN? do we say if last one is less than 5 minutes OLD then we're probably still waiting for the WAL so ok?
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
//     // Also need a list of WAL or WAL directories to check and an index to indicate where we are in the list
//     // Also need a list of Backup directories to check and an index to indicate where we are in the list
// } VerifyData;
//
// static ProtocolParallelJob *
// verifyCallback(void *data, unsigned int clientIdx)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM_P(VOID, data);
//         FUNCTION_TEST_PARAM(UINT, clientIdx);
//     FUNCTION_TEST_END();
//
//     ASSERT(data != NULL); -- or can it be - like when there is no more data
//
//     // No special logic based on the client, we'll just get the next job
//     (void)clientIdx;
//
//     // Get a new job if there are any left
//     VerifyData *jobData = data;
//
//     if (jobData->walSegmentIdx < strLstSize(jobData->walSegmentList))
//     {
//         const String *walSegment = strLstGet(jobData->walSegmentList, jobData->walSegmentIdx);
//         jobData->walSegmentIdx++;
//
//         ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR);
//         protocolCommandParamAdd(command, VARSTR(walSegment));
//
//         FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
//     }
//
//     FUNCTION_TEST_RETURN(NULL);
// }

void
cmdVerify(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // CSHANG START Will any of these be required?
        // Get information for the current user
        // userInit();
        //
        // // PostgreSQL must be local
        // pgIsLocalVerify();
        //
        // // Validate restore path
        // restorePathValidate();
        // CSHANG END

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // // Create the parallel executor
        // ProtocolParallel *parallelExec = protocolParallelNew(
        //     (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2, verifyJobCallback, &jobData);
        //
        // // If a fast option has been requested, then only create one process to handle, else create as many as process-max
        // unsigned int numProcesses = cfgOptionTest(cfgOptFast) ? 1 : cfgOptionUInt(cfgOptProcessMax);
        //
        // for (unsigned int processIdx = 1; processIdx <= numProcesses; processIdx++)
        //     protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 1, processIdx));
        //
        // do
        // {
        //     unsigned int completed = protocolParallelProcess(parallelExec);
        //
        //     for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
        //     {
        //     }
        // }
        // while (!protocolParallelDone(parallelExec));

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
