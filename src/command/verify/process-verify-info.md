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
6. Verify whether a backup is consistent, valid and can be played through PITR
    * Consistent = no gaps (missing or invalid) in the WAL from the backup start to backup end for a single backup (not backup set)
    * Valid = Consistent and all backed up PG files it references (including ones in a prior backup for DIFF and INCR backups) are verified as OK
    * PITR = Consistent, Valid and no gaps in the WAL from the start of the backup to the last WAL in the archive (timeline can be followed)
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
6. The archive-start and archive-stop for a single backup (not a set) will never cross a timeline


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

PER SLACK CONVERSATION 8/31/20
OK, so 3 steps: 1) if WAL is exists for the backup, say the DIFF, then it is consistent and MAY be PITR-able. 2) if a backed up PG file in the FULL backup is invalid, then as long as that file is NOT referenced in the DIFF backup, the DIFF is valid but if it does reference a file in the FULL and that PG file was bad, then the DIFF is completely invalid. 3) If WAL for the backup is consistent and all WAL after exists, is valid and can play through any timeline change then it is consistent, valid and PITR-able.

If we were missing some WAL in the backup (say the FULL) but all the PG files in the FULL backup exist and are valid - what does that make the backup?

david.steele  1:18 PM
It makes that backup invalid because of missing wal, but dependent backups may be ok.
*/ -->

