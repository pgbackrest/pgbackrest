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
2. Verify WAL by reporting:
    1. Checksum mismatches
    2. Missing WAL
    3. Skipped duplicates
    4. Additional WAL skipped
    5. Size matches expected WAL size
3. Verify all manifests/copies are valid
    1. Files exist
    2. Checksums match
4. Verify backup files using manifest
    1. Files exist
    2. Checksums and size match
    3. Ignore files with references when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if request to verify a single incremental backup, then the manifest may have references to files in another (prior) backup which then must be checked.
5. Verify whether a backup is consistent, valid and can be played through PITR
    * Consistent = no gaps (missing or invalid) in the WAL from the backup start to backup end for a single backup (not backup set)
    * Valid = Consistent and all backed up PG files it references (including ones in a prior backup for DIFF and INCR backups) are verified as OK
    * PITR = Consistent, Valid and no gaps in the WAL from the start of the backup to the last WAL in the archive (timeline can be followed)
6. The command can be modified with the following options:
    * --set - only verify the specified backup and associated WAL (including PITR)
    * --no-pitr only verify the WAL required to make the backup consistent.
    * --fast - check only that the file is present and has the correct size.
7. Additional features (which may not be in the initially released version):
    1. WARN on extra files (e.g. junk files or files not in the manifest)
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
7. Neither links nor tables spaces will be verified


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

In order to verify that backup/archive.info and their copies are valid, each file will be loaded and checked as follows:

Results:
archivePtr - pointer to contents of archive info to keep in memory
backupPtr - pointer to contents of backup info to keep in memory
manifestPtr - pointer to contents of the manifest being processed to keep in memory
checksum - checksum of the file read
errorCode - 0 if successfully read, else error code

```
FOR each info file and its copy (archive.info, backup.info)
    IF archive.info exists and is readable
    THEN
        set result checksum to calculated checksum for file;
        set pointer to the file contents if requested (e.g. if the main was loaded successfully, then there is
            no need to keep the copy in memory);
    ELSE
        set error code (e.g. read error, checksum mismatch, etc)
    FI

    IF main loaded successfully
    THEN
        keep the file in memory
        load the copy but don't keep the copy contents in memory
        IF copy loaded successfully
        THEN
            IF main checksum != copy checksum
            THEN
                one of the files is corrupted so WARN but use main
            FI
        FI
    ELSE IF copy loaded successfully
        keep the file in memory
    FI
ROF        
```


## Requirement 2. Verify WAL

Verify WAL by reporting
1. Checksum mismatches
    - retrieved from the file name
2. Missing WAL
    - this will create a WAL ranges collected and will be analyzed in reconciliation phase (where backups are verified Consistent/PITRable)
3. Skipped duplicates
    - duplicates will be reported and skipped, creating a gap in the WAL ranges
4. Additional WAL skipped
    - specific to PG 9.2 where WAL FF never exists, so if it does it is reported and that file is skipped
5. Size matches expected WAL size
    - WAL size will be retrieved from the first WAL found in an archive Id directory (e.g. 9.6-1). **NOTE** all WAL will be expected to be of the same size - variable WAL sizes are not supported.

Considerations:
1. What to do with .partial or .backup files? Currently only WAL files with/without compression extension are verified.
2. Should we try to verify .history files? Currently we do not but we will need to read and follow them for final archive/backup reconciliation.
3. Option-archive-copy copies WAL to backup dir but since all that WAL will then be in the backup manifest, it will also be checked, although not in the same way, it still will result in a backup being invalid because Postgres will use the WAL from the backup dir first during a restore.
4. For our purposes the size should never change for a db-id. But, we'll need to put in code to check if the size changes and force them to do a stanza-upgrade in that case. (<- in archive push/get) So, for now you'll need to check the first WAL and use that size to verify the sizes of the rest. Later we'll pull size info from archive.info.


## Requirement 3. Verify all manifests/copies are valid

Minimum to check is:
1. Files exist
2. Checksums match

