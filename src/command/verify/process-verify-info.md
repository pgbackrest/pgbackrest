# Overview
Add a command to verify the contents of the repository in order to alert users to missing WAL or inconsistent/non-PITR backups that would invalidate a restore.

# References
https://github.com/pgbackrest/pgbackrest/issues/1032

# Requirements
By the default the command will:

1. Verify that backup/archive.info and their copies are valid
    1. Files exist
    2. Checksums match
    3. PG histories match
2. Verify all manifests/copies are valid
3. Verify WAL by reporting:
    1. Checksum mismatches
    2. Missing WAL
    3. Skipped duplicates
    4. Additional WAL skipped
4. Verify all manifests/copies are valid
    1. Files exist
    2. Checksums match
5. Verify backup files using manifest
    1. Files exist
    2. Checksums and size match
    3. Ignore files with references when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if request to verify a single incremental backup, then the manifest may have references to files in another (prior) backup which then must be checked.
6. Verify whether a backup is consistent or can be played through PITR
    * Consistent = no gaps (missing or invalid) in the WAL from the backup start to backup end
    * PITR = no gaps in the WAL from the start of the backup to the last WAL in the archive (timeline can be followed)
7. The command can be modified with the following options:
    * --set - only verify the specified backup and associated WAL (including PITR)
    * --no-pitr only verify the WAL required to make the backup consistent.
    * --fast - check only that the file is present and has the correct size.
8. Additional features (which may not be in the initially released version):
    1. WARN on extra files
    2. Check that history files exist for timelines after the first (the first timeline will not have a history file if it is the oldest timeline ever written to the repo)
    3. Check that copies of the manifests exist in backup.history

## Assumptions

The following must be assumed to be true:
1. There is no access to the PostgreSQL cluster
2. Only the ability to read from the pgBackRest repository exists - no write capability
3. No lock will be taken therefore archiving, backups and expiration will continue and may cause archives or backups to be added or removed during the verification process
4. Archive and Backup info files are required to exist in order for archives and backups to be verified
5. A valid manifest file will be required to verify backup files