<!--
/* CSHANG manifest questions:
1) What sections of the manifest can we really verify? I don't want to read the manifest again if I can help it so now that we have it,
what sections can we check?
    [backrest] - NO
    [backup] - maybe we should save all or a subset of this information in the result to check later? Definitely archive-start/stop but is anything else important?
                ==> No, start/stop all we need, at least for now
        backup-archive-start="000000010000000000000002"
        backup-archive-stop="000000010000000000000002"
        backup-label="20191112-173146F"
        backup-lsn-start="0/2000028"
        backup-lsn-stop="0/2000130"
        backup-timestamp-copy-start=1573579908
        backup-timestamp-start=1573579906
        backup-timestamp-stop=1573579919
        backup-type="full"
    [backup:option] - is there anything here we can/should check? If so, how?
                ==> No. Most of these are informational, and if something important like compress-type is wrong we’ll know it when we check the files.
        option-archive-check=true
        option-archive-copy=false
        option-backup-standby=false
        option-buffer-size=1048576
        option-checksum-page=true
        option-compress=true
        option-compress-level=6
        option-compress-level-network=3
        option-compress-type="gz"
        option-delta=false
        option-hardlink=false
        option-online=true
        option-process-max=1
    [backup:target] - NO (on the db so we can't check that)
    [db] - NO (on the db so we can't check that)
    [target:file] - definitely - this is the crux
        pg_data/PG_VERSION={"checksum":"ad552e6dc057d1d825bf49df79d6b98eba846ebe","master":true,"reference":"20200810-171426F","repo-size":23,"size":3,"timestamp":1597079647}
        pg_data/global/6100_vm={"checksum-page":true,"repo-size":20,"size":0,"timestamp":1574780487}
        pg_data/global/6114={"checksum":"348b9fc06b2db1b0e547bcf51ec7c7715143cd93","checksum-page":true,"repo-size":78,"size":8192,"timestamp":1574780487}
        pg_data/global/6115={"checksum":"0a1eda482229f7dfec6ec7083362ad2e80ce2262","checksum-page":true,"repo-size":76,"size":8192,"timestamp":1574780487}
        pg_data/global/pg_control={"checksum":"edb8d7e62358c8a0538d2a209da8ae76f289a7e0","master":true,"repo-size":213,"size":8192,"timestamp":1574780832}
        pg_data/global/pg_filenode.map={"checksum":"1b85310413a1541d7a326c2dbc3d0283b7da0f60","repo-size":136,"size":512,"timestamp":1574780487}
        pg_data/pg_logical/replorigin_checkpoint={"checksum":"347fc8f2df71bd4436e38bd1516ccd7ea0d46532","master":true,"repo-size":28,"size":8,"timestamp":1574780832}
        pg_data/pg_multixact/members/0000={"checksum":"0631457264ff7f8d5fb1edc2c0211992a67c73e6","repo-size":43,"size":8192,"timestamp":1574780487}
        pg_data/pg_multixact/offsets/0000={"checksum":"0631457264ff7f8d5fb1edc2c0211992a67c73e6","repo-size":43,"size":8192,"timestamp":1574780703}
        pg_data/pg_xact/0000={"checksum":"535bf8d445838537821cb3cb60c89e814a6287e6","repo-size":49,"size":8192,"timestamp":1574780824}
        pg_data/postgresql.auto.conf={"checksum":"c6b93d1c2880daea891c8879c86d167c6678facf","master":true,"repo-size":103,"size":88,"timestamp":1574780487}
        pg_data/tablespace_map={"checksum":"e71a909fe617319cff5c4463e0a65e056054d94b","master":true,"size":30,"timestamp":1574780855}
        pg_tblspc/16385/PG_10_201707211/16386/112={"checksum":"c0330e005f13857f54da25099ef919ca0c143cb6","checksum-page":true,"repo-size":74,"size":8192,"timestamp":1574780704}
    [target:file:default] - NO unless it is necessary for checking permissions?
                ==> No way to check anything in here — it would depend the system where the backup is restored.
    [target:link] - NO
    [target:link:default] - NO
    [target:path] - NO
    [target:path:default] - NO

2) If there are "reference" to prior backup, we should be able to skip checking them, no?
    ==> Yes, definitely skip them or we’ll just be doing the same work over and over.
-->
<!--
CSHANG pseudo code if job successful:

    If archive file
        Get the archiveId from the filePathName (stringList[1] - is there a better way?)
        Loop through verifyResultData.archiveIdResultList and find the archiveId
        If found, find the range in which this file falls into and add the file name and the reason to the invalidFileList

    If backup file
        Get the backup label from the filePathName (stringList[1] - is there a better way?)
        Loop through verifyResultData.backupList and find the backup label - NO we are removing the labels as we go along, so we will need to build the result
        If found, add the file name and the reason to the invalidFileList


Final stage, after all jobs are complete, is to reconcile the archive with the backup data which, it seems at this pioint is just determining if the backup is 1) consistent (no gaps) 2) can run through PITR (trickier - not sure what this would look like....)
Let's say we have archives such that walList Ranges are:
start 000000010000000000000001, stop 000000010000000000000005
start 000000020000000000000005, stop 000000020000000000000006
start 000000030000000000000007, stop 000000030000000000000007

After all jobs complete: If invalidFileList not NULL (or maybe size > 0) then there is a problem in this range
        PROBLEM: I am generating WAL ranges by timeline so in the above, because we are in a new timeline, it looks like a gap. So how would I determine that the following is OK? Would MUST use the archive timeline history file to confirm that indeed there are no actual gaps in the WAL

        full backup: 20200810-171426F
            wal start/stop: 000000010000000000000002 / 000000010000000000000002

        diff backup: 20200810-171426F_20200810-171442D
            wal start/stop: 000000010000000000000003 / 000000010000000000000003
            backup reference list: 20200810-171426F

        diff backup: 20200810-171426F_20200810-171445D
            wal start/stop: 000000010000000000000004 / 000000010000000000000004
            backup reference list: 20200810-171426F

        incr backup: 20200810-171426F_20200810-171459I
            wal start/stop: 000000020000000000000006 / 000000020000000000000006

/var/lib/pgbackrest/archive/demo/12-1/0000000100000000:
total 2280
-rw-r----- 1 postgres postgres 1994249 Aug 10 17:14 000000010000000000000001-da5d050e95663fe95f52dd5059db341b296ae1fa.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000002.00000028.backup
-rw-r----- 1 postgres postgres   73388 Aug 10 17:14 000000010000000000000002-498acf8c1dc48233f305bdd24cbb7bdc970d1268.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000003.00000028.backup
-rw-r----- 1 postgres postgres   73365 Aug 10 17:14 000000010000000000000003-b323c5739356590e18aa75c8079ec9ff06cb32b7.gz
-rw-r----- 1 postgres postgres     370 Aug 10 17:14 000000010000000000000004.00000028.backup
-rw-r----- 1 postgres postgres   73382 Aug 10 17:14 000000010000000000000004-0e54893dcff383538d3f6fd93f59b62e2bb42432.gz
-rw-r----- 1 postgres postgres  105843 Aug 10 17:14 000000010000000000000005-b1cc92d58afd5d6bf3a4a530f72bb9e3d3f2e8f6.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000200000000:
total 192
-rw-r----- 1 postgres postgres 115133 Aug 10 17:15 000000020000000000000005-74bd3036721ccfdfec648fe1b6fd2cc7b60fe310.gz
-rw-r----- 1 postgres postgres    370 Aug 10 17:15 000000020000000000000006.00000028.backup
-rw-r----- 1 postgres postgres  73368 Aug 10 17:15 000000020000000000000006-c6d2580ccd6fbd2ee83bb4bf5cb445f673eb17ff.gz

/var/lib/pgbackrest/archive/demo/12-1/0000000300000000:
total 76
-rw-r----- 1 postgres postgres 74873 Aug 10 17:15 000000030000000000000007-fb920b357b0bccc168b572196dccd42fcca05f53.gz


    // CSHANG No - maybe what we need to do is just store the full names in a list because we have to know which DB-ID the wal belongs to and tie that back to the backup data (from the manifest file) A: David says we shouldn't be tying back to backup.info, but rather the manifest - which is where the data in backup.info is coming from anyway
    // CSHANG and what about individual backup files, if any one of them is invalid (or any gaps in archive), that entire backup needs to be marked invalid, right? So maybe we need to be creating a list of invalid backups such that String *strLstAddIfMissing(StringList *this, const String *string); is called when we find a backup that is not good. And remove from the jobdata.backupList()?

To tie the backup back to the archive, we need to be able to search for the db-id - so maybe we need an strEndsWith?
-->

After all WAL have been checked, check the backups

Find the WAL range where walRange.stop >= backup stop.
IF walRange.start <= backup start
THEN
    Range is found
    IF walRange has an invalid file
    THEN
        backup is not consistent