Considerations:
1. If a file exists and we deem it usable, then what if the database id, system-id or version is not in the history?
    - It was decided that if the database information does not exist in the history of the backup info file, then the backup will be considered invalid and the files will not be checked.
2. If a manifest is considered unusable, then should there be a backup result for it or do we just report an error in the log and indicate the backup is being skipped? Be consistent with WAL.
    - Every backup and every WAL on disk will have a result structure. If a manifest is unusable, the backup result status for that backup will be set to invalid
3. What if there are no backups in the backup.info file? Should we still assume the last one on disk is the "current"?
    - Consider it a temporary "current" until the manifest is determined to exist or not.
4. If the backup label is not thought to be the current and it is NOT in the backup.info file, then it is not restorable by the restore command (it relies on the backup:current section of backup.info to perform any restore), so shouldn't we error? And should we check it anyway or skip this backup?

```
IF manifest is readable
THEN
    IF history NOT found
    THEN
        ERROR
        status = manifest invalid
    ELSE
        IF manifest copy is readable
        THEN
            IF manifest checksum != manifest copy checksum
            THEN
                WARN
            FI
        FI
        move the manifest to result
    FI
ELSE
    IF manifest copy is readable
    THEN
        IF manifest is the current backup
        THEN
            skip checking backup
            status = backupMissingManifest
        FI
        ELSE

```            


## Requirement 4. Verify backup files using manifest

1. Files exist
2. Checksums and size match
3. Files with references should skip processing when every backup will be validated, otherwise, check each file referenced to another file in the backup set. For example, if request to verify a single incremental backup, then the manifest may have references to files in another (prior) backup which then must be checked. (NOTE: See Considerations below on a discussion of the race condition and possible fixes)

Considerations:
1. Only a valid manifest file will be used - if one is not found, the backup result status will indicate the backup is not valid and no files will be verified.
2. Backups are processed in alphabetical order so that backups will be processed after any backups they depend on.
3. There is a known race condition where the file results for a backup are still being processed when the dependent backup begins processing. Currently there is no mechanism to have the dependent backup wait for all the results of the prior backup, therefore there is a risk that one or more files in the dependent backup will skip processing because it is assumed to be verified OK if it is not yet in the InvalidFileList of the prior backup. One strategy was to mark the dependent backup invalid if the prior backup was marked invalid, however if the prior backup is invalid because of a backup file that, validly, no longer exists and is therefore not part of the dependent backup, then marking the dependent backup is flawed.
Another flaw here is that we think we can skip checking files in a prior backup but we won't know if a file that was referenced in a
prior backup was invalid or not until we have completed checking that backup. Since the next backup to process will only start when the current backup processing is near the end of its file list - i.e. when one process is freed up to allow for processing of the next backup to begin - then the solution is to employ a state flag to indicated if a backup is in progress or completed. If a referenced backup is still in progress, then it is accepted that the first process-max - 1 files could be reverified. So for each file in a backup:
    - if the prior backup is not in the result list, then that backup was never processed (likely due to the --set option) so verify the file
    - if the prior backup is in the result list and the verify-state of the backup is NOT 'completed' then verify the file
    - if the prior backup is in the result list and the verify-state of the backup is 'completed' then
        - if the file does not exist in the prior backup invalid file list, then the file is deemed valid and processing is skipped
        - if the file is in the invalid file list, then mark this backup invalid and add the file to this backup's invalid file list and skip processing this file
4. The verify-state of a backup is 'completed' when all files sent to be verified have been processed.

manifestFileIdx:
- Index of the file within the manifest file list that is being processed.

BackupList:
- List of backup labels in the repository. Although this list is available through the job processing, it is not persisted as each
- list item will be removed after it is processed.

BackupResult:
- label - backup label
- status - default to valid
- backupPrior - label of prior backup if any
- totalFileVerify - total number of files in the manifest for this backup that will be verified. For INCR or DIFF backups, not all of these files will be processed but all will attempt to be verified.
- pgId - db-id of the database that was backed up - used for building the archiveId for checking the WAL
- pgVersion - PG version of the database that was backed up - used for building the archiveId for checking the WAL
- pgSystemId - systemId of the database that was backed up **// CSHANG DO WE NEED THIS???**

