# pg_backrest installation

## configuration

BackRest takes some command-line parameters, but depends on a configuration file for most of the settings.  The default location for the configuration file is /etc/pg_backrest.conf.

### configuration examples

#### Simple single host install

This configuration is appropriate for a small installation where backups are being made locally or to a remote file system that is mounted locally.

`/etc/pg_backrest.conf`:
```
[global:command]
psql=/usr/bin/psql

[retention]
full-retention=2
archive-retention-type=full

[db]
path=/var/postgresql
```

### configuration sections

Each section defines important aspects of the backup.  All configuration sections below should be prefixed with "global:" as demonstrated in the configuration samples.

#### command section

The command section defines external commands that are used by BackRest.

##### psql key

Defines the full path to psql.  psql is used to call pg_start_backup() and pg_stop_backup().

_required_: y
_example_: psql=/usr/bin/psql

##### remote key

Defines the file path to pg_backrest_remote.pl.

Required only if the path to pg_backrest_remote.pl is different on the local and remote systems.  If not defined, the remote path will be assumed to be the same as the local path.

_required_: N
_example_: remote=/home/postgres/backrest/bin/pg_backrest_remote.pl

#### command-option section

The command-option section allows abitrary options to be passed to any command in the command section.

##### psql key

Allows command line parameters to be passed to psql.

_required_: no
_example_: psql=--port=5433

#### log section

The log section defines logging-related settings.  The following log levels are supported:

- `off   `- No logging at all (not recommended)
- `error `- Log only errors
- `warn  `- Log warnings and errors
- `info  `- Log info, warnings, and errors
- `debug `- Log debug, info, warnings, and errors
- `trace `- Log trace (very verbose debugging), debug, info, warnings, and errors

##### level-file

__default__: info

Sets file log level.

_Example_: level-file=warn

##### level-console

Sets console log level.

_default_: error
_example_: level-file=info

#### backup section

The backup section defines settings related to backup and archiving.

##### host

__Required__: N (but must be set if user is defined)

Sets the backup host.

_Example_: host=backup.mydomain.com

##### user

__Required__: N (but must be set if host is defined)

Sets user account on the backup host.

_Example_: user=backrest

##### path

__Required__: Y

Path where backups are stored on the local or remote host.

_Example_: path=/backup/backrest

##### compress

__Default__: Y

Enable gzip compression.  Files stored in the backup are compatible with command-line gzip tools.

_Example_: compress=n

##### checksum

__Default__: Y

Enable SHA-1 checksums.  Backup checksums are stored in backup.manifest while archive checksums are stored in the filename.

_Example_: checksum=n

##### start_fast

Forces an immediate checkpoint (by passing true to the fast parameter of pg_start_backup()) so the backup begins immediately.

_default_: n
_example_: hardlink=y

##### hardlink

__Default__: N

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each
backup is a full backup.  Be care though, because modifying files that are hard-linked can affect all the backups in the set.

_Example_: hardlink=y

##### thread-max

__default__: 1

Defines the number of threads to use for backup.  Each thread will perform compression and transfer to make the backup run faster, but don't set `thread-max` so high that it impacts database performance.

_Example_: thread-max=4

##### thread-timeout

Maximum amount of time that a backup thread should run.  This limits the amount of time that a thread might be stuck due to unforeseen issues during the backup.

_default_: <none>
_example_: thread-max=4

##### archive-required

Are archive logs required to to complete the backup?  It's a good idea to leave this as the default unless you are using another
method for archiving.

_default_: y
_example_: archive-required=n

#### archive section

The archive section defines parameters when doing async archiving.  This means that the archive files will be stored locally, then a background process will pick them and move them to the backup.

##### path

Path where archive logs are stored before being asynchronously transferred to the backup.  Make sure this is not the same path as the backup is using if the backup is local.

_required_: y
_example_: path=/backup/archive

##### compress-async

When set then archive logs are not compressed immediately, but are instead compressed when copied to the backup host.  This means that more space will be used on local storage, but the initial archive process will complete more quickly allowing greater throughput from Postgres.

_default_: n
_example_: compress-async=y

##### archive-max-mb

Limits the amount of archive log that will be written locally.  After the limit is reached, the following will happen:

1. BackRest will notify Postgres that the archive was succesfully backed up, then DROP IT.
2. An error will be logged to the console and also to the Postgres log.
3. A stop file will be written in the lock directory and no more archive files will be backed up until it is removed.

If this occurs then the archive log stream will be interrupted and PITR will not be possible past that point.  A new backup will be required to regain full restore capability.

The purpose of this feature is to prevent the log volume from filling up at which point Postgres will stop all operation.  Better to lose the backup than have the database go down completely.

To start normal archiving again you'll need to remove the stop file which will be located at `${archive-path}/lock/${stanza}-archive.stop` where `${archive-path}` is the path set in the archive section, and ${stanza} is the backup stanza.

_required_: n
_example_: archive-max-mb=1024

#### retention section

The rentention section defines how long backups will be retained.  Expiration only occurs when the number of complete backups exceeds the allowed retention.  In other words, if full-retention is set to 2, then there must be 3 complete backups before the oldest will be expired.  Make sure you always have enough space for rentention + 1 backups.

##### full-retention

Number of full backups to keep.  When a full backup expires, all differential and incremental backups associated with the full backup will also expire.  When not defined then all full backups will be kept.

_required_: n
_example_: full-retention=2

##### differential-retention

Number of differential backups to keep.  When a differential backup expires, all incremental backups associated with the differential backup will also expire.  When not defined all differential backups will be kept.

_required_: n
_example_: differential-retention=3

##### archive-retention-type

Type of backup to use for archive retention (full or differential).  If set to full, then BackRest will keep archive logs for the number of full backups defined by `archive-retention`.  If set to differential, then BackRest will keep archive logs for the number of differential backups defined by `archive-retention`.

If not defined then archive logs will be kept indefinitely.  In general it is not useful to keep archive logs that are older than the oldest backup, but there may be reasons for doing so.

_required_: n
_example_: archive-retention-type=full

##### archive-retention

Number of backups worth of archive log to keep.  If not defined, then `full-retention` will be used when `archive-retention-type=full` and `differential-retention` will be used when `archive-retention-type=differential`.

_required_: n
_example_: archive-retention=2

### stanza sections

A stanza defines a backup for a specific database.  The stanza section must define the base database path and host/user if the database is remote.  Also, any global configuration sections can be overridden to define stanza-specific settings.

##### host

Sets the database host.

* _required_: n (but must be set if user is defined)
* _example_: host=db.mydomain.com

##### user

Sets user account on the db host.

* _required_: n (but must be set if host is defined)
* _example_: user=postgres

##### path

Path to the db data directory (data_directory setting in postgresql.conf).

* _required_: y
* _example_: path=/var/postgresql/data

## sample ubuntu 12.04 install

apt-get update
apt-get upgrade (reboot if required)

apt-get install ssh
apt-get install cpanminus
apt-get install postgresql-9.3
apt-get install postgresql-server-dev-9.3

cpanm JSON
cpanm Moose
cpanm Net::OpenSSH
cpanm DBI
cpanm DBD::Pg
cpanm IPC::System::Simple

Create the file /etc/apt/sources.list.d/pgdg.list, and add a line for the repository
deb http://apt.postgresql.org/pub/repos/apt/ precise-pgdg main

wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update

For unit tests:

create backrest_dev user
setup trusted ssh between test user account and backrest_dev
backrest user and test user must be in the same group