# Design
Referencing the [Requirements](#requirements) section, the following sections detail the design

## Requirement 1. Verify backup/archive info files

```mermaid
graph TD
subgraph Verify Info Files
errtotal[errorTotal = 0]-->loadarchiveinfo
loadarchiveinfo[Load Archive Info]-->archiveloaded{infoPtr != NULL?}
archiveloaded-->|no|archivelogerr[Log ERROR]
archivelogerr-->archivemissing[errorTotal++]
archivemissing-->loadbackupinfo
archiveloaded-->|yes|archiveinfoptr[archiveInfoPtr = infoPtr]
archiveinfoptr-->loadbackupinfo[Load Backup Info]
loadbackupinfo-->backuploaded{infoPtr != NULL?}
backuploaded-->|yes|backupinfoptr[backupInfoPtr = infoPtr]
backuploaded-->|no|backuplogerr[Log ERROR]
backuplogerr-->backupmissing[errorTotal++]
backupmissing-->ptrcheck
backupinfoptr-->ptrcheck{archiveInfoPtr AND backupInfoPtr set?}
ptrcheck-->|yes|dbhistorymatch{db AND history match?}
dbhistorymatch-->|no|dbhistorylogerror[Log ERROR]
dbhistorylogerror-->dbhistoryerr[errorTotal++]
dbhistoryerr-->endinfo(-END-)
dbhistorymatch-->|yes|noerror{errorTotal == 0?}
noerror-->|yes|verifyprocess(-Verify Process-)
end

loadarchiveinfo-->infoset

subgraph Load Info File
infoset[infoPtr = NULL]-->info
info[Load main info file]-->infoload{Load Success?}
infoload-->|no|infoerr[Log WARN]
infoload-->|yes|infoptr[infoPtr = main]
infoerr-->copy
infoptr-->copy[Load info.copy]
copy-->copyload{Load Success?}
copyload-->|no|copyerr[Log WARN]
copyerr-->done
copyload-->|yes|ptrnull{infoPtr == NULL?}
ptrnull-->|no|done
ptrnull-->|yes|infoptrcopy[infoPtr = copy]
infoptrcopy-->done(-END-)
end
```

In order to verify that backup/archive.info and their copies are valid, each file will be loaded and checked for the following:
<!--
- May have to expose infoBackupNewLoad so can load the backup.info and then the backup.info.copy and compare the 2 - need to know if one is corrupt and always failing over to the other.
        - want to WARN if only one exists
        - If backup.info/copy AND/OR archive.info/copy is missing then do not abort. ONLY error and abort if encrypted since we can't read the archive dir or backup dirs
        - checkStanzaInfoPg() makes sure the archive and backup info files (not copies just that one or the other) exist and are valid for the database version - BUT the db being available is not allowed because restore doesn't require it - so maybe just check archive and backup info DB sections match each other. BUT this throws an error if can't open one or the other - so would need to catch this and do LOG_ERROR() - maybe prefix with VERIFY-ERROR - or just take out the conditions and check for what we need OR maybe call checkStanzaInfo
        - We should probably check archive and backup history lists match (expire checks that the backup.info history contains at least the same history as archive.info (it can have more)) since we need to get backup db-id needs to be able to translate exactly to the archiveId (i.e. db-id=2, db-version=9.6 in backu.info must translate to archiveId 9.6-2.
        - If archive-copy set the WAL can be in the backup dirs so just because archive.info is missing, don't want to error

    * Reconstruct backup.info and WARN on mismatches - NO because this doesn't really help us since we do not want to take a lock which means things can be added/removed as we're verifying. So instead:
        - Get a list of backup on disk and a list of archive ids and pass that to job data
        - When checking backups, if the label is missing then skip (probably expired from underneath us) and then if it is there but only the manifest.copy exists, then skip (could be aborted or in progress so can't really check anything)
        - It is possible to have a backup without all the WAL if option-archive-check=true is not set but in this is not on then all bets are off

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

    * Verify all manifests/copies are valid
        - Both the manifest and manifest.copy exist, loadable and are identical. WARN if any condition is false (this should be in the jobCallback).
         If most recent has only copy, then move on since it could be the latest backup in progress. If missing both, then expired so skip. But if only copy and not the most recent then the backup still needs to be checked since restore will just try to read the manifest BUT it checks the manifest against the backup.info current section so if not in there (than what does restore do? WARN? ERROR?). If main is not there and copy is but it is not the latest then warn that main is missing
         BUT should we skip because backup reconstruct would remove it from current backup.info or should we use it to verify the backups? Meaning go with the copy if not the latest backup and warn? cmdRestore does not reconstruct the backup.info when it loads it.

    * Verify the checksum of all WAL/backup files
        - Pass the checksum to verifyFile - this needs to be stripped off from file in WAL but for backup it must be read from the manifest (which we need to read in the jobCallback
        - Skip checking size for WAL file but should check for size equality with the backup files - if one or the other doesn't match, then corrupt
        - in verifyFile() function be sure to add IoSync filter and then do IoReadDrain and then look at the results of the filters. See restore/file.c lines 85 though 87 (although need more filters e.g. decrypt, decompress, size, sha, sync).

            // Generate checksum for the file if size is not zero
            IoRead *read = NULL;

                read = storageReadIo(storageNewReadP(storageRepo(), filename));
                IoFilterGroup *filterGroup = ioReadFilterGroup(read);

                // Add decryption filter
                if (cipherPass != NULL)
                    ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, cipherTypeAes256Cbc, BUFSTR(cipherPass), NULL));


                // Add decompression filter
                if (repoFileCompressType != compressTypeNone)
                    ioFilterGroupAdd(filterGroup, decompressFilter(repoFileCompressType));

                // Add sha1 filter
                ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));

                // Add size filter
                ioFilterGroupAdd(filterGroup, ioSizeNew());

                // Add IoSink - this just throws away data so we don't want to move the data from the remote. The data is thrown away after the file is read, checksummed, etc
                ioFilterGroupAdd(filterGroup, ioSinkNew());

                ioReadDrain(read); // this will throw away if ioSink was not implemented

                // Then filtergroup get result to validate checksum
                if (!strEq(pgFileChecksum, varStr(ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR))))

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
// typedef enum // CSHANG Don't think I will need
// {
//     verifyWal,
//     verifyBackup,
// } VerifyState;
//
// typedef struct VerifyData
// {
//     bool fast;                                                      // Has the fast option been requested
//     VerifyState state; // CSHANG Don't think I need this as I will just be checking the Lists and removing(?) each item verified
//         // CSHANG list of WAL / WAL directories / archiveId to check
//         StringList *archiveIdList;
//         StringList *walPathList;
//         StringList *walFileList;
//
//         // CSHANG list of Backup directories/files to check
//         StringList *backupList;
//         StringList *backupFileList;
// // } VerifyData;