BackupResultList:
    List of BackupResult for all backups processed. This list will persist after all jobs are completed.

```
FOR each backup label in BackupList
    read the manifest
    IF manifest is not usable
    THEN
        set backup result status = unusable manifest
    THEN
        initialize the backup result totalFileVerify = total files listed in manifest
        default backup result status = ok
        add backupResult to the BackupResultList
        initialize manifestFileIdx = 0

        FOR each target file referenced by manifestFileIdx
            get the ManifestFile data at manifestFileIdx
            fileName = NULL
            IF reference to a prior backup exists for the file
            THEN
                IF the prior backup label does not exist in BackupResultList
                THEN
                    fileName = prepend the prior backup label to the file name and add the compression extension if any
                FI
                ELSE
                    IF there were files were checked in the prior backup  (totalFileVerify != 0)
                    THEN
                        IF the file is in the prior backup invalid file list
                        THEN
                            add file to invalid file list for this backup result
                            set the backup result to invalid
                        ELSE
                            do nothing (skip checking the file - assume it is valid)
                        FI
                    ELSE
                        fileName = prepend the prior backup label to the file name and add the compression extension if any
                    FI
                FI
            ELSE
                fileName = prepend the label of the backup to the file name and add the compression extension if any
            FI

            IF fileName != NULL
            THEN
                send file for processing to verify existence, checksum and size
            FI

            increment manifestFileIdx
        ROF

    ELSE
        IF manifest not usable because the backup is in progress
        THEN
            log a warning that the backup is in progress
        ELSE
            log a warning that the backup may have expired
        FI

        skip the verification of the backup
    FI
ROF

FOR each file sent for processing
    IF file invalid
    THEN
        add file to the invalid file list for this backup
        log error
        backup status = not valid  // CSHANG May need to have several booleans instead of enum so statusIsValid, statusIsConsistent, statusIsPitrable
    FI

```

## Requirement 5. Verify whether a backup is consistent, valid and can be played through PITR

Check each backup, using the following definitions:

- Consistent - no gaps (missing or invalid) in the WAL from the backup start to backup end for a single backup (not backup set)
- Valid - backup is Consistent and all backed up PG files it references (including ones in a prior backup for DIFF and INCR backups) are verified as OK
- PITR - backup is Consistent and Valid and no gaps in the WAL from the start of the backup to the last WAL in the archive (timeline can be followed)

Considerations:
1. If no backups have PITR, especially the latest, then report an error
2. Need to determine gaps that are not legitimate.
    1. Gaps that are legitimate could be a result of archive expiration or a timeline switch.
    2. Gaps that are not legitimate are when WAL is expected to be there for a backup to be consistent and it is not there or there are invalid files listed for the backup range.
    3. Can we access, or have the user pass, retention settings to help determine if gaps are legitimate or not?
3. What if the backup is not in the backup.info file after we have completed verifying the backup files? The backup might be good, but it may not be restorable unless in the backup.info, right? Also, it may have just been expired out from under us - but in either case, should we indicate it is not in the backup.info and may not be restore-able?
4. We are skipping verifying what we believe to be the current backup (only a copy and was the newest backup on disk) so at the end, if it complete, we won't know and also if a dependent backup was invalid, we should probably still indicate the "current" backup is invalid even though we did not check it's files. BUT maybe not - since we didn't check the files, we're not really sure if the file that was a problem in the prior backup was referenced by the current backup - so should probably just report the backup as verification skipped....
5. It is possible to have a backup without all the WAL? Yes, if option-archive-check=false but if this is not on then all bets are off, but should we have special reporting for this case?
6. There can be valid gaps in the WAL so "missing" is only if it is expected to be there for a backup to be consistent. Log an error when the backup relies on WAL that is not valid. If invalidFileList not NULL (or maybe size > 0) then there is a problem in this range but that does not mean it affects a backup (but maybe PITR) so check but should do this AFTER using the archive timeline history file to confirm that indeed there are or are not actual gaps in the WAL when timeline switches occurred.
7. How to check WAL for PITR (e.g. after end of last backup - is that last backup or last completed backup?)? If doing async archive and process max = 8 then could be 8 missing. But we don't have access to process-max and we don't know if they had asynch archiving, so if we see gaps in the last 5 minutes of the WAL stream and the last backup stop WAL is there, then we'll just ignore that PITR has gaps?
8. History and WAL files:
    1. If WAL files and no history and visa versa then should be reporting as WARN
    2. If 00002.history make sure there is at least one timeline 2 range then WARN
    3. If there is a timeline 2 WAL range and no 2.history then WARN
    4. Find relationship between timeline 1 and 2 and if missing WARN
    5. If history file and no wal but it is before any timeline that exists in the ranges then OK
