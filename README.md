# pg_backrest

Simple Postgres Backup and Restore

## recognition

Put something here, there are people to recognize!

## planned for next release

* Capture SDTERR in file functions - start with file_list_get() - IN PROGRESS.

## feature backlog

* Move backups to be removed to temp before deleting.

* Async archive-get.

* Database restore.

* --version param (with with version written into backup.manifest).

* Threading for archive-get and archive-put.

* Add configurable sleep to archiver process to reduce ssh connections.

* Fix bug where .backup files written into old directories can cause the archive process to error.

* Default restore.conf is written to each backup.

* Able to set timeout on ssh connection in config file.

* File->wait() function.  Waits for a file or directory to exist with configurable retry and timeout.

* Missing files during backup generate an ERROR in the log - the backup works but this message should probably be suppressed.

## required perl modules

* Net::OpenSSH
* Digest::SHA
* IO::Compress::Gzip
* IO::Uncompress::Gunzip
* JSON

* Moose (Not using many features here, just use standard Perl object syntax?)
* IPC::System::Simple (only used in DB object - should convert this to DBD::Pg)

## release notes

### v0.30: ???

* Complete rewrite of BackRest::File module to use a custom protocol for remote operations and Perl native GZIP and SHA operations.  Compression is performed in threads rather than forked processes.

* Fairly comprehensive unit tests for all the basic operations.  More work to be done here for sure, but then there is always more work to be done on unit tests.

* Removed dependency on Storable and replaced with a custom ini file implementation.

### v0.19: Improved error reporting/handling

* Working on improving error handling in the file object.  This is not complete, but works well enough to find a few errors that have been causing us problems (notably, find is occasionally failing building the archive async manifest when system is under load).

* Found and squashed a nasty bug where file_copy was defaulted to ignore errors.  There was also an issue in file_exists that was causing the test to fail when the file actually did exist.  Together they could have resulted in a corrupt backup with no errors, though it is very unlikely.

### v0.18: Return soft error from archive-get when file is missing

* The archive-get function returns a 1 when the archive file is missing to differentiate from hard errors (ssh connection failure, file copy error, etc.)  This lets Postgres know that that the archive stream has terminated normally.  However, this does not take into account possible holes in the archive stream.

### v0.17: Warn when archive directories cannot be deleted

* If an archive directory which should be empty could not be deleted backrest was throwing an error.  There's a good fix for that coming, but for the time being it has been changed to a warning so processing can continue.  This was impacting backups as sometimes the final archive file would not get pushed if the first archive file had been in a different directory (plus some bad luck).

### v0.16: RequestTTY=yes for SSH sessions

* Added RequestTTY=yes to ssh sesssions.  Hoping this will prevent random lockups.

### v0.15: Added archive-get

* Added archive-get functionality to aid in restores.

* Added option to force a checkpoint when starting the backup (start_fast=y).

### v0.11: Minor fixes

Tweaking a few settings after running backups for about a month.

* Removed master_stderr_discard option on database SSH connections.  There have been occasional lockups and they could be related issues originally seen in the file code.

* Changed lock file conflicts on backup and expire commands to ERROR.  They were set to DEBUG due to a copy-and-paste from the archive locks.

### v0.10: Backup and archiving are functional

This version has been put into production at Resonate, so it does work, but there are a number of major caveats.

* No restore functionality, but the backup directories are consistent Postgres data directories.  You'll need to either uncompress the files or turn off compression in the backup.  Uncompressed backups on a ZFS (or similar) filesystem are a good option because backups can be restored locally via a snapshot to create logical backups or do spot data recovery.

* Archiving is single-threaded.  This has not posed an issue on our multi-terabyte databases with heavy write volume.  Recommend a large WAL volume or to use the async option with a large volume nearby.

* Backups are multi-threaded, but the Net::OpenSSH library does not appear to be 100% threadsafe so it will very occasionally lock up on a thread.  There is an overall process timeout that resolves this issue by killing the process.  Yes, very ugly.

* Checksums are lost on any resumed backup. Only the final backup will record checksum on multiple resumes.  Checksums from previous backups are correctly recorded and a full backup will reset everything.

* The backup.manifest is being written as Storable because Config::IniFile does not seem to handle large files well.  Would definitely like to save these as human-readable text.

* Absolutely no documentation (outside the code).  Well, excepting these release notes.

* Lots of other little things and not so little things.  Much refactoring to follow.
