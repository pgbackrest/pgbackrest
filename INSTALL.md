# PgBackRest Installation

## sample ubuntu 12.04 install

1. Starting from a clean install, update the OS:

```
apt-get update
apt-get upgrade (reboot if required)
```

2. Install ssh, git and cpanminus

```
apt-get install ssh
apt-get install git
apt-get install cpanminus
```

3. Install Postgres (instructions from http://www.postgresql.org/download/linux/ubuntu/)

Create the file /etc/apt/sources.list.d/pgdg.list, and add a line for the repository:
```
deb http://apt.postgresql.org/pub/repos/apt/ precise-pgdg main
```
Then run the following:
```
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update

apt-get install postgresql-9.3
apt-get install postgresql-server-dev-9.3
```

4. Install required Perl modules:
```
cpanm JSON
cpanm Net::OpenSSH
cpanm DBI
cpanm DBD::Pg
cpanm IPC::System::Simple
cpanm Digest::SHA
cpanm IO::Compress::Gzip
cpanm IO::Uncompress::Gunzip
```
5. Install PgBackRest

Backrest can be installed by downloading the most recent release:

https://github.com/pgmasters/backrest/releases

6. To run unit tests:

* Create backrest_dev user
* Setup trusted ssh between test user account and backrest_dev
* Backrest user and test user must be in the same group

## configuration examples

PgBackRest takes some command-line parameters, but depends on a configuration file for most of the settings.  The default location for the configuration file is /etc/pg_backrest.conf.

#### confguring postgres for archiving with backrest

Modify the following settings in postgresql.conf:
```
wal_level = archive
archive_mode = on
archive_command = '/path/to/backrest/bin/pg_backrest.pl --stanza=db archive-push %p'
```

Replace the path with the actual location where PgBackRest was installed.  The stanza parameter should be changed to the actual stanza name you used for your database in pg_backrest.conf.

#### simple single host install

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

#### simple multiple host install

This configuration is appropriate for a small installation where backups are being made remotely.  Make sure that postgres@db-host has trusted ssh to backrest@backup-host and vice versa.  This configuration assumes that you have pg_backrest_remote.pl and pg_backrest.pl in the same path on both servers.

`/etc/pg_backrest.conf on the db host`:
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
`/etc/pg_backrest.conf on the backup host`:
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

## operations

PgBackRest is intended to be run from a scheduler like cron as there is no built-in scheduler.

### general options

These options are either global or used by all operations.

#### stanza

Defines the stanza for the operation.  A stanza is the configuration for a database that defines where it is located, how it will be backed up, archiving options, etc.  Most db servers will only have one Postgres cluster and therefore one stanza, whereas backup servers will have a stanza for every database that needs to be backed up.

Examples of how to configure a stanza can be found in the `configuration examples` section.

#### config

By default PgBackRest expects the its configuration file to be located at `/etc/pg_backrest.conf`.  Use this option to specify another location.

#### version

Returns the PgBackRest version.

#### help

Prints help with a summary of all options.

### backup

Perform a database backup.

#### options

##### type

The following backup types are supported:

- `full` - all database files will be copied and there will be no dependencies on previous backups.
- `incr` - incremental from the last successful backup.
- `diff` - like an incremental backup but always based on the last full backup.

##### no-start-stop

This option prevents PgBackRest from running `pg_start_backup()` and `pg_stop_backup()` on the database.  In order for this to work Postgres should be shut down and PgBackRest will generate an error if it is not.

The purpose of this option is to allow cold backups.  The `pg_xlog` directory is copied as-is and `backup::archive-required` is automatically set to `n` for the backup.

##### force

When used with  `--no-start-stop` a backup will be run even if PgBackRest thinks that Postgres is running.  **This option should be used with extreme care as it will likely result in a bad backup.**

There are some scenarios where a backup might still be desirable under these conditions.  For example, if a server crashes and database volume can only be mounted read-only, it would be a good idea to take a backup even if `postmaster.pid` is present.  In this case it would be better to revert to the prior backup and replay WAL, but possibly there is a very important transaction in a WAL log that did not get archived.

#### usage examples

```
/path/to/pg_backrest.pl --stanza=db --type=full backup
```
Run a `full` backup on the `db` stanza.  `--type` can also be set to `incr` or `diff` for incremental or differential backups.  However, if no `full` backup exists then a `full` backup will be forced even if `incr` or `diff` is requested.

### archive-push
```
/path/to/pg_backrest.pl --stanza=db archive-push %p
```
Accepts an archive file from Postgres and pushes it to the backup.  `%p` is how Postgres specifies the location of the file to be archived.  This command has no other purpose.

### archive-get

```
/path/to/pg_backrest.pl --stanza=db archive-get %f %p
```
Retrieves an archive log from the backup.  This is used in `restore.conf` to restore a backup to that last archive log, do PITR, or as an alternative to streaming for keep a replica up to date.  `%f` is how Postgres specifies the archive log it needs, and `%p` is the location where it should be copied.

### expire

PgBackRest does backup rotation, but it is not concerned with when the backups were created.  So if two full backups are configured in rentention, PgBackRest will keep two full backup no matter whether they occur 2 hours apart or two weeks apart.

```
/path/to/pg_backrest.pl --stanza=db expire
```
Expire (rotate) any backups that exceed the defined retention.  Expiration is run automatically after every successful backup, so there's no need to run this command on its own unless you have reduced rentention, usually to free up some space.

### restore

Restore a database from the PgBackRest repository.

* restore options

  * set

The backup set to be restored.  `latest` will restore the latest backup, otherwise provide the name of the backup to restore.  For example: `20150131-153358F` or `20150131-153358F_20150131-153401I`.

  * delta

By default the database base and tablespace directories are expected to be present but empty.  This option performs a delta restore using checksums if available and timestamp/size when checksums are not available.

  * force

By itself this option forces the database base and tablespace paths to be completely overwritten.  In combination with `--delta` a timestamp/size delta will be performed even if checksums are available.

* recovery options

  * type

The following recovery types are supported:

- `default` - recover to the end of the archive stream.
- `name` - recover the restore point specified in `--target`.
- `xid` - recover to the transaction id specified in `--target`.
- `time` - recover to the time specified in `--target`.
- `preserve` - preserve the existing `recovery.conf` file.
- `none` - no recovery past database becoming consistent.

Note that the `none` option may produce duplicate WAL if the database is started with archive logging enabled.  It is recommended that a new stanza be created for production databases restored in this way.

  * target

Defines the recovery target when `--type` is `name`, `xid`, or `time`.  For example, `--type=time` and `--target=2015-01-30 14:15:11 EST`.

  * target-exclusive

Defines whether recovery to the target whould be exclusive (the default is inclusive) and is only valid when `--type` is `time` or `xid`.  For example, using `--target-exclusive` would exclude the contents of transaction `1007` when `--type=xid` and `-target=1007`.  See `recovery_target_inclusive` option in Postgres docs for more information.

  * target-resume

Specifies whether recovery should resume when the recovery target is reached.  See `pause_at_recovery_target` in Postgres docs for more information.

  * target-timeline

Recovers along the specified timeline.  See `recovery_target_timeline` in Postgres docs for more information.

  * usage examples

```
/path/to/pg_backrest.pl --stanza=db --set=latest --type=name --target=release
```
Restores the latest database backup and then recovers to the `release` restore point.

**MORE TO BE ADDED HERE**

[reference this when writing about tablespace remapping]

http://www.databasesoup.com/2013/11/moving-tablespaces.html

## structure

PgBackRest stores files in a way that is easy for users to work with directly.  Each backup directory has one file and two subdirectories:

* `backup.manifest` file

Stores information about all the directories, links, and files in the backup.  The file is plaintext and should be very clear, but documentation of the format is planned in a future release.

* `base` directory

Contains the Postgres data directory as defined by the data_directory setting in `postgresql.conf`.

* `tablespace` directory

If tablespaces are present in the database, contains each tablespace in a separate subdirectory.  Tablespace names are used for the subdirectories unless --no-start-stop is specified in which case oids will be used instead.  The links in `base/pg_tblspc` are rewritten to the tablespace directory in either case.

## configuration options

Each section defines important aspects of the backup.  All configuration sections below should be prefixed with `global:` as demonstrated in the configuration samples.

#### command section

The command section defines external commands that are used by PgBackRest.

##### psql key

Defines the full path to psql.  psql is used to call pg_start_backup() and pg_stop_backup().
```
required: y
example: psql=/usr/bin/psql
```
##### remote key

Defines the file path to pg_backrest_remote.pl.

Required only if the path to pg_backrest_remote.pl is different on the local and remote systems.  If not defined, the remote path will be assumed to be the same as the local path.
```
required: n
example: remote=/home/postgres/backrest/bin/pg_backrest_remote.pl
```
#### command-option section

The command-option section allows abitrary options to be passed to any command in the command section.

##### psql key

Allows command line parameters to be passed to psql.
```
required: no
example: psql=--port=5433
```

#### log section

The log section defines logging-related settings.  The following log levels are supported:

- `off   `- No logging at all (not recommended)
- `error `- Log only errors
- `warn  `- Log warnings and errors
- `info  `- Log info, warnings, and errors
- `debug `- Log debug, info, warnings, and errors
- `trace `- Log trace (very verbose debugging), debug, info, warnings, and errors

##### level-file

Sets file log level.
```
default: info
example: level-file=warn
```
##### level-console

Sets console log level.
```
default: error
example: level-file=info
```
#### backup section

The backup section defines settings related to backup and archiving.

##### host

Sets the backup host.
```
required: n (but must be set if user is defined)
example: host=backup.mydomain.com
```
##### user

Sets user account on the backup host.
```
required: n (but must be set if host is defined)
example: user=backrest
```
##### path

Path where backups are stored on the local or remote host.
```
required: y
example: path=/backup/backrest
```
##### compress

Enable gzip compression.  Files stored in the backup are compatible with command-line gzip tools.
```
default: y
example: compress=n
```
##### checksum

Enable SHA-1 checksums.  Backup checksums are stored in backup.manifest while archive checksums are stored in the filename.
```
default: y
example: checksum=n
```
##### start-fast

Forces an immediate checkpoint (by passing true to the fast parameter of pg_start_backup()) so the backup begins immediately.
```
default: n
example: start-fast=y
```
##### hardlink

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each
backup is a full backup.  Be care though, because modifying files that are hard-linked can affect all the backups in the set.
```
default: y
example: hardlink=n
```
##### thread-max

Defines the number of threads to use for backup.  Each thread will perform compression and transfer to make the backup run faster, but don't set `thread-max` so high that it impacts database performance.
```
default: 1
example: thread-max=4
```
##### thread-timeout

Maximum amount of time (in seconds) that a backup thread should run.  This limits the amount of time that a thread might be stuck due to unforeseen issues during the backup.
```
default: <none>
example: thread-timeout=86400
```
##### archive-required

Are archive logs required to to complete the backup?  It's a good idea to leave this as the default unless you are using another
method for archiving.
```
default: y
example: archive-required=n
```
#### archive section

The archive section defines parameters when doing async archiving.  This means that the archive files will be stored locally, then a background process will pick them and move them to the backup.

##### path

Path where archive logs are stored before being asynchronously transferred to the backup.  Make sure this is not the same path as the backup is using if the backup is local.
```
required: y
example: path=/backup/archive
```
##### compress-async

When set then archive logs are not compressed immediately, but are instead compressed when copied to the backup host.  This means that more space will be used on local storage, but the initial archive process will complete more quickly allowing greater throughput from Postgres.
```
default: n
example: compress-async=y
```
##### archive-max-mb

Limits the amount of archive log that will be written locally.  After the limit is reached, the following will happen:

1. PgBackRest will notify Postgres that the archive was succesfully backed up, then DROP IT.
2. An error will be logged to the console and also to the Postgres log.
3. A stop file will be written in the lock directory and no more archive files will be backed up until it is removed.

If this occurs then the archive log stream will be interrupted and PITR will not be possible past that point.  A new backup will be required to regain full restore capability.

The purpose of this feature is to prevent the log volume from filling up at which point Postgres will stop all operation.  Better to lose the backup than have the database go down completely.

To start normal archiving again you'll need to remove the stop file which will be located at `${archive-path}/lock/${stanza}-archive.stop` where `${archive-path}` is the path set in the archive section, and ${stanza} is the backup stanza.
```
required: n
example: archive-max-mb=1024
```
#### retention section

The rentention section defines how long backups will be retained.  Expiration only occurs when the number of complete backups exceeds the allowed retention.  In other words, if full-retention is set to 2, then there must be 3 complete backups before the oldest will be expired.  Make sure you always have enough space for rentention + 1 backups.

##### full-retention

Number of full backups to keep.  When a full backup expires, all differential and incremental backups associated with the full backup will also expire.  When not defined then all full backups will be kept.
```
required: n
example: full-retention=2
```
##### differential-retention

Number of differential backups to keep.  When a differential backup expires, all incremental backups associated with the differential backup will also expire.  When not defined all differential backups will be kept.
```
required: n
example: differential-retention=3
```
##### archive-retention-type

Type of backup to use for archive retention (full or differential).  If set to full, then PgBackRest will keep archive logs for the number of full backups defined by `archive-retention`.  If set to differential, then PgBackRest will keep archive logs for the number of differential backups defined by `archive-retention`.

If not defined then archive logs will be kept indefinitely.  In general it is not useful to keep archive logs that are older than the oldest backup, but there may be reasons for doing so.
```
required: n
example: archive-retention-type=full
```
##### archive-retention

Number of backups worth of archive log to keep.  If not defined, then `full-retention` will be used when `archive-retention-type=full` and `differential-retention` will be used when `archive-retention-type=differential`.
```
required: n
example: archive-retention=2
```

### restore section

?????

### restore:option section

`Archive Recovery` and `Standby Server` `restore.conf` options can be specified here.  See http://www.postgresql.org/docs/X.X/static/recovery-config.html for details on `restore.conf` options (replace `X.X` with your database version).

Note: `restore_command` will automatically be generated unless overridden in this section.  Be careful about specifying your own `restore_command` as PgBackRest is designed to handle this for you.

'Target Recovery` options are specified on the command-line since they end to change from restore to restore (or not be needed at all in the case of a standby server).

Since PgBackRest does not start PostgreSQL after writing the recovery.conf file, it is always possible to edit/check the file before manually restarting.

### stanza sections

A stanza defines a backup for a specific database.  The stanza section must define the base database path and host/user if the database is remote.  Also, any global configuration sections can be overridden to define stanza-specific settings.

##### host

Sets the database host.
```
required: n (but must be set if user is defined)
example: host=db.mydomain.com
```
##### user

Sets user account on the db host.
```
required: n (but must be set if host is defined)
example: user=postgres
```
##### path

Path to the db data directory (data_directory setting in postgresql.conf).
```
required: y
example: path=/var/postgresql/data
```