9. History files retrieval and reading:
    - Maybe use an expression to get all the history files along with the WAL files "(^[0-F]{16}$)|(^[0-F]{8}\.history$)" (OR maybe we get everything and then create lists: history, WALranges, junk)
    - Originally it was thought that we'd only need the latest history file since history is continually copied from each to the next. However, if we were on timeline 3 and then we did a restore and recovered only to timeline 3, then timeline 3, is what, invalid? So PITR then only valid on timeline 2?
    - NOTE!!! The timeline history file format is changed in version 9.3. The (validated) formats are:
    ```
    9.3 and later:
     timelineId	LSN	"reason"
     2	0/7000000	before 2000-01-01 00:00:00+00
    9.2 and earlier:
     timelineId	WAL_segment	"reason"
     1	000000010000000000000009	before 2000-01-01 01:00:00+01
     2	00000002000000000000000C	no recovery target specified
     ```
10. Timing. If originally had F1 D1 I1 dependency chain but when I go to check, and D1 is gone, then the I1 is no loner valid.
11. If offline backup, then both archive-start and archive-stop could be null so need to verify files but not going to do any wal comparison

### Pseudo-code

```

IF backup archive-stop == NULL (then start would also be NULL)
THEN
    skip checking the WAL  // CSHANG: should an INFO or WARN be logged?
ELSE
    FOR wal range sorted on stop-file ascending
        IF backup archive-stop <= walRange.stop
        THEN
            IF backup archive-start >= walRange.start
            THEN
                // Range is found
                IF walRange has an invalid file
                THEN
                    backup is not consistent
                    log error
                    continue to next backup
                ELSE
                    backup is consistent
                    IF backup-prior
                    THEN
                        find prior backup in backup result list
                        IF found and status = consistent, then


```

## Requirement 6. The command can be modified with the following options

These options will be available for the Verify command and are detailed in the following sections
    * --set - only verify the specified backup and associated WAL (including PITR)
    * --no-pitr only verify the WAL required to make the backup consistent.
    * --fast - check only that the file is present and has the correct size.


### -- set

### -- no-pitr

### -- fast
    A list of files and their sizes will be retrieved and files will only be checked for the size and existence.

Considerations:
1. WAL file checks will be limited. If the WAL is compressed, the listed size cannot be verified because there will be no attempt to open the file. The only existence check is if there are gaps: e.g. missing files or duplicates
2. Backup files require the manifest and will be checked for existence and size. If the backup is compressed, the file size must be checked against the repo-size entry.  If the backup is uncompressed, the repo-size entry will not exist and the file size must checked against the size entry in the manifest.
3. Only one process will be created for jobs so:
```
    unsigned int numProcesses = cfgOptionTest(cfgOptFast) ? 1 : cfgOptionUInt(cfgOptProcessMax);
    for (unsigned int processIdx = 1; processIdx <= numProcesses; processIdx++)
        protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 1, processIdx));
```

<!--
 ------------------------------------------------------------------------------------
TESTING
1) Local and remote tests
2) Compressed backups

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

1) What sections of the manifest can we really verify?
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

-->
<!--

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

-->