// static ProtocolParallelJob *
// verifyJobCallback(void *data, unsigned int clientIdx)
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
// if archiveIDlist ==Null
//     go get a list of archiveid.
//     done.
//
// if (walPathList size == 0)
//     go read the walpath
//     add all walPathlist
//     remove archiveid from list
//
// if walPathlist > 0
//     go read the walPathlist[i]
//     add all walFilelist
//     Here we check for gaps? Then when check for backup consistency, we warn if a specific backup can't do pitr or can do pitr
//     remove walPathlist[i] from list
//
// maybe have some info log (like "checking archinve id 9.4-1" and then detail level logging could in addition list the archive files being checked).
//
//     if (walFilelist)
//     {
//         process the 1 walFile from the list
//         remove that from the list
//
//         below are the guts from the PROTOCOL_COMMAND_ARCHIVE_GET_STR
//
//         const String *walSegment = strLstGet(jobData->walSegmentList, jobData->walSegmentIdx);
//         jobData->walSegmentIdx++;
//
//         ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_ARCHIVE_GET_STR); // CSHANG Replace with PROTOCOL_COMMAND_VERIFY_FILE_STR
//         protocolCommandParamAdd(command, VARSTR(walSegment));
//
//         FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
//     }
// else // OR maybe just have a verifyFile - may be that it just takes a file name and it doesn't know what type of file it is - like pass it the compress type, encryption, etc - whatever it needs to read the file. Make the verifyFile dumb. There is a funtion to get the compression type. BUT can't test compression on the files in backup because need to read the manifest to determine that.
//     if (backupList)
//         {
//             next backup thing to process
//
//             FUNCTION_TEST_RETURN(protocolParallelJobNew(VARSTR(walSegment), command));
//         }
//
//     FUNCTION_TEST_RETURN(NULL);
// }
//
// verifyJobResult(...)
// {
//     // Check that the job was successful
//     if (protocolParallelJobErrorCode(job) == 0)
//     {
//         MEM_CONTEXT_TEMP_BEGIN()
//         {
//             THISMAYBEASTRUCT result = varSOMETHING(protocolParallelJobResult(job));
// // CSHANG This is just an example - we need to do logging based on INFO for what we're checking, DETAIL for each files checked, then BACKUP success, WAL success would also be INFO, WARN logLevelWarn or ERROR logLevelError
//             LOG_PID(logLevelInfo, protocolParallelJobProcessId(job), 0, strZ(log));
//         }
//         MEM_CONTEXT_TEMP_END();
//
//         // Free the job
//         protocolParallelJobFree(job);
//     }
//     else
//         THROW_CODE(protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));
// }
//***********************************************************************************************************************************
/* CSHANG NOTES for PITR, For example:
postgres@pg-primary:~$ more /var/lib/pgbackrest/archive/demo/12-1/00000002.history
1	0/501C4D0	before 2020-08-10 17:14:51.275288+00

postgres@pg-primary:~$ more /var/lib/pgbackrest/archive/demo/12-1/00000003.history
1	0/501C4D0	before 2020-08-10 17:14:51.275288+00


2	0/7000000	before 2000-01-01 00:00:00+00

And archive has:
var/lib/pgbackrest/archive/demo/12-1/0000000100000000:
000000010000000000000001-da5d050e95663fe95f52dd5059db341b296ae1fa.gz
000000010000000000000002.00000028.backup
000000010000000000000002-498acf8c1dc48233f305bdd24cbb7bdc970d1268.gz
000000010000000000000003.00000028.backup
000000010000000000000003-b323c5739356590e18aa75c8079ec9ff06cb32b7.gz
000000010000000000000004.00000028.backup
000000010000000000000004-0e54893dcff383538d3f6fd93f59b62e2bb42432.gz
000000010000000000000005-b1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz  <-- timeline switch
000000010000000000000006-a1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000200000000:
000000020000000000000005-74bd3036721ccfdfec648fe1b6fd2cc7b60fe310.gz
000000020000000000000006.00000028.backup
000000020000000000000006-c6d2580ccd6fbd2ee83bb4bf5cb445f673eb17ff.gz <-- timeline switch at 2 0/7 but nothing after this so it won't
                                                            be found but that's OK -or- can't play on timeline 1 to the WAL switch?

/var/lib/pgbackrest/archive/demo/12-1/0000000300000000:
000000030000000000000007-fb920b357b0bccc168b572196dccd42fcca05f53.gz

So if we had a list of timelineWalSwitch:
000000010000000000000005
000000020000000000000007

And created another list of expectedTimelineWal (each switch file but timeline incremented)
000000020000000000000005
000000030000000000000007

So we can check each backup for consistency and when we say that we mean the backup and all of the prior backups it relies on. For
example, if full backup has
start 000000010000000000000001, stop 000000010000000000000004   FULL
start 000000020000000000000005, stop 000000020000000000000006   INCR depends on FULL
The if 000000010000000000000003 is missing/corrupt then FULL and then everything that depends on it must be marked invalid.

Once we know a backup is "consistent" then we need to check how far it can play forward. So, technically, the FULL could play through
the timeline 1 to 000000010000000000000006 (although it is not clear to me how this can be since PG would skip it because PG will be
following the progression through the history file) or through to the current 000000030000000000000007 is we follow the timeline
switches and there are no gaps.

    // Check if there was a timeline change
    if (strCmp(strSubN(range[x]->stop, 0, 8), strSubN(range[x+1], 0, 8)) != 0)
    {
        uint32_t stopTimeline = (uint32_t)strtol(strZ(strSubN(range[x]stop, 0, 8)), NULL, 16);
        uint32_t currentTimeline = (uint32_t)strtol(strZ(strSubN(range[x+1], 0, 8)), NULL, 16);

But the timeline+1 true? Could we skip to timeline 3 if there was a problem with timeline 2? But shouldn't that be in the history file?
        if (currentTimeline == stopTimeline + 1)
        {
*/ -->
