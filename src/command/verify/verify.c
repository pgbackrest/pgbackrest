/* https://github.com/pgbackrest/pgbackrest/issues/1032
Add a command to verify the contents of the repository. By the default the command will:

    Verify that backup/archive.info and their copies are valid
    Reconstruct backup.info and WARN on mismatches
    Check for missing WAL and backup files
    Verify all manifests/copies are valid
    Verify the checksum of all WAL/backup files
    Ignore files with references when all backups are validated, otherwise, check each file referenced to another file in the backup set. For example, if ask for an incremental backup verify, then the manifest may have references to files in another (prior) backup so go check them.

The command can be modified with the following options:

    --set - only verify the specified backup and associated WAL (including PITR)
    --no-pitr only verify the WAL required to make the backup consistent.
    --fast - check only that the file is present and has the correct size.

We would like these additional features, but are willing to release the first version without them:

    WARN on extra files
    Check that history files exist for timelines after the first (the first timeline will not have a history file if it is the oldest timeline ever written to the repo)
    Check that copies of the manifests exist in backup.history

Questions/Concerns
- The command can be run locally or remotely, so from any pgbackrest host, right?
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
// archiveGetAsyncCallback(void *data, unsigned int clientIdx)
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
