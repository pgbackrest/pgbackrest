# PgBackRest - Simple Postgres Backup & Restore

PgBackRest aims to be a simple backup and restore system that can seamlessly scale up to the largest databases and workloads.

Instead of relying on traditional backup tools like tar and rsync, PgBackRest implements all backup features internally and features a custom protocol for communicating with remote systems.  Removing reliance on tar and rsync allows better solutions to database-specific backup issues.  The custom remote protocol limits the types of connections that are required to perform a backup which increases security.  Each thread requires only one SSH connection for remote backups.

Primary PgBackRest features:

* Local or remote backup
* Multi-threaded backup/restore for performance
* Checksums
* Safe backups (checks that logs required for consistency are present before backup completes)
* Full, differential, and incremental backups
* Backup rotation (and minimum retention rules with optional separate retention for archive)
* In-stream compression/decompression
* Archiving and retrieval of logs for replicas/restores built in
* Async archiving for very busy systems (including space limits)
* Backup directories are consistent Postgres clusters (when hardlinks are on and compression is off)
* Tablespace support
* Restore delta option
* Restore using timestamp/size or checksum
* Restore remapping base/tablespaces

## release notes

### v0.50: [under development]

* Added restore functionality.

* Added option (--no-start-stop) to allow backups when Postgres is shut down.  If postmaster.pid is present then --force is required to make the backup run (though if Postgres is running an inconsistent backup will likely be created).  This option was added primarily for the purpose of unit testing, but there may be applications in the real world as well.

* Removed dependency on Moose.  It wasn't being used extensively and makes for longer startup times.

* Fixed broken checksums and now they work with normal and resumed backups.  Finally realized that checksums and checksum deltas should be functionally separated and this simplied a number of things.  Issue #28 has been created for checksum deltas.

* Fixed an issue where a backup could be resumed from an aborted backup that didn't have the same type and prior backup.

* More comprehensive backup unit tests.

* Link (called latest) always points to the last backup.  This has been added for convenience and to make restore simpler.

* Checksum for backup.manifest to detect corrupted/modified manifest.

### v0.30: core restructuring and unit tests

* Complete rewrite of BackRest::File module to use a custom protocol for remote operations and Perl native GZIP and SHA operations.  Compression is performed in threads rather than forked processes.

* Fairly comprehensive unit tests for all the basic operations.  More work to be done here for sure, but then there is always more work to be done on unit tests.

* Removed dependency on Storable and replaced with a custom ini file implementation.

* Added much needed documentation (see INSTALL.md).

* Numerous other changes that can only be identified with a diff.

### v0.19: improved error reporting/handling

* Working on improving error handling in the file object.  This is not complete, but works well enough to find a few errors that have been causing us problems (notably, find is occasionally failing building the archive async manifest when system is under load).

* Found and squashed a nasty bug where file_copy was defaulted to ignore errors.  There was also an issue in file_exists that was causing the test to fail when the file actually did exist.  Together they could have resulted in a corrupt backup with no errors, though it is very unlikely.

### v0.18: return soft error from archive-get when file is missing

* The archive-get function returns a 1 when the archive file is missing to differentiate from hard errors (ssh connection failure, file copy error, etc.)  This lets Postgres know that that the archive stream has terminated normally.  However, this does not take into account possible holes in the archive stream.

### v0.17: warn when archive directories cannot be deleted

* If an archive directory which should be empty could not be deleted backrest was throwing an error.  There's a good fix for that coming, but for the time being it has been changed to a warning so processing can continue.  This was impacting backups as sometimes the final archive file would not get pushed if the first archive file had been in a different directory (plus some bad luck).

### v0.16: RequestTTY=yes for SSH sessions

* Added RequestTTY=yes to ssh sesssions.  Hoping this will prevent random lockups.

### v0.15: added archive-get

* Added archive-get functionality to aid in restores.

* Added option to force a checkpoint when starting the backup (start_fast=y).

### v0.11: minor fixes

Tweaking a few settings after running backups for about a month.

* Removed master_stderr_discard option on database SSH connections.  There have been occasional lockups and they could be related issues originally seen in the file code.

* Changed lock file conflicts on backup and expire commands to ERROR.  They were set to DEBUG due to a copy-and-paste from the archive locks.

### v0.10: backup and archiving are functional

This version has been put into production at Resonate, so it does work, but there are a number of major caveats.

* No restore functionality, but the backup directories are consistent Postgres data directories.  You'll need to either uncompress the files or turn off compression in the backup.  Uncompressed backups on a ZFS (or similar) filesystem are a good option because backups can be restored locally via a snapshot to create logical backups or do spot data recovery.

* Archiving is single-threaded.  This has not posed an issue on our multi-terabyte databases with heavy write volume.  Recommend a large WAL volume or to use the async option with a large volume nearby.

* Backups are multi-threaded, but the Net::OpenSSH library does not appear to be 100% threadsafe so it will very occasionally lock up on a thread.  There is an overall process timeout that resolves this issue by killing the process.  Yes, very ugly.

* Checksums are lost on any resumed backup. Only the final backup will record checksum on multiple resumes.  Checksums from previous backups are correctly recorded and a full backup will reset everything.

* The backup.manifest is being written as Storable because Config::IniFile does not seem to handle large files well.  Would definitely like to save these as human-readable text.

* Absolutely no documentation (outside the code).  Well, excepting these release notes.

* Lots of other little things and not so little things.  Much refactoring to follow.

## recognition

Primary recognition goes to Stephen Frost for all his valuable advice and criticism during the development of PgBackRest.  It's a far better piece of software than it would have been without him.

Resonate (http://www.resonateinsights.com) also contributed to the development of PgBackRest and allowed me to install early (but well tested) versions as their primary Postgres backup solution.
