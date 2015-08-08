# pgBackRest - User Guide

## Installation

pgBackRest is written entirely in Perl.  Some additional modules will need to be installed depending on the OS.

### Ubuntu 12.04/14.04 Setup

* Install required Perl modules:
```
apt-get install libdbd-pg-perl
```

### CentOS 6 Setup

* Install Perl and required modules:
```
yum install perl perl-Time-HiRes perl-IO-String perl-parent perl-JSON perl-Digest-SHA perl-DBD-Pg
```

### CentOS 7 Setup

* Install Perl and required modules:
```
yum install perl perl-IO-String perl-Thread-Queue perl-JSON-PP perl-Digest-SHA perl-DBD-Pg
```

### Software Installation

pgBackRest can be installed by downloading the most recent release:

https://github.com/pgmasters/backrest/releases

pgBackRest can be installed anywhere but it's best (though not required) to install it in the same location on all systems.

### Regression Test Setup

* Create the backrest user

The backrest user must be created on the same system and in the same group as the user you will use for testing (which can be any user you prefer).  For example:
```
adduser -g <test-user-group> backrest
```
* Setup password-less SSH login between the test user and the backrest user

The test user should be able to `ssh backrest@127.0.0.1` and the backrest user should be able to `ssh <testuser>@127.0.0.1` without requiring any passwords.  This article (http://archive.oreilly.com/pub/h/66) has details on how to accomplish this.  Do the logons both ways at the command line before running regression tests.

* Give group read and execute permissions to `~/backrest/test`:

Usually this can be accomplished by running the following as the test user:
```
chmod 750 ~
```
* Running regression:

Running the full regression suite is generally not necessary.  Run the following first:
```
./test.pl --module=backup --test=full --db-version=all --thread-max=<# threads>
```
This will run full backup/restore regression with a variety of options on all installed versions of PostgreSQL.  If you are only interested in one version then modify the `db-version` setting to X.X (e.g. 9.4).  `--thread-max` can be omitted if you are running single-threaded.

If there are errors in this test then run full regression to help isolate problems:
```
./test.pl --db-version=all --thread-max=<# threads>
```
Report regression test failures at https://github.com/pgmasters/backrest/issues.

## Configuration

pgBackRest can be used entirely with command-line parameters but a configuration file is more practical for installations that are complex or set a lot of options. The default location for the configuration file is `/etc/pg_backrest.conf`.

### Examples

#### Confguring Postgres for Archiving

Modify the following settings in `postgresql.conf`:
```
wal_level = archive
archive_mode = on
archive_command = '/path/to/backrest/bin/pg_backrest --stanza=db archive-push %p'
```
Replace the path with the actual location where pgBackRest was installed.  The stanza parameter should be changed to the actual stanza name for your database.

#### Minimal Configuration

The absolute minimum required to run pgBackRest (if all defaults are accepted) is the database path.

`/etc/pg_backrest.conf`:
```
[main]
db-path=/data/db
```
The `db-path` option could also be provided on the command line, but it's best to use a configuration file as options tend to pile up quickly.

#### Simple Single Host Configuration

This configuration is appropriate for a small installation where backups are being made locally or to a remote file system that is mounted locally.  A number of additional options are set:

- `db-port` - Custom port for PostgreSQL.
- `compress` - Disable compression (handy if the file system is already compressed).
- `repo-path` - Path to the pgBackRest repository where backups and WAL archive are stored.
- `log-level-file` - Set the file log level to debug (Lots of extra info if something is not working as expected).
- `hardlink` - Create hardlinks between backups (but never between full backups).
- `thread-max` - Use 2 threads for backup/restore operations.

`/etc/pg_backrest.conf`:
```
[global:general]
compress=n
repo-path=/path/to/db/repo

[global:log]
log-level-file=debug

[global:backup]
hardlink=y
thread-max=2

[main]
db-path=/data/db
db-port=5555
```

#### Simple Multiple Host Configuration

This configuration is appropriate for a small installation where backups are being made remotely.  Make sure that postgres@db-host has trusted ssh to backrest@backup-host and vice versa.  This configuration assumes that you have pg_backrest in the same path on both servers.

`/etc/pg_backrest.conf` on the db host:
```
[global:general]
repo-path=/path/to/db/repo
repo-remote-path=/path/to/backup/repo

[global:backup]
backup-host=backup.mydomain.com
backup-user=backrest

[global:archive]
archive-async=y

[main]
db-path=/data/db
```
`/etc/pg_backrest.conf` on the backup host:
```
[global:general]
repo-path=/path/to/backup/repo

[main]
db-host=db.mydomain.com
db-path=/data/db
db-user=postgres
```

### Setttings

#### `command` section

The `command` section defines the location of external commands that are used by pgBackRest.

##### `cmd-remote` key

Defines the location of `pg_backrest_remote.pl`.

Required only if the path to `pg_backrest_remote.pl` is different on the local and remote systems.  If not defined, the remote path will be assumed to be the same as the local path.
```
required: n
default: same as local
example: cmd-remote=/usr/lib/backrest/bin/pg_backrest_remote.pl
```

#### `log` section

The `log` section defines logging-related settings.  The following log levels are supported:

- `off` - No logging at all (not recommended)
- `error` - Log only errors
- `warn` - Log warnings and errors
- `info` - Log info, warnings, and errors
- `debug` - Log debug, info, warnings, and errors
- `trace` - Log trace (very verbose debugging), debug, info, warnings, and errors


##### `log-level-file` key

Sets file log level.
```
required: n
default: info
example: log-level-file=debug
```

##### `log-level-console` key

Sets console log level.
```
required: n
default: warn
example: log-level-console=error
```

#### `general` section

The `general` section defines settings that are shared between multiple operations.

##### `buffer-size` key

Set the buffer size used for copy, compress, and uncompress functions.  A maximum of 3 buffers will be in use at a time per thread.  An additional maximum of 256K per thread may be used for zlib buffers.
```
required: n
default: 4194304
allow: 16384 - 8388608
example: buffer-size=32768
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
example: compress-level=9
```

##### `compress-level-network` key

Sets the zlib level to be used for protocol compression when `compress=n` and the database is not on the same host as the backup.  Protocol compression is used to reduce network traffic but can be disabled by setting `compress-level-network=0`.  When `compress=y` the `compress-level-network` setting is ignored and `compress-level` is used instead so that the file is only compressed once.  SSH compression is always disabled.
```
required: n
default: 3
allow: 0-9
example: compress-level-network=1
```

##### `neutral-umask` key

Sets the umask to 0000 so modes in the repository as created in a sensible way.  The default directory mode is 0750 and default file mode is 0640.  The lock and log directories set the directory and file mode to 0770 and 0660 respectively.

To use the executing user's umask instead specify `neutral-umask=n` in the config file or `--no-neutral-umask` on the command line.
```
required: n
default: y
example: neutral-umask=n
```

##### `repo-path` key

Path to the backrest repository where WAL segments, backups, logs, etc are stored.
```
required: n
default: /var/lib/backup
example: repo-path=/data/db/backrest
```

##### `repo-remote-path` key

Path to the remote backrest repository where WAL segments, backups, logs, etc are stored.
```
required: n
example: repo-remote-path=/backup/backrest
```

#### `backup` section

The `backup` section defines settings related to backup.

##### `backup-host` key

Sets the backup host when backup up remotely via SSH.  Make sure that trusted SSH authentication is configured between the db host and the backup host.

When backing up to a locally mounted network filesystem this setting is not required.
```
required: n
example: backup-host=backup.domain.com
```

##### `backup-user` key

Sets user account on the backup host.
```
required: n
example: backup-user=backrest
```

##### `db-timeout` key

Sets the timeout for operations against the database during the backup.  This includes the `pg_start_backup()` and `pg_stop_backup()` functions which can each take a substantial amount of time.  Because of this the timeout should be kept high unless you know that these functions will return quickly (i.e. if you have set `startfast=y` and you know that the database cluster will not generate many WAL segments during the backup).
```
required: n
default: 1800
example: db-timeout=600
```

##### `stop-auto` key

Automatically stops a prior failed backup when a new backup is run.  This will only be done if an exclusive advisory lock can be acquired to demonstrate that the prior failed backup process has really stopped.

This feature relies on pg_is_in_backup() so only works on PostgreSQL >= `9.3`.

The setting is disabled by default because it assumes that pgBackRest is the only process doing exclusive online backups.  It depends on an advisory lock that only pgBackRest sets so it may abort other processes that do exclusive online backups.  Note that `base_backup` and `pg_dump` are safe to use with this setting because they do not call `pg_start_backup()` so are not exclusive.

```
required: n
default: n
example: stop-auto=y
```

##### `start-fast` key

Forces a checkpoint (by passing `true` to the `fast` parameter of `pg_start_backup()`) so the backup begins immediately.  Otherwise the backup will start after the next regular checkpoint.

This feature only works in PostgreSQL <= `8.3`.
```
required: n
default: n
example: start-fast=y
```

##### `hardlink` key

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each backup is a full backup.  Be careful, though, because modifying files that are hard-linked can affect all the backups in the set.
```
required: n
default: n
example: hardlink=y
```

##### `manifest-save-threshold` key

Defines how often the manifest will be saved during a backup (in bytes).  Saving the manifest is important because it stores the checksums and allows the resume function to work efficiently.  The actual threshold used is 1% of the backup size or `manifest-save-threshold`, whichever is greater.
```
required: n
default: 1073741824
example: manifest-save-threshold=5368709120
```

##### `resume` key

Defines whether the resume feature is enabled.  Resume can greatly reduce the amount of time required to run a backup after a previous backup of the same type has failed.  It adds complexity, however, so it may be desirable to disable in environments that do not require the feature.
```
required: n
default: y
example: resume=false
```

##### `thread-max` key

Defines the number of threads to use for backup or restore.  Each thread will perform compression and transfer to make the backup run faster, but don't set `thread-max` so high that it impacts database performance during backup.
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

##### `archive-check` key

Checks that all WAL segments required to make the backup consistent are present in the WAL archive.  It's a good idea to leave this as the default unless you are using another method for archiving.
```
required: n
default: y
example: archive-check=n
```

##### `archive-copy` key

Store WAL segments required to make the backup consistent in the backup's pg_xlog path.  This slightly paranoid option protects against corruption or premature expiration in the WAL segment archive.  PITR won't be possible without the WAL segment archive and this option also consumes more space.

Even though WAL segments will be restored with the backup, PostgreSQL will ignore them if a `recovery.conf` file exists and instead use `archive_command` to fetch WAL segments.  Specifying `type=none` when restoring will not create `recovery.conf` and force PostgreSQL to use the WAL segments in pg_xlog.  This will get the database to a consistent state.
```
required: n
default: n
example: archive-copy=y
```

#### `archive` section

The `archive` section defines parameters when doing async archiving.  This means that the archive files will be stored locally, then a background process will pick them and move them to the backup.

##### `archive-async` key

Archive WAL segments asynchronously.  WAL segments will be copied to the local repo, then a process will be forked to compress the segment and transfer it to the remote repo if configured.  Control will be returned to PostgreSQL as soon as the WAL segment is copied locally.
```
required: n
default: n
example: archive-async=y
```

##### `archive-max-mb` key

Limits the amount of archive log that will be written locally when `archive-async=y`.  After the limit is reached, the following will happen:

- pgBackRest will notify Postgres that the archive was successfully backed up, then DROP IT.
- An error will be logged to the console and also to the Postgres log.
- A stop file will be written in the lock directory and no more archive files will be backed up until it is removed.

If this occurs then the archive log stream will be interrupted and PITR will not be possible past that point.  A new backup will be required to regain full restore capability.

The purpose of this feature is to prevent the log volume from filling up at which point Postgres will stop completely.  Better to lose the backup than have the database go down.

To start normal archiving again you'll need to remove the stop file which will be located at `${repo-path}/lock/${stanza}-archive.stop` where `${repo-path}` is the path set in the `general` section, and `${stanza}` is the backup stanza.
```
required: n
example: archive-max-mb=1024
```

#### `restore` section

The `restore` section defines settings used for restoring backups.

##### `tablespace` key

Defines whether tablespaces will be be restored into their original (or remapped) locations or stored directly under the `pg_tblspc` path.  Disabling this setting produces compact restores that are convenient for development, staging, etc.  Currently these restores cannot be backed up as pgBackRest expects only links in the `pg_tblspc` path. If no tablespaces are present this this setting has no effect.
```
required: n
default: y
example: tablespace=n
```

#### `expire` section

The `expire` section defines how long backups will be retained.  Expiration only occurs when the number of complete backups exceeds the allowed retention.  In other words, if full-retention is set to 2, then there must be 3 complete backups before the oldest will be expired.  Make sure you always have enough space for retention + 1 backups.

##### `retention-full` key

Number of full backups to keep.  When a full backup expires, all differential and incremental backups associated with the full backup will also expire.  When not defined then all full backups will be kept.
```
required: n
example: retention-full=2
```

##### `retention-diff` key

Number of differential backups to keep.  When a differential backup expires, all incremental backups associated with the differential backup will also expire.  When not defined all differential backups will be kept.
```
required: n
example: retention-diff=3
```

##### `retention-archive-type` key

Type of backup to use for archive retention (full or differential).  If set to full, then pgBackRest will keep archive logs for the number of full backups defined by `retention-archive`.  If set to differential, then pgBackRest will keep archive logs for the number of differential backups defined by `retention-archive`.

If not defined then archive logs will be kept indefinitely.  In general it is not useful to keep archive logs that are older than the oldest backup, but there may be reasons for doing so.
```
required: n
default: full
example: retention-archive-type=diff
```

##### `retention-archive` key

Number of backups worth of archive log to keep.  If this is set less than your backup retention then be sure you set `archive-copy=y` or you won't be able to restore some older backups.

For example, if `retention-archive=2` and `retention-full=4`, then any backups older than the most recent two full backups will not have WAL segments in the archive to make them consistent.  To solve this, set `archive-copy=y` and use `type=none` when restoring.  This issue will be addressed in a future release but for now be careful with this setting.
```
required: n
example: retention-archive=2
```

#### `stanza` section

A stanza defines a backup for a specific database.  The stanza section must define the base database path and host/user if the database is remote.  Also, any global configuration sections can be overridden to define stanza-specific settings.

##### `db-host` key

Define the database host.  Used for backups where the database host is different from the backup host.
```
required: n
example: db-host=db.domain.com
```

##### `db-user` key

Defines the logon user when `db-host` is defined.  This user will also own the remote pgBackRest process and will initiate connections to PostgreSQL.  For this to work correctly the user should be the PostgreSQL cluster owner which is generally `postgres`, the default.
```
required: n
default: postgres
example: db-user=test_user
```

##### `db-path` key

Path to the db data directory (data_directory setting in postgresql.conf).
```
required: y
example: db-path=/data/db
```

##### `db-port` key

Port that PostgreSQL is running on.  This usually does not need to be specified as most clusters run on the default port.
```
required: n
default: 5432
example: db-port=6543
```

##### `db-socket-path` key

The unix socket directory that was specified when PostgreSQL was started.  pgBackRest will automatically look in the standard location for your OS so there usually no need to specify this setting unless the socket directory was explicily modified with the `unix_socket_directory` setting in `postgressql.conf`.
```
required: n
example: db-socket-path=/var/run/postgresql
```

## Commands

### General Options

These options are either global or used by all commands.

#### `config` option

By default pgBackRest expects the its configuration file to be located at `/etc/pg_backrest.conf`.  Use this option to specify another location.
```
required: n
default: /etc/pg_backrest.conf
example: config=/var/lib/backrest/pg_backrest.conf
```

#### `stanza` option

Defines the stanza for the command.  A stanza is the configuration for a database that defines where it is located, how it will be backed up, archiving options, etc.  Most db servers will only have one Postgres cluster and therefore one stanza, whereas backup servers will have a stanza for every database that needs to be backed up.

Examples of how to configure a stanza can be found in the `configuration examples` section.
```
required: y
example: stanza=main
```

#### `help` option

Displays the pgBackRest help.
```
required: n
```

#### `version` option

Displays the pgBackRest version.
```
required: n
```

### Commands

#### `backup` command

Perform a database backup.  pgBackRest does not have a built-in scheduler so it's best to run it from cron or some other scheduling mechanism.

##### `type` option

The following backup types are supported:

- `full` - all database files will be copied and there will be no dependencies on previous backups.
- `incr` - incremental from the last successful backup.
- `diff` - like an incremental backup but always based on the last full backup.

```
required: n
default: incr
example: --type=full
```

##### `no-start-stop` option

This option prevents pgBackRest from running `pg_start_backup()` and `pg_stop_backup()` on the database.  In order for this to work PostgreSQL should be shut down and pgBackRest will generate an error if it is not.

The purpose of this option is to allow cold backups.  The `pg_xlog` directory is copied as-is and `archive-check` is automatically disabled for the backup.
```
required: n
default: n
```

##### `force` option

When used with  `--no-start-stop` a backup will be run even if pgBackRest thinks that PostgreSQL is running.  **This option should be used with extreme care as it will likely result in a bad backup.**

There are some scenarios where a backup might still be desirable under these conditions.  For example, if a server crashes and the database volume can only be mounted read-only, it would be a good idea to take a backup even if `postmaster.pid` is present.  In this case it would be better to revert to the prior backup and replay WAL, but possibly there is a very important transaction in a WAL segment that did not get archived.
```
required: n
default: n
```

##### Example: Full Backup

```
/path/to/pg_backrest --stanza=db --type=full backup
```
Run a `full` backup on the `db` stanza.  `--type` can also be set to `incr` or `diff` for incremental or differential backups.  However, if no `full` backup exists then a `full` backup will be forced even if `incr` or `diff` is requested.

#### `archive-push` command

Archive a WAL segment to the repository.

##### Example

```
/path/to/pg_backrest --stanza=db archive-push %p
```
Accepts a WAL segment from PostgreSQL and archives it in the repository defined by `repo-path`.  `%p` is how PostgreSQL specifies the location of the WAL segment to be archived.

#### `archive-get` command

Get a WAL segment from the repository.

##### Example

```
/path/to/pg_backrest --stanza=db archive-get %f %p
```
Retrieves a WAL segment from the repository.  This command is used in `recovery.conf` to restore a backup, perform PITR, or as an alternative to streaming for keeping a replica up to date.  `%f` is how PostgreSQL specifies the WAL segment it needs and `%p` is the location where it should be copied.

#### `expire` command

pgBackRest does backup rotation, but is not concerned with when the backups were created.  So if two full backups are configured for retention, pgBackRest will keep two full backups no matter whether they occur, two hours apart or two weeks apart.

##### Example

```
/path/to/pg_backrest --stanza=db expire
```
Expire (rotate) any backups that exceed the defined retention.  Expiration is run automatically after every successful backup, so there is no need to run this command separately unless you have reduced retention, usually to free up some space.

#### `restore` command

Perform a database restore.  This command is generally run manually, but there are instances where it might be automated.

##### `set` option

The backup set to be restored.  `latest` will restore the latest backup, otherwise provide the name of the backup to restore.
```
required: n
default: latest
example: --set=20150131-153358F_20150131-153401I
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
- `none` - no `recovery.conf` file is written so PostgreSQL will attempt to achieve consistency using WAL segments present in `pg_xlog`.  Provide the required WAL segments or use the `archive-copy` setting to include them with the backup.

```
required: n
default: default
example: --type=xid
```

##### `target` option

Defines the recovery target when `--type` is `name`, `xid`, or `time`.
```
required: y
example: "--target=2015-01-30 14:15:11 EST"
```

##### `target-exclusive` option

Defines whether recovery to the target would be exclusive (the default is inclusive) and is only valid when `--type` is `time` or `xid`.  For example, using `--target-exclusive` would exclude the contents of transaction `1007` when `--type=xid` and `--target=1007`.  See `recovery_target_inclusive` option in the PostgreSQL docs for more information.
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
example: --target-timeline=3
```

##### `recovery-setting` option

Recovery settings in recovery.conf options can be specified with this option.  See http://www.postgresql.org/docs/X.X/static/recovery-config.html for details on recovery.conf options (replace X.X with your database version).  This option can be used multiple times.

Note: `restore_command` will be automatically generated but can be overridden with this option.  Be careful about specifying your own `restore_command` as pgBackRest is designed to handle this for you.  Target Recovery options (recovery_target_name, recovery_target_time, etc.) are generated automatically by pgBackRest and should not be set with this option.

Recovery settings can also be set in the `restore:recovery-setting` section of pg_backrest.conf.  For example:
```
[restore:recovery-setting]
primary_conn_info=db.mydomain.com
standby_mode=on
```
Since pgBackRest does not start PostgreSQL after writing the `recovery.conf` file, it is always possible to edit/check `recovery.conf` before manually restarting.
```
required: n
example: --recovery-setting primary_conninfo=db.mydomain.com
```

##### `tablespace-map` option

Moves a tablespace to a new location during the restore.  This is useful when tablespace locations are not the same on a replica, or an upgraded system has different mount points.

Since PostgreSQL 9.2 tablespace locations are not stored in pg_tablespace so moving tablespaces can be done with impunity.  However, moving a tablespace to the `data_directory` is not recommended and may cause problems.  For more information on moving tablespaces http://www.databasesoup.com/2013/11/moving-tablespaces.html is a good resource.
```
required: n
example: --tablespace-map ts_01=/db/ts_01
```

##### Example: Restore Latest

```
/path/to/pg_backrest --stanza=db --type=name --target=release restore
```
Restores the latest database backup and then recovers to the `release` restore point.

#### `info` command

Retrieve information about backups for a single stanza or for all stanzas.  Text output is the default and gives a human-readable summary of backups for the stanza(s) requested.  This format is subject to change with any release.

For machine-readable output use `--output=json`.  The JSON output contains far more information than the text output, however **this feature is currently experimental so the format may change between versions**.

##### `output` option

The following output types are supported:

- `text` - Human-readable summary of backup information.
- `json` - Exhaustive machine-readable backup information in JSON format.

```
required: n
default: text
example: --output=json
```

##### Example: Information for a single stanza

```
/path/to/pg_backrest --stanza=db --output=json info
```

Get information about backups in the `db` stanza.

##### Example: Information for all stanzas

```
/path/to/pg_backrest --output=json info
```

Get information about backups for all stanzas in the repository.
