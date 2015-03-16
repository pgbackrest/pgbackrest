# PgBackRest - Simple Postgres Backup & Restore

PgBackRest aims to be a simple backup and restore system that can seamlessly scale up to the largest databases and workloads.

Primary PgBackRest features:

- Local or remote backup
- Multi-threaded backup/restore for performance
- Checksums
- Safe backups (checks that logs required for consistency are present before backup completes)
- Full, differential, and incremental backups
- Backup rotation (and minimum retention rules with optional separate retention for archive)
- In-stream compression/decompression
- Archiving and retrieval of logs for replicas/restores built in
- Async archiving for very busy systems (including space limits)
- Backup directories are consistent Postgres clusters (when hardlinks are on and compression is off)
- Tablespace support
- Restore delta option
- Restore using timestamp/size or checksum
- Restore remapping base/tablespaces

Instead of relying on traditional backup tools like tar and rsync, PgBackRest implements all backup features internally and features a custom protocol for communicating with remote systems.  Removing reliance on tar and rsync allows better solutions to database-specific backup issues.  The custom remote protocol limits the types of connections that are required to perform a backup which increases security.  Each thread requires only one SSH connection for remote backups.

## Install

!!! Perl-based blah, blah, blah

### Ubuntu 12.04

* Starting from a clean install, update the OS:
```
apt-get update
apt-get upgrade (reboot if required)
```
* Install ssh, git and cpanminus:
```
apt-get install ssh
apt-get install git
apt-get install cpanminus
```
* Install Postgres (instructions from http://www.postgresql.org/download/linux/ubuntu/)

Create the file /etc/apt/sources.list.d/pgdg.list, and add a line for the repository:
```
deb http://apt.postgresql.org/pub/repos/apt/ precise-pgdg main
```
* Then run the following:
```
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update

apt-get install postgresql-9.3
apt-get install postgresql-server-dev-9.3
```
* Install required Perl modules:
```
cpanm JSON
cpanm Net::OpenSSH
cpanm DBI
cpanm DBD::Pg
cpanm IPC::System::Simple
cpanm Digest::SHA
cpanm Compress::ZLib
```
* Install PgBackRest

Backrest can be installed by downloading the most recent release:

https://github.com/pgmasters/backrest/releases

* To run unit tests:
```
Create backrest_dev user
Setup trusted ssh between test user account and backrest_dev
Backrest user and test user must be in the same group
```

## Operations

PgBackRest is intended to be run from a scheduler like cron as there is no built-in scheduler.

### General Options

These options are either global or used by all operations.

#### `type` option

By default PgBackRest expects the its configuration file to be located at `/etc/pg_backrest.conf`.  Use this option to specify another location.
```
required: n
example: type=/path/to/backrest/pg_backrest.conf
```

#### `stanza` option

Defines the stanza for the operation.  A stanza is the configuration for a database that defines where it is located, how it will be backed up, archiving options, etc.  Most db servers will only have one Postgres cluster and therefore one stanza, whereas backup servers will have a stanza for every database that needs to be backed up.

Examples of how to configure a stanza can be found in the `configuration examples` section.
```
required: n
example: stanza=main
```

#### `help` option

Displays the PgBackRest help.
```
required: n
```

#### `version` option

Displays the PgBackRest version.
```
required: n
```

### Commands

Sample text for the commands, which I'm sure will be very, very interesting.

#### `backup` command

Perform a database backup.

##### `type` option

The following backup types are supported:

- `full` - all database files will be copied and there will be no dependencies on previous backups.
- `incr` - incremental from the last successful backup.
- `warn` - like an incremental backup but always based on the last full backup.

```
required: n
default: incr
example: type=full
```

##### `no-start-stop` option

This option prevents PgBackRest from running `pg_start_backup()` and `pg_stop_backup()` on the database.  In order for this to work Postgres should be shut down and PgBackRest will generate an error if it is not.

The purpose of this option is to allow cold backups.  The `pg_xlog` directory is copied as-is and `archive-required` is automatically disabled for the backup.
```
required: n
default: n
```

##### `force` option

When used with  `--no-start-stop` a backup will be run even if PgBackRest thinks that Postgres is running.  **This option should be used with extreme care as it will likely result in a bad backup.**

There are some scenarios where a backup might still be desirable under these conditions.  For example, if a server crashes and database volume can only be mounted read-only, it would be a good idea to take a backup even if `postmaster.pid` is present.  In this case it would be better to revert to the prior backup and replay WAL, but possibly there is a very important transaction in a WAL log that did not get archived.
```
required: n
default: n
```

##### Example: Full Backup

```
/path/to/pg_backrest.pl --stanza=db --type=full backup
```
Run a `full` backup on the `db` stanza.  `--type` can also be set to `incr` or `diff` for incremental or differential backups.  However, if no `full` backup exists then a `full` backup will be forced even if `incr` or `diff` is requested.

#### `archive-push` command

Archive a WAL segment to the repository.

##### Example

```
/path/to/pg_backrest.pl --stanza=db archive-push %p
```
Accepts a WAL segment from PostgreSQL and archives it in the repository.  `%p` is how PostgreSQL specifies the location of the WAL segment to be archived.

#### `archive-get` command

Get a WAL segment from the repository.

##### Example

```
/path/to/pg_backrest.pl --stanza=db archive-get %f %p
```
Retrieves a WAL segment from the repository.  This command is used in `restore.conf` to restore a backup, perform PITR, or as an alternative to streaming for keeping a replica up to date.  `%f` is how PostgreSQL specifies the WAL segment it needs, and `%p` is the location where it should be copied.

#### `expire` command

PgBackRest does backup rotation, but it is not concerned with when the backups were created.  So if two full backups are configured in rentention, PgBackRest will keep two full backup no matter whether they occur 2 hours apart or two weeks apart.

##### Example

```
/path/to/pg_backrest.pl --stanza=db expire
```
Expire (rotate) any backups that exceed the defined retention.  Expiration is run automatically after every successful backup, so there's no need to run this command on its own unless you have reduced rentention, usually to free up some space.

#### `restore` command

Perform a database restore.
PITR should start after the stop time in the .backup file.

[reference this when writing about tablespace remapping]

http://www.databasesoup.com/2013/11/moving-tablespaces.html

##### `set` option

The backup set to be restored.  `latest` will restore the latest backup, otherwise provide the name of the backup to restore.
```
required: n
default: default
example: set=20150131-153358F_20150131-153401I
```

##### `delta` option

By default the PostgreSQL data and tablespace directories are expected to be present but empty.  This option performs a delta restore using checksums.
```
required: n
default: n
```

##### `force` option

By itself this option forces the PostgreSQL data and tablespace paths to be completely overwritten.  In combination with `--delta` a timestamp/size delta will be performed instead of using checksums.
```
required: n
default: n
```

##### `type` option

The following recovery types are supported:

- `default` - recover to the end of the archive stream.
- `name` - recover the restore point specified in `--target`.
- `xid` - recover to the transaction id specified in `--target`.
- `time` - recover to the time specified in `--target`.
- `preserve` - preserve the existing `recovery.conf` file.
- `none` - no recovery past database becoming consistent

Note that the `none` option may produce duplicate archive log (WAL) if the database is started with archive logging enabled.  It is recommended that a new stanza be created for production databases restored in this way.
```
required: n
default: default
example: type=xid
```

##### `target` option

Defines the recovery target when `--type` is `name`, `xid`, or `time`.
```
required: y
example: target=--target=2015-01-30 14:15:11 EST
```

##### `target-exclusive` option

Defines whether recovery to the target whould be exclusive (the default is inclusive) and is only valid when `--type` is `time` or `xid`.  For example, using `--target-exclusive` would exclude the contents of transaction `1007` when `--type=xid` and `--target=1007`.  See `recovery_target_inclusive` option in the PostgreSQL docs for more information.
```
required: n
default: n
```

##### `target-resume` option

Specifies whether recovery should resume when the recovery target is reached.  See `pause_at_recovery_target` in the PostgreSQL docs for more information.
```
required: n
default: n
```

##### `target-timeline` option

Recovers along the specified timeline.  See `recovery_target_timeline` in the PostgreSQL docs for more information.
```
required: n
```

##### Example: Restore Latest


```
/path/to/pg_backrest.pl --stanza=db --set=latest --type=name --target=release
```
Restores the latest database backup and then recovers to the `release` restore point.

## Configuration

PgBackRest takes some command-line parameters, but depends on a configuration file for most of the settings. The default location for the configuration file is `/etc/pg_backrest.conf`.

### Examples

#### Confguring Postgres for Archiving

Modify the following settings in `postgresql.conf`:
```
wal_level = archive
archive_mode = on
archive_command = '/path/to/backrest/bin/pg_backrest.pl --stanza=db archive-push %p'
```
Replace the path with the actual location where PgBackRest was installed.  The stanza parameter should be changed to the actual stanza name for your database in `pg_backrest.conf`.


#### Simple Single Host Install

This configuration is appropriate for a small installation where backups are being made locally or to a remote file system that is mounted locally.

`/etc/pg_backrest.conf`:
```
[global:command]
psql=/usr/bin/psql

[global:backup]
path=/var/lib/postgresql/backup

[global:retention]
full-retention=2
differential-retention=2
archive-retention-type=diff
archive-retention=2

[db]
path=/var/lib/postgresql/9.3/main
```


#### Simple Multiple Host Install

This configuration is appropriate for a small installation where backups are being made remotely.  Make sure that postgres@db-host has trusted ssh to backrest@backup-host and vice versa.  This configuration assumes that you have pg_backrest_remote.pl and pg_backrest.pl in the same path on both servers.

`/etc/pg_backrest.conf` on the db host:
```
[global:command]
psql=/usr/bin/psql

[global:backup]
host=backup-host@mydomain.com
user=postgres
path=/var/lib/postgresql/backup

[db]
path=/var/lib/postgresql/9.3/main
```
`/etc/pg_backrest.conf` on the backup host:
```
[global:command]
psql=/usr/bin/psql

[global:backup]
path=/var/lib/postgresql/backup

[global:retention]
full-retention=2
archive-retention-type=full

[db]
host=db-host@mydomain.com
user=postgres
path=/var/lib/postgresql/9.3/main
```


### Options

#### `command` section

The `command` section defines the location of external commands that are used by PgBackRest.

##### `psql` key

Defines the full path to `psql`.  `psql` is used to call `pg_start_backup()` and `pg_stop_backup()`.

If addtional parameters need to be passed to `psql` (such as `--port` or `--cluster`) then add `%option%` to the command line and use `command-option::psql` to set options.
```
required: y
example: psql=/usr/bin/psql -X %option%
```

##### `remote` key

Defines the location of `pg_backrest_remote.pl`.

Required only if the path to `pg_backrest_remote.pl` is different on the local and remote systems.  If not defined, the remote path will be assumed to be the same as the local path.
```
required: n
default: same as local
example: remote=/usr/lib/backrest/bin/pg_backrest_remote.pl
```

#### `command-option` section

The `command-option` section allows abitrary options to be passed to any command in the `command` section.

##### `psql` key

Allows command line parameters to be passed to `psql`.
```
required: n
example: psql=--port=5433
```

#### `log` section

The `log` section defines logging-related settings.  The following log levels are supported:

- `off` - No logging at all (not recommended)
- `error` - Log only errors
- `warn` - Log warnings and errors
- `info` - Log info, warnings, and errors
- `debug` - Log debug, info, warnings, and errors
- `trace` - Log trace (very verbose debugging), debug, info, warnings, and errors


##### `level-file` key

Sets file log level.
```
required: n
default: info
example: level-file=debug
```

##### `level-console` key

Sets console log level.
```
required: n
default: warning
example: level-console=error
```

#### `general` section

The `general` section defines settings that are shared between multiple operations.

##### `buffer-size` key

Set the buffer size used for copy, compress, and uncompress functions.  A maximum of 3 buffers will be in use at a time per thread.  An additional maximum of 256K per thread may be used for zlib buffers.
```
required: n
default: 1048576
allow: 4096 - 8388608
example: buffer-size=16384
```

##### `compress` key

Enable gzip compression.  Backup files are compatible with command-line gzip tools.
```
required: n
default: y
example: compress=n
```

##### `compress-level` key

Sets the zlib level to be used for file compression when `compress=y`.
```
required: n
default: 6
allow: 0-9
override: backup, archive
example: compress-level=9
```

##### `compress-level-network` key

Sets the zlib level to be used for protocol compression when `compress=n` and the database is not on the same host as the backup.  Protocol compression is used to reduce network traffic but can be disabled by setting `compress-level-network=0`.  When `compress=y` the `compress-level-network` setting is ignored and `compress-level` is used instead so that the file is only compressed once.  SSH compression is always disabled.
```
required: n
default: 3
allow: 0-9
override: backup, archive, restore
example: compress-level-network=1
```

#### `backup` section

The `backup` section defines settings related to backup.

##### `host` key

Sets the backup host when backup up remotely via SSH.  Make sure that trusted SSH authentication is configured between the db host and the backup host.

When backing up to a locally mounted network filesystem this setting is not required.
```
required: n
example: host=backup.domain.com
```

##### `user` key

Sets user account on the backup host.
```
required: n
example: user=backrest
```

##### `path` key

Path where backups are stored on the local or remote host.
```
required: y
example: path=/var/lib/backrest
```

##### `start-fast` key

Forces a checkpoint (by passing `true` to the `fast` parameter of `pg_start_backup()`) so the backup begins immediately.
```
required: n
default: n
example: start-fast=y
```

##### `hardlink` key

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each backup is a full backup.  Be care though, because modifying files that are hard-linked can affect all the backups in the set.
```
required: n
default: n
example: hardlink=y
```

##### `thread-max` key

Defines the number of threads to use for backup.  Each thread will perform compression and transfer to make the backup run faster, but don't set `thread-max` so high that it impacts database performance.
```
required: n
default: 1
example: thread-max=4
```

##### `thread-timeout` key

Maximum amount of time (in seconds) that a backup thread should run.  This limits the amount of time that a thread might be stuck due to unforeseen issues during the backup.  Has no affect when `thread-max=1`.
```
required: n
example: thread-timeout=3600
```

##### `archive-required` key

Are archive logs required to to complete the backup?  It's a good idea to leave this as the default unless you are using another method for archiving.
```
required: n
default: y
example: archive-required=n
```

#### `archive` section

The `archive` section defines parameters when doing async archiving.  This means that the archive files will be stored locally, then a background process will pick them and move them to the backup.

##### `path` key

Path where archive logs are stored before being asynchronously transferred to the backup.  Make sure this is not the same path as the backup is using if the backup is local.
```
required: n
example: path=/var/lib/backrest
```

##### `compress-async` key

When set archive logs are not compressed immediately, but are instead compressed when copied to the backup host.  This means that more space will be used on local storage, but the initial archive process will complete more quickly allowing greater throughput from Postgres.
```
required: n
default: n
example: compress-async=y
```

##### `archive-max-mb` key

Limits the amount of archive log that will be written locally when `compress-async=y`.  After the limit is reached, the following will happen:

- PgBackRest will notify Postgres that the archive was succesfully backed up, then DROP IT.
- An error will be logged to the console and also to the Postgres log.
- A stop file will be written in the lock directory and no more archive files will be backed up until it is removed.

If this occurs then the archive log stream will be interrupted and PITR will not be possible past that point.  A new backup will be required to regain full restore capability.

The purpose of this feature is to prevent the log volume from filling up at which point Postgres will stop completely.  Better to lose the backup than have the database go down.

To start normal archiving again you'll need to remove the stop file which will be located at `${archive-path}/lock/${stanza}-archive.stop` where `${archive-path}` is the path set in the `archive` section, and `${stanza}` is the backup stanza.
```
required: n
example: archive-max-mb=1024
```

#### `retention` section

The `rentention` section defines how long backups will be retained.  Expiration only occurs when the number of complete backups exceeds the allowed retention.  In other words, if full-retention is set to 2, then there must be 3 complete backups before the oldest will be expired.  Make sure you always have enough space for rentention + 1 backups.

##### `full-retention` key

Number of full backups to keep.  When a full backup expires, all differential and incremental backups associated with the full backup will also expire.  When not defined then all full backups will be kept.
```
required: n
example: full-retention=2
```

##### `differential-retention` key

Number of differential backups to keep.  When a differential backup expires, all incremental backups associated with the differential backup will also expire.  When not defined all differential backups will be kept.
```
required: n
example: differential-retention=3
```

##### `archive-retention-type` key

Type of backup to use for archive retention (full or differential).  If set to full, then PgBackRest will keep archive logs for the number of full backups defined by `archive-retention`.  If set to differential, then PgBackRest will keep archive logs for the number of differential backups defined by `archive-retention`.

If not defined then archive logs will be kept indefinitely.  In general it is not useful to keep archive logs that are older than the oldest backup, but there may be reasons for doing so.
```
required: n
example: archive-retention-type=full
```

##### `archive-retention` key

Number of backups worth of archive log to keep.  If not defined, then `full-retention` will be used when `archive-retention-type=full` and `differential-retention` will be used when `archive-retention-type=differential`.
```
required: n
example: archive-retention=2
```

#### `stanza` section

A stanza defines a backup for a specific database.  The stanza section must define the base database path and host/user if the database is remote.  Also, any global configuration sections can be overridden to define stanza-specific settings.

##### `host` key

Define the database host.  Used for backups where the database host is different from the backup host.
```
required: n
example: host=db.domain.com
```

##### `user` key

Defines user account on the db host when `stanza::host` is defined.
```
required: n
example: user=postgres
```

##### `path` key

Path to the db data directory (data_directory setting in postgresql.conf).
```
required: y
example: path=/data/db
```

## Release Notes

### v0.50: [under development]

- Added restore functionality.

- De/compression is now performed without threads and checksum/size is calculated in stream.  That means file checksums are no longer optional.

- Added option `--no-start-stop` to allow backups when Postgres is shut down.  If `postmaster.pid` is present then `--force` is required to make the backup run (though if Postgres is running an inconsistent backup will likely be created).  This option was added primarily for the purpose of unit testing, but there may be applications in the real world as well.

- Fixed broken checksums and now they work with normal and resumed backups.  Finally realized that checksums and checksum deltas should be functionally separated and this simplied a number of things.  Issue #28 has been created for checksum deltas.

- Fixed an issue where a backup could be resumed from an aborted backup that didn't have the same type and prior backup.

- Removed dependency on Moose.  It wasn't being used extensively and makes for longer startup times.

- Checksum for backup.manifest to detect corrupted/modified manifest.

- Link `latest` always points to the last backup.  This has been added for convenience and to make restore simpler.

- More comprehensive backup unit tests.

### v0.30: Core Restructuring and Unit Tests

- Complete rewrite of BackRest::File module to use a custom protocol for remote operations and Perl native GZIP and SHA operations.  Compression is performed in threads rather than forked processes.

- Fairly comprehensive unit tests for all the basic operations.  More work to be done here for sure, but then there is always more work to be done on unit tests.

- Removed dependency on Storable and replaced with a custom ini file implementation.

- Added much needed documentation

- Numerous other changes that can only be identified with a diff.

### v0.19: Improved Error Reporting/Handling

- Working on improving error handling in the file object.  This is not complete, but works well enough to find a few errors that have been causing us problems (notably, find is occasionally failing building the archive async manifest when system is under load).

- Found and squashed a nasty bug where `file_copy()` was defaulted to ignore errors.  There was also an issue in file_exists that was causing the test to fail when the file actually did exist.  Together they could have resulted in a corrupt backup with no errors, though it is very unlikely.

### v0.18: Return Soft Error When Archive Missing

- The `archive-get` operation returns a 1 when the archive file is missing to differentiate from hard errors (ssh connection failure, file copy error, etc.)  This lets Postgres know that that the archive stream has terminated normally.  However, this does not take into account possible holes in the archive stream.

### v0.17: Warn When Archive Directories Cannot Be Deleted

- If an archive directory which should be empty could not be deleted backrest was throwing an error.  There's a good fix for that coming, but for the time being it has been changed to a warning so processing can continue.  This was impacting backups as sometimes the final archive file would not get pushed if the first archive file had been in a different directory (plus some bad luck).

### v0.16: RequestTTY=yes for SSH Sessions

- Added `RequestTTY=yes` to ssh sesssions.  Hoping this will prevent random lockups.

### v0.15: RequestTTY=yes for SSH Sessions

- Added archive-get functionality to aid in restores.

- Added option to force a checkpoint when starting the backup `start-fast=y`.

### v0.11: Minor Fixes

- Removed `master_stderr_discard` option on database SSH connections.  There have been occasional lockups and they could be related to issues originally seen in the file code.

- Changed lock file conflicts on backup and expire commands to ERROR.  They were set to DEBUG due to a copy-and-paste from the archive locks.

### v0.10: Backup and Archiving are Functional

- No restore functionality, but the backup directories are consistent Postgres data directories.  You'll need to either uncompress the files or turn off compression in the backup.  Uncompressed backups on a ZFS (or similar) filesystem are a good option because backups can be restored locally via a snapshot to create logical backups or do spot data recovery.

- Archiving is single-threaded.  This has not posed an issue on our multi-terabyte databases with heavy write volume.  Recommend a large WAL volume or to use the async option with a large volume nearby.

- Backups are multi-threaded, but the Net::OpenSSH library does not appear to be 100% threadsafe so it will very occasionally lock up on a thread.  There is an overall process timeout that resolves this issue by killing the process.  Yes, very ugly.

- Checksums are lost on any resumed backup. Only the final backup will record checksum on multiple resumes.  Checksums from previous backups are correctly recorded and a full backup will reset everything.

- The backup.manifest is being written as Storable because Config::IniFile does not seem to handle large files well.  Would definitely like to save these as human-readable text.

- Absolutely no documentation (outside the code).  Well, excepting these release notes.

## Recognition

Primary recognition goes to Stephen Frost for all his valuable advice and criticism during the development of PgBackRest.  It's a far better piece of software than it would have been without him.

Resonate (http://www.resonate.com/) also contributed to the development of PgBackRest and allowed me to install early (but well tested) versions as their primary Postgres backup solution.
