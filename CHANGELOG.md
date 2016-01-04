# pgBackRest<br/>Change Log

## v0.90dev: UNDER DEVELOPMENT
__No Release Date Set__

* Fixed an issue where specifying `--no-archive-check` would throw a configuration error. _Reported by Jason O'Donnell_.

## v0.89: Timeout Bug Fix and Restore Read-Only Repositories
__Released December 24, 2015__

* Fixed an issue where longer-running backups/restores would timeout when remote and threaded. Keepalives are now used to make sure the remote for the main process does not timeout while the thread remotes do all the work. The error message for timeouts was also improved to make debugging easier.

* Allow restores to be performed on a read-only repository by using `--no-lock` and `--log-level-file=off`. The `--no-lock` option can only be used with restores.

* Minor styling changes, clarifications and rewording in the user guide.

* The dev branch has been renamed to master and for the time being the master branch has renamed to release, though it will probably be removed at some point -- thus ends the gitflow experiment for pgBackRest. It is recommended that any forks get re-forked and clones get re-cloned.

## v0.88: Documentation and Minor Bug Fixes
__Released November 22, 2015__

* Added documentation in the user guide for delta restores, expiration, dedicated backup hosts, starting and stopping pgBackRest, and replication.

* Fixed an issue where the `start`/`stop` commands required the `--config` option.

* Fixed an issue where log files were being overwritten instead of appended.

* Fixed an issue where `backup-user` was not optional.

* Symlinks are no longer created in backup directories in the repository. These symlinks could point virtually anywhere and potentially be dangerous. Symlinks are still recreated during a restore.

* Added better messaging for backup expiration. Full and differential backup expirations are logged on a single line along with a list of all dependent backups expired.

* Archive retention is automatically set to full backup retention if not explicitly configured.

## v0.87: Website and User Guide
__Released October 28, 2015__

* Added a new user guide that covers pgBackRest basics and some advanced topics including PITR. Much more to come, but it's a start.

* The website, markdown, and command-line help are now all generated from the same XML source.

* The `backup_label.old` and `recovery.done` files are now excluded from backups.

## v0.85: Start/Stop Commands and Minor Bug Fixes
__Released October 8, 2015__

* Added new feature to allow all pgBackRest operations to be stopped or started using the `stop` and `start` commands. This prevents any pgBackRest processes from running on a system where PostgreSQL is shutdown or the system needs to be quiesced for some reason.

* Removed dependency on `IO::String` module.

* Fixed an issue where an error could be returned after a backup or restore completely successfully.

* Fixed an issue where a resume would fail if temp files were left in the root backup directory when the backup failed. This scenario was likely if the backup process got terminated during the copy phase.

* Experimental support for PostgreSQL 9.5 beta1. This may break when the control version or WAL magic changes in future versions but will be updated in each pgBackRest release to keep pace. All regression tests pass except for `--target-resume` tests (this functionality has changed in 9.5) and there is no testing yet for `.partial` WAL segments.

## v0.82: Refactoring, Command-line Help, and Minor Bug Fixes
__Released September 14, 2015__

* Fixed an issue where resumed compressed backups were not preserving existing files.

* Fixed an issue where resume and incr/diff would not ensure that the prior backup had the same compression and hardlink settings.

* Fixed an issue where a cold backup using `--no-start-stop` could be started on a running PostgreSQL cluster without `--force` specified.

* Fixed an issue where a thread could be started even when none were requested.

* Fixed an issue where the pgBackRest version number was not being updated in `backup.info` and `archive.info` after an upgrade/downgrade.

* Fixed an issue where the `info` command was throwing an exception when the repository contained no stanzas. _Reported by Stephen Frost_.

* Fixed an issue where the PostgreSQL `pg_stop_backup()` NOTICEs were being output to stderr. _Reported by Stephen Frost_.

* Renamed `recovery-setting` option and section to `recovery-option` to be more consistent with pgBackRest naming conventions.

* Command-line help is now extracted from the same XML source that is used for the other documentation and includes much more detail.

* Code cleanup and refactoring to standardize on patterns that have evolved over time.

* Added dynamic module loading to speed up commands, especially asynchronous archiving.

* Expiration tests are now synthetic rather than based on actual backups. This will allow development of more advanced expiration features.

* Experimental support for PostgreSQL 9.5 alpha2. This may break when the control version or WAL magic changes in future versions but will be updated in each pgBackRest release to keep pace. All regression tests pass except for `--target-resume` tests (this functionality has changed in 9.5) and there is no testing yet for `.partial` WAL segments.

## v0.80: DBI Support, Stability, and Convenience Features
__Released August 9, 2015__

* Fixed an issue that caused the formatted timestamp for both the oldest and newest backups to be reported as the current time by the `info` command. Only `text` output was affected -- `json` output reported the correct epoch values. _Reported by Michael Renner_.

* Fixed protocol issue that was preventing ssh errors (especially on connection) from being logged.

* Now using Perl `DBI` and `DBD::Pg` for connections to PostgreSQL rather than `psql`. The `cmd-psql` and `cmd-psql-option` settings have been removed and replaced with `db-port` and `db-socket-path`. Follow the instructions in [Installation](USERGUIDE.md#installation) to install `DBD::Pg` on your operating system.

* Add [stop-auto](USERGUIDE.md#stop-auto-key) option to allow failed backups to automatically be stopped when a new backup starts.

* Add [db-timeout](USERGUIDE.md#db-timeout-key) option to limit the amount of time pgBackRest will wait for pg_start_backup() and pg_stop_backup() to return.

* Remove `pg_control` file at the beginning of the restore and copy it back at the very end. This prevents the possibility that a partial restore can be started by PostgreSQL.

* The repository is now created and updated with consistent directory and file modes. By default `umask` is set to `0000` but this can be disabled with the `neutral-umask` setting. _Reported by Cynthia Shang_

* Added checks to be sure the `db-path` setting is consistent with `db-port` by comparing the `data_directory` as reported by the cluster against the `db-path` setting and the version as reported by the cluster against the value read from `pg_control`. The `db-socket-path` setting is checked to be sure it is an absolute path.

* Experimental support for PostgreSQL 9.5 alpha1. This may break when the control version or WAL magic changes in future versions but will be updated in each pgBackRest release to keep pace. All regression tests pass except for `--target-resume` tests (this functionality has changed in 9.5) and there is no testing yet for `.partial` WAL segments.

* Major refactoring of the protocol layer to support future development.

* Added vagrant test configurations for Ubuntu 14.04 and CentOS 7.

* Split most of `README.md` out into `USERGUIDE.md` and `CHANGELOG.md` because it was becoming unwieldy. Changed most references to "database" in the user guide to "database cluster" for clarity.

## v0.78: Remove CPAN Dependencies, Stability Improvements
__Released July 13, 2015__

* Removed dependency on CPAN packages for multi-threaded operation. While it might not be a bad idea to update the `threads` and `Thread::Queue` packages, it is no longer necessary.

* Added vagrant test configurations for Ubuntu 12.04 and CentOS 6.

* Modified wait backoff to use a Fibonacci rather than geometric sequence. This will make wait time grow less aggressively while still giving reasonable values.

* More options for regression tests and improved code to run in a variety of environments.

## v0.77: CentOS/RHEL 6 Support and Protocol Improvements
__Released June 30, 2015__

* Removed `pg_backrest_remote` and added the functionality to `pg_backrest` as the `remote` command.

* Added file and directory syncs to the `File` object for additional safety during backup/restore and archiving. _Suggested by Andres Freund_.

* Support for Perl 5.10.1 and OpenSSH 5.3 which are default for CentOS/RHEL 6. _Reported by Eric Radman._

* Improved error message when backup is run without `archive_command` set and without `--no-archive-check` specified. _Reported by Eric Radman_.

* Moved version number out of the `VERSION` file to `Version.pm` to better support packaging. _Suggested by Michael Renner_.

* Replaced `IPC::System::Simple` and `Net::OpenSSH` with `IPC::Open3` to eliminate CPAN dependency for multiple operating systems.

## v0.75: New Repository Format, Info Command and Experimental 9.5 Support
__Released June 14, 2015__

* **IMPORTANT NOTE**: This flag day release breaks compatibility with older versions of pgBackRest. The manifest format, on-disk structure, and the binary names have all changed. You must create a new repository to hold backups for this version of pgBackRest and keep your older repository for a time in case you need to do a restore. The `pg_backrest.conf` file has not changed but you'll need to change any references to `pg_backrest.pl` in cron (or elsewhere) to `pg_backrest` (without the `.pl` extension).

* Add `info` command.

* More efficient file ordering for `backup`. Files are copied in descending size order so a single thread does not end up copying a large file at the end. This had already been implemented for `restore`.

* Logging now uses unbuffered output. This should make log files that are being written by multiple threads less chaotic. _Suggested by Michael Renner_.

* Experimental support for PostgreSQL 9.5. This may break when the control version or WAL magic changes but will be updated in each release.

## v0.70: Stability Improvements for Archiving, Improved Logging and Help
__Released June 1, 2015__

* Fixed an issue where `archive-copy` would fail on an incr/diff backup when `hardlink=n`. In this case the `pg_xlog` path does not already exist and must be created. _Reported by Michael Renner_

* Allow duplicate WAL segments to be archived when the checksum matches. This is necessary for some recovery scenarios.

* Allow comments/disabling in `pg_backrest.conf` using the `#` character. Only `#` characters in the forst character of the line are honored. _Suggested by Michael Renner_.

* Better logging before `pg_start_backup()` to make it clear when the backup is waiting on a checkpoint. _Suggested by Michael Renner_.

* Various command behavior, help and logging fixes. _Reported by Michael Renner_.

* Fixed an issue in async archiving where `archive-push` was not properly returning 0 when `archive-max-mb` was reached and moved the async check after transfer to avoid having to remove the stop file twice. Also added unit tests for this case and improved error messages to make it clearer to the user what went wrong. _Reported by Michael Renner_.

* Fixed a locking issue that could allow multiple operations of the same type against a single stanza. This appeared to be benign in terms of data integrity but caused spurious errors while archiving and could lead to errors in backup/restore. _Reported by Michael Renner_.

* Replaced `JSON` module with `JSON::PP` which ships with core Perl.

## v0.65: Improved Resume and Restore Logging, Compact Restores
__Released May 11, 2015__

* Better resume support. Resumed files are checked to be sure they have not been modified and the manifest is saved more often to preserve checksums as the backup progresses. More unit tests to verify each resume case.

* Resume is now optional. Use the `resume` setting or `--no-resume` from the command line to disable.

* More info messages during restore. Previously, most of the restore messages were debug level so not a lot was output in the log.

* Fixed an issue where an absolute path was not written into recovery.conf when the restore was run with a relative path.

* Added `tablespace` setting to allow tablespaces to be restored into the `pg_tblspc` path. This produces compact restores that are convenient for development, staging, etc. Currently these restores cannot be backed up as pgBackRest expects only links in the `pg_tblspc` path.

## v0.61: Bug Fix for Uncompressed Remote Destination
__Released April 21, 2015__

* Fixed a buffering error that could occur on large, highly-compressible files when copying to an uncompressed remote destination. The error was detected in the decompression code and resulted in a failed backup rather than corruption so it should not affect successful backups made with previous versions.

## v0.60: Better Version Support and WAL Improvements
__Released April 19, 2015__

* Pushing duplicate WAL now generates an error. This worked before only if checksums were disabled.

* Database System IDs are used to make sure that all WAL in an archive matches up. This should help prevent misconfigurations that send WAL from multiple clusters to the same archive.

* Regression tests working back to PostgreSQL 8.3.

* Improved threading model by starting threads early and terminating them late.

## v0.50: Restore and Much More
__Released March 25, 2015__

* Added restore functionality.

* All options can now be set on the command-line making `pg_backrest.conf` optional.

* De/compression is now performed without threads and checksum/size is calculated in stream. That means file checksums are no longer optional.

* Added option `--no-start-stop` to allow backups when Postgres is shut down. If `postmaster.pid` is present then `--force` is required to make the backup run (though if Postgres is running an inconsistent backup will likely be created). This option was added primarily for the purpose of unit testing, but there may be applications in the real world as well.

* Fixed broken checksums and now they work with normal and resumed backups. Finally realized that checksums and checksum deltas should be functionally separated and this simplified a number of things. Issue #28 has been created for checksum deltas.

* Fixed an issue where a backup could be resumed from an aborted backup that didn't have the same type and prior backup.

* Removed dependency on `Moose`. It wasn't being used extensively and makes for longer startup times.

* Checksum for `backup.manifest` to detect a corrupted/modified manifest.

* Link `latest` always points to the last backup. This has been added for convenience and to make restores simpler.

* More comprehensive unit tests in all areas.

## v0.30: Core Restructuring and Unit Tests
__Released October 5, 2014__

* Complete rewrite of `BackRest::File` module to use a custom protocol for remote operations and Perl native GZIP and SHA operations. Compression is performed in threads rather than forked processes.

* Fairly comprehensive unit tests for all the basic operations. More work to be done here for sure, but then there is always more work to be done on unit tests.

* Removed dependency on `Storable` and replaced with a custom ini file implementation.

* Added much needed documentation

* Numerous other changes that can only be identified with a diff.

## v0.19: Improved Error Reporting/Handling
__Released May 13, 2014__

* Working on improving error handling in the `File` object. This is not complete, but works well enough to find a few errors that have been causing us problems (notably, find is occasionally failing building the archive async manifest when system is under load).

* Found and squashed a nasty bug where `file_copy()` was defaulted to ignore errors. There was also an issue in `file_exists()` that was causing the test to fail when the file actually did exist. Together they could have resulted in a corrupt backup with no errors, though it is very unlikely.

## v0.18: Return Soft Error When Archive Missing
__Released April 13, 2014__

* The `archive-get` command returns a 1 when the archive file is missing to differentiate from hard errors (ssh connection failure, file copy error, etc.) This lets PostgreSQL know that that the archive stream has terminated normally. However, this does not take into account possible holes in the archive stream.

## v0.17: Warn When Archive Directories Cannot Be Deleted
__Released April 3, 2014__

* If an archive directory which should be empty could not be deleted backrest was throwing an error. There's a good fix for that coming, but for the time being it has been changed to a warning so processing can continue. This was impacting backups as sometimes the final archive file would not get pushed if the first archive file had been in a different directory (plus some bad luck).

## v0.16: RequestTTY=yes for SSH Sessions
__Released April 1, 2014__

* Added `RequestTTY=yes` to ssh sessions. Hoping this will prevent random lockups.

## v0.15: Added archive-get
__Released March 29, 2014__

* Added `archive-get` functionality to aid in restores.

* Added option to force a checkpoint when starting the backup, `start-fast=y`.

## v0.11: Minor Fixes
__Released March 26, 2014__

* Removed `master_stderr_discard` option on database SSH connections. There have been occasional lockups and they could be related to issues originally seen in the file code.

* Changed lock file conflicts on `backup` and `expire` commands to `ERROR`. They were set to `DEBUG` due to a copy-and-paste from the archive locks.

## v0.10: Backup and Archiving are Functional
__Released March 5, 2014__

* No restore functionality, but the backup directories are consistent PostgreSQL data directories. You'll need to either uncompress the files or turn off compression in the backup. Uncompressed backups on a ZFS (or similar) filesystem are a good option because backups can be restored locally via a snapshot to create logical backups or do spot data recovery.

* Archiving is single-threaded. This has not posed an issue on our multi-terabyte databases with heavy write volume. Recommend a large WAL volume or to use the async option with a large volume nearby.

* Backups are multi-threaded, but the `Net::OpenSSH` library does not appear to be 100% thread-safe so it will very occasionally lock up on a thread. There is an overall process timeout that resolves this issue by killing the process. Yes, very ugly.

* Checksums are lost on any resumed backup. Only the final backup will record checksum on multiple resumes. Checksums from previous backups are correctly recorded and a full backup will reset everything.

* The `backup.manifest` is being written as `Storable` because `Config::IniFile` does not seem to handle large files well. Would definitely like to save these as human-readable text.

* Absolutely no documentation (outside the code). Well, excepting these release notes.
