# pgBackRest - User Guide

## Table of Contents
1. [Installation](#installation)
  * [Ubuntu 12.04/14.04 Setup](#ubuntu-12-14-setup)
  * [CentOS 6 Setup](#centos-6-setup)
  * [CentOS 7 Setup](#centos-7-setup)
  * [Software Installation](#software-installation)
  * [Regression Test Setup](#regression-test-setup)
2. [Configuration](#configuration)
  * [Examples](#configuration-examples)
    * [Configuring Postgres for Archiving](#configuring-postgres-archiving)
    * [Minimal Configuration](#minimal-configuration)
    * [Simple Single Host Configuration](#simple-single-host-configuration)
    * [Single Multiple Host Configuration](#simple-multiple-host-configuration)
  * [Settings](#configuration-settings)
    * [`command` section](#command-section)
        * [`cmd-remote` key](#cmd-remote-key)
    * [`log` section](#log-section)
        * [`log-level-file` key](#log-level-file-key)
        * [`log-level-console` key](#log-level-console-key)
    * [`general` section](#configuration-general-section)
        * [`buffer-size` key](#buffer-size-key)
        * [`compress` key](#compress-key)
        * [`compress-level` key](#compress-level-key)
        * [`compress-level-network` key](#compress-level-network-key)
        * [`db-timeout` key](#db-timeout-key)
        * [`neutral-umask` key](#neutral-umask-key)
        * [`repo-path` key](#repo-path-key)
        * [`repo-remote-path` key](#repo-remote-path-key)
        * [`thread-max` key](#thread-max-key)
        * [`thread-timeout` key](#thread-timeout-key)
    * [`backup` section](#backup-section)
        * [`archive-check` key](#archive-check-key)
        * [`archive-copy` key](#archive-copy-key)
        * [`backup-host` key](#backup-host-key)
        * [`backup-user` key](#backup-user-key)
        * [`hardlink` key](#hardlink-key)
        * [`manifest-save-threshold` key](#manifest-save-threshold-key)
        * [`resume` key](#resume-key)
        * [`start-fast` key](#start-fast-key)
        * [`stop-auto` key](#stop-auto-key)
    * [`archive` section](#archive-section)
        * [`archive-async` key](#archive-async-key)
        * [`archive-max-mb` key](#archive-max-mb-key)
    * [`restore` section](#restore-section)
        * [`tablespace` key](#tablespace-key)
    * [`expire` section](#expire-section)
        * [`retention-full` key](#retention-full-key)
        * [`retention-diff` key](#retention-diff-key)
        * [`retention-archive-type` key](#retention-archive-type-key)
        * [`retention-archive` key](#retention-archive-key)
    * [`stanza` section](#stanza-section)
        * [`db-host` key](#db-host-key)
        * [`db-user` key](#db-user-key)
        * [`db-path` key](#db-path-key)
        * [`db-port` key](#db-port-key)
        * [`db-socket-path` key](#db-socket-path-key)
2. [Commands](#commands)
  * [General Options](#general-options)
    * [`config` option](#config-option)
    * [`stanza` option](#stanza-option)
  * [Commands](#commands-inner)
    * [`backup` command](#backup-command)
        * [`type` option](#type-option)
        * [`no-start-stop` option](#no-start-stop-option)
        * [`force` option](#force-option)
        * [Example: Full Backup](#example-full-backup)
    * [`archive-push` command](#archive-push-command)
        * [Example](#archive-push-command-example)
    * [`archive-get` command](#archive-get-command)
        * [Example](#archive-get-command-example)
    * [`expire` command](#expire-command)
        * [Example](#expire-command-example)
    * [`restore` command](#restore-command)
        * [`set` option](#set-option)
        * [`delta` option](#delta-option)
        * [`force` option](#force-option)
        * [`type` option](#type-option)
        * [`target` option](#target-option)
        * [`target-exclusive` option](#target-exclusive-option)
        * [`target-resume` option](#target-resume-option)
        * [`target-timeline` option](#target-timeline-option)
        * [`recovery-option` option](#recovery-option-option)
        * [`tablespace-map` option](#tablespace-map-option)
        * [Example: Restore Latest](#restore-latest-example)
    * [`info` command](#info-command)
        * [`output` option](#info-output-option)
        * [Example: Information for a single stanzas](#info-single-stanza-example)
        * [Example: Information for all stanzas](#info-all-stanza-example)
    * [`help` command](#help-command)
        * [Example: Help for the backup command](#help-backup-command-example)
        * [Example: Help for backup command, --force option](#help-backup-command-force-example)
    * [`version` command](#version-command)
        * [Example: Get version](#get-version-example)

## <a name="installation"></a>Installation

pgBackRest is written entirely in Perl.  Some additional modules will need to be installed depending on the OS.

### <a name="ubuntu-12-14-setup"></a>Ubuntu 12.04/14.04 Setup

* Install required Perl modules:
```
apt-get install libdbd-pg-perl
```

### <a name="centos-6-setup"></a>CentOS 6 Setup

* Install Perl and required modules:
```
yum install perl perl-Time-HiRes perl-IO-String perl-parent perl-JSON perl-Digest-SHA perl-DBD-Pg
```

### <a name="centos-7-setup"></a>CentOS 7 Setup

* Install Perl and required modules:
```
yum install perl perl-IO-String perl-Thread-Queue perl-JSON-PP perl-Digest-SHA perl-DBD-Pg
```

### <a name="software-installation"></a>Software Installation

pgBackRest can be installed by downloading the most recent release:

https://github.com/pgmasters/backrest/releases

pgBackRest can be installed anywhere but it's best (though not required) to install it in the same location on all systems.

### <a name="regression-test-setup"></a>Regression Test Setup

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

## <a name="configuration"></a>Configuration

pgBackRest can be used entirely with command-line parameters but a configuration file is more practical for installations that are complex or set a lot of options. The default location for the configuration file is `/etc/pg_backrest.conf`.

Each system where pgBackRest is installed should have a repository owned by the user that will be running pgBackRest on that system.  Normally this will be the `postgres` user on a database server and the `backrest` user on a backup server.  See [repo-path](USERGUIDE.md#repo-path-key) for more information on how repositories are used.

### <a name="configuration-examples"></a>Examples

#### <a name="configuring-postgres-archiving"></a>Confguring Postgres for Archiving

Modify the following settings in `postgresql.conf`:
```
wal_level = archive
archive_mode = on
archive_command = '/path/to/backrest/bin/pg_backrest --stanza=db archive-push %p'
```
Replace the path with the actual location where pgBackRest was installed.  The stanza parameter should be changed to the actual stanza name for your database cluster.

#### <a name="minimal-configuration"></a>Minimal Configuration

The absolute minimum required to run pgBackRest (if all defaults are accepted) is the database cluster path.

`/etc/pg_backrest.conf`:
```
[main]
db-path=/data/db
```
The `db-path` option could also be provided on the command line, but it's best to use a configuration file as options tend to pile up quickly.

#### <a name="simple-single-host-configuration"></a>Simple Single Host Configuration

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

#### <a name="simple-multiple-host-configuration"></a>Simple Multiple Host Configuration

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

### <a name="configuration-settings"></a>Setttings

#### <a name="command-section"></a>`command` section

The `command` section defines the location of external commands that are used by pgBackRest.

##### <a name="cmd-remote-key"></a>`cmd-remote` key

pgBackRest exe path on the remote host.

Required only if the path to pg_backrest is different on the local and remote systems.  If not defined, the remote exe path will be set the same as the local exe path.
```
default: same as local
example: cmd-remote=/usr/lib/backrest/bin/pg_backrest_remote.pl
```

#### <a name="log-section"></a>`log` section

The `log` section defines logging-related settings.

##### <a name="log-level-file-key"></a>`log-level-file` key

Level for file logging.

The following log levels are supported:

- `off` - No logging at all (not recommended)
- `error` - Log only errors
- `warn` - Log warnings and errors
- `info` - Log info, warnings, and errors
- `debug` - Log debug, info, warnings, and errors
- `trace` - Log trace (very verbose debugging), debug, info, warnings, and errors

```
default: info
example: log-level-file=debug
```

##### <a name="log-level-console-key"></a>`log-level-console` key

Level for console logging.

The following log levels are supported:

- `off` - No logging at all (not recommended)
- `error` - Log only errors
- `warn` - Log warnings and errors
- `info` - Log info, warnings, and errors
- `debug` - Log debug, info, warnings, and errors
- `trace` - Log trace (very verbose debugging), debug, info, warnings, and errors

```
default: warn
example: log-level-console=error
```

#### <a name="configuration-general-section"></a>`general` section

The `general` section defines settings that are shared between multiple operations.

##### <a name="buffer-size-key"></a>`buffer-size` key

Buffer size for file operations.

Set the buffer size used for copy, compress, and uncompress functions.  A maximum of 3 buffers will be in use at a time per thread.  An additional maximum of 256K per thread may be used for zlib buffers.
```
default: 4194304
allow: 16384 - 8388608
example: buffer-size=32768
```

##### <a name="compress-key"></a>`compress` key

Use file compression.

Enable gzip compression.  Backup files are compatible with command-line gzip tools.
```
default: y
example: compress=n
```

##### <a name="compress-level-key"></a>`compress-level` key

Compression level for stored files.

Sets the zlib level to be used for file compression when `compress=y`.
```
default: 6
allow: 0-9
example: compress-level=9
```

##### <a name="compress-level-network-key"></a>`compress-level-network` key

Compression level for network transfer when `compress=n`.

Sets the zlib level to be used for protocol compression when `compress=n` and the database cluster is not on the same host as the backup.  Protocol compression is used to reduce network traffic but can be disabled by setting `compress-level-network=0`.  When `compress=y` the `compress-level-network` setting is ignored and `compress-level` is used instead so that the file is only compressed once.  SSH compression is always disabled.
```
default: 3
allow: 0-9
example: compress-level-network=1
```

##### <a name="db-timeout-key"></a>`db-timeout` key

Database query timeout.

Sets the timeout for queries against the database.  This includes the `pg_start_backup()` and `pg_stop_backup()` functions which can each take a substantial amount of time.  Because of this the timeout should be kept high unless you know that these functions will rme=""></a>eturn quickly (i.e. if you have set `startfast=y` and you know that the database cluster will not generate many WAL segments during the backup).
```
default: 1800
example: db-timeout=600
```

##### <a name="neutral-umask-key"></a>`neutral-umask` key

Use a neutral umask.

Sets the umask to 0000 so modes in the repository as created in a sensible way.  The default directory mode is 0750 and default file mode is 0640.  The lock and log directories set the directory and file mode to 0770 and 0660 respectively.

To use the executing user's umask instead specify `neutral-umask=n` in the config file or `--no-neutral-umask` on the command line.
```
default: y
example: neutral-umask=n
```

##### <a name="repo-path-key"></a>`repo-path` key

Repository path where WAL segments, backups, logs, etc are stored.

The repository serves as both storage and working area for pgBackRest.  In a simple installation where the backups are stored locally to the database server there will be only one repository which will contain everything: backups, archives, logs, locks, etc.

If the backups are being done remotely then the backup server's repository will contain backups, archives, locks and logs while the database server's repository will contain only locks and logs.  However, if asynchronous archiving is enabled then the database server's repository will also contain a spool directory for archive logs that have not yet been pushed to the remote repository.

Each system where pgBackRest is installed should have a repository directory configured.  Storage requirements vary based on usage.  The main backup repository will need the most space as it contains both backups and WAL segments for whatever retention you have specified.  The database repository only needs significant space if asynchronous archiving is enabled and then it will act as an overflow for WAL segments and might need to be large depending on your database activity.

If you are new to backup then it will be difficult to estimate in advance how much space you'll need.  The best thing to do it take some backups then record the size of different types of backups (full/incr/diff) and measure the amount of WAL generated per day.  This will give you a general idea of how much space you'll need, though of course requirements will change over time as your database evolves.

```
default: /var/lib/backup
example: repo-path=/data/db/backrest
```

##### <a name="repo-remote-path-key"></a>`repo-remote-path` key

Remote repository path where WAL segments, backups, logs, etc are stored.

The remote repository is relative to the current installation of pgBackRest.  On a database server the backup server will be remote and visa versa for the backup server where the database server will be remote.  This option is only required if the remote repository has a different path than the local repository.
```
example: repo-remote-path=/backup/backrest
```

##### <a name="thread-max-key"></a>`thread-max` key

Max threads to use in process.

Each thread will perform compression and transfer to make the command run faster, but don't set `thread-max` so high that it impacts database performance.
```
default: 1
example: thread-max=4
```

##### <a name="thread-timeout-key"></a>`thread-timeout` key

Max time a thread can run.

This limits the amount of time (in seconds) that a thread might be stuck due to unforeseen issues executing the command.  Has no affect when `thread-max=1`.
```
example: thread-timeout=3600
```

#### <a name="backup-section"></a>`backup` section

The `backup` section defines settings related to backup.

##### <a name="archive-check-key"></a>`archive-check` key

Check that WAL segments are present in the archive before backup completes.

Checks that all WAL segments required to make the backup consistent are present in the WAL archive.  It's a good idea to leave this as the default unless you are using another method for archiving.
```
default: y
example: archive-check=n
```

##### <a name="archive-copy-key"></a>`archive-copy` key

Copy WAL segments needed for consistency to the backup.

This slightly paranoid option protects against corruption or premature expiration in the WAL segment archive by storing the WAL segments directly in the backup.  PITR won't be possible without the WAL segment archive and this option also consumes more space.

Even though WAL segments will be restored with the backup, PostgreSQL will ignore them if a `recovery.conf` file exists and instead use `archive_command` to fetch WAL segments.  Specifying `type=none` when restoring will not create `recovery.conf` and force PostgreSQL to use the WAL segments in pg_xlog.  This will get the database cluster to a consistent state.
```
default: n
example: archive-copy=y
```

##### <a name="backup-host-key"></a>`backup-host` key

Backup host when operating remotely via SSH.

Make sure that trusted SSH authentication is configured between the db host and the backup host.

When backing up to a locally mounted network filesystem this setting is not required.
```
example: backup-host=backup.domain.com
```

##### <a name="backup-user-key"></a>`backup-user` key

Backup host user when `backup-host` is set.

Defines the user that will be used for operations on the backup server.  Preferably this is not the `postgres` user but rather some other user like `backrest`.  If PostgreSQL runs on the backup server the `postgres` user can be placed in the `backrest` group so it has read permissions on the repository without being able to damage the contents accidentally.
```
example: backup-user=backrest
```

##### <a name="hardlink-key"></a>`hardlink` key

Hardlink files between backups.

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each backup is a full backup.  Be careful, though, because modifying files that are hard-linked can affect all the backups in the set.
```
default: n
example: hardlink=y
```

##### <a name="manifest-save-threshold-key"></a>`manifest-save-threshold` key

Manifest save threshold during backup.

Defines how often the manifest will be saved during a backup (in bytes).  Saving the manifest is important because it stores the checksums and allows the resume function to work efficiently.  The actual threshold used is 1% of the backup size or `manifest-save-threshold`, whichever is greater.
```
default: 1073741824
example: manifest-save-threshold=5368709120
```

##### <a name="resume-key"></a>`resume` key

Allow resume of failed backup.

Defines whether the resume feature is enabled.  Resume can greatly reduce the amount of time required to run a backup after a previous backup of the same type has failed.  It adds complexity, however, so it may be desirable to disable in environments that do not require the feature.
```
default: y
example: resume=false
```

##### <a name="start-fast-key"></a>`start-fast` key

Force a checkpoint to start backup quickly.

Forces a checkpoint (by passing `true` to the `fast` parameter of `pg_start_backup()`) so the backup begins immediately.  Otherwise the backup will start after the next regular checkpoint.

This feature only works in PostgreSQL <= `8.3`.
```
default: n
example: start-fast=y
```

##### <a name="stop-auto-key"></a>`stop-auto` key

Stop prior failed backup on new backup.

This will only be done if an exclusive advisory lock can be acquired to demonstrate that the prior failed backup process has really stopped.

This feature relies on pg_is_in_backup() so only works on PostgreSQL >= `9.3`.

The setting is disabled by default because it assumes that pgBackRest is the only process doing exclusive online backups.  It depends on an advisory lock that only pgBackRest sets so it may abort other processes that do exclusive online backups.  Note that `base_backup` and `pg_dump` are safe to use with this setting because they do not call `pg_start_backup()` so are not exclusive.
```
default: n
example: stop-auto=y
```

#### <a name="archive-section"></a>`archive` section

The `archive` section defines parameters when doing async archiving.  This means that the archive files will be stored locally, then a background process will pick them and move them to the backup.

##### <a name="archive-async-key"></a>`archive-async` key

Archive WAL segments asynchronously.

WAL segments will be copied to the local repo, then a process will be forked to compress the segment and transfer it to the remote repo if configured.  Control will be returned to PostgreSQL as soon as the WAL segment is copied locally.
```
default: n
example: archive-async=y
```

##### <a name="archive-max-mb-key"></a>`archive-max-mb` key

Limit size of the local asynchronous archive queue when `archive-async=y`.

After the limit is reached, the following will happen:

- pgBackRest will notify Postgres that the archive was successfully backed up, then DROP IT.
- An error will be logged to the console and also to the Postgres log.
- A stop file will be written in the lock directory and no more archive files will be backed up until it is removed.

If this occurs then the archive log stream will be interrupted and PITR will not be possible past that point.  A new backup will be required to regain full restore capability.

The purpose of this feature is to prevent the log volume from filling up at which point Postgres will stop completely.  Better to lose the backup than have PostgreSQL go down.

To start normal archiving again you'll need to remove the stop file which will be located at `${repo-path}/lock/${stanza}-archive.stop` where `${repo-path}` is the path set in the `general` section, and `${stanza}` is the backup stanza.
```
example: archive-max-mb=1024
```

#### <a name="restore-section"></a>`restore` section

The `restore` section defines settings used for restoring backups.

##### <a name="tablespace-key"></a>`tablespace` key

Restore tablespaces into original or remapped paths.

Defines whether tablespaces will be be restored into their original (or remapped) paths or stored directly under the `pg_tblspc` path.  Disabling this setting produces compact restores that are convenient for development, staging, etc.  Currently these restores cannot be backed up as pgBackRest expects only links in the `pg_tblspc` path. If no tablespaces are present this this setting has no effect.
```
default: y
example: tablespace=n
```

#### <a name="expire-section"></a>`expire` section

The `expire` section defines how long backups will be retained.  Expiration only occurs when the number of complete backups exceeds the allowed retention.  In other words, if full-retention is set to 2, then there must be 3 complete backups before the oldest will be expired.  Make sure you always have enough space for retention + 1 backups.

##### <a name="retention-full-key"></a>`retention-full` key

Number of full backups to retain.

When a full backup expires, all differential and incremental backups associated with the full backup will also expire.  When not defined then all full backups will be kept.
```
example: retention-full=2
```

##### <a name="retention-diff-key"></a>`retention-diff` key

Number of differential backups to retain.

When a differential backup expires, all incremental backups associated with the differential backup will also expire.  When not defined all differential backups will be kept.
```
example: retention-diff=3
```

##### <a name="retention-archive-type-key"></a>`retention-archive-type` key

Backup type for WAL retention.

If set to full, then pgBackRest will keep archive logs for the number of full backups defined by `retention-archive`.  If set to diff (differential), then pgBackRest will keep archive logs for the number of differential backups defined by `retention-archive`.

If not defined then archive logs will be kept indefinitely.  In general it is not useful to keep archive logs that are older than the oldest backup but there may occasionally be reasons for doing so.
```
default: full
example: retention-archive-type=diff
```

##### <a name="retention-archive-key"></a>`retention-archive` key

Number of backups worth of WAL to retain.

Number of backups worth of archive log to keep.  If this is set less than your backup retention then be sure you set `archive-copy=y` or you won't be able to restore some older backups.

For example, if `retention-archive=2` and `retention-full=4`, then any backups older than the most recent two full backups will not have WAL segments in the archive to make them consistent.  To solve this, set `archive-copy=y` and use `type=none` when restoring.  This issue will be addressed in a future release but for now be careful with this setting.
```
example: retention-archive=2
```

#### <a name="stanza-section"></a>`stanza` section

A stanza defines the backup configuration for a specific PostgreSQL database cluster.  The stanza section must define the database cluster path and host/user if the database cluster is remote.  Also, any global configuration sections can be overridden to define stanza-specific settings.

##### <a name="db-host-key"></a>`db-host` key

Cluster host for operating remotely via SSH.

Used for backups where the database cluster host is different from the backup host.
```
example: db-host=db.domain.com
```

##### <a name="db-user-key"></a>`db-user` key

Cluster host logon user when `db-host` is set.

This user will also own the remote pgBackRest process and will initiate connections to PostgreSQL.  For this to work correctly the user should be the PostgreSQL database cluster owner which is generally `postgres`, the default.
```
default: postgres
example: db-user=db_owner
```

##### <a name="db-path-key"></a>`db-path` key

Cluster data directory.

This should be the same as the `data_directory` setting in `postgresql.conf`.  Even though this value can be read from `postgresql.conf` or the database cluster it is prudent to set it in case those resources are not available during a restore or cold backup scenario.

The `db-path` option is tested against the value reported by PostgreSQL on every hot backup so it should always be current.
```
required: y
example: db-path=/data/db
```

##### <a name="db-port-key"></a>`db-port` key

Cluster port.

Port that PostgreSQL is running on.  This usually does not need to be specified as most database clusters run on the default port.
```
default: 5432
example: db-port=6543
```

##### <a name="db-socket-path-key"></a>`db-socket-path` key

cluster unix socket path.

The unix socket directory that was specified when PostgreSQL was started.  pgBackRest will automatically look in the standard location for your OS so there usually no need to specify this setting unless the socket directory was explicitly modified with the `unix_socket_directory` setting in `postgressql.conf`.
```
example: db-socket-path=/var/run/postgresql
```

## <a name="commands"></a>Commands

### <a name="general-options"></a>General Options

These options are either global or used by all commands.

#### <a name="config-option"></a>`config` option

pgBackRest configuration file.

Use this option to specify a different configuration file than the default.
```
default: /etc/pg_backrest.conf
example: config=/var/lib/backrest/pg_backrest.conf
```

#### <a name="stanza-option"></a>`stanza` option

Command stanza.

A stanza is the configuration for a PostgreSQL database cluster that defines where it is located, how it will be backed up, archiving options, etc.  Most db servers will only have one Postgres database cluster and therefore one stanza, whereas backup servers will have a stanza for every database cluster that needs to be backed up.

Examples of how to configure a stanza can be found in the `configuration examples` section.
```
required: y
example: stanza=main
```

### <a name="commands-inner"></a>Commands

#### <a name="backup-command"></a>`backup` command

Backup a database cluster.

pgBackRest does not have a built-in scheduler so it's best to run it from cron or some other scheduling mechanism.

##### <a name="type-option"></a>`type` option

Backup type.

The following backup types are supported:

- `full` - all database cluster files will be copied and there will be no dependencies on previous backups.
- `incr` - incremental from the last successful backup.
- `diff` - like an incremental backup but always based on the last full backup.

```
default: incr
example: --type=full
```

##### <a name="no-start-stop-option"></a>`no-start-stop` option

Perform cold backup.

This option prevents pgBackRest from running `pg_start_backup()` and `pg_stop_backup()` on the database cluster.  In order for this to work PostgreSQL should be shut down and pgBackRest will generate an error if it is not.

The purpose of this option is to allow cold backups.  The `pg_xlog` directory is copied as-is and `archive-check` is automatically disabled for the backup.
```
default: n
```

##### <a name="force-option"></a>`force` option

Force a cold backup.

When used with `--no-start-stop` a backup will be run even if pgBackRest thinks that PostgreSQL is running.  **This option should be used with extreme care as it will likely result in a bad backup.**

There are some scenarios where a backup might still be desirable under these conditions.  For example, if a server crashes and the database cluster volume can only be mounted read-only, it would be a good idea to take a backup even if `postmaster.pid` is present.  In this case it would be better to revert to the prior backup and replay WAL, but possibly there is a very important transaction in a WAL segment that did not get archived.
```
default: n
```

##### <a name="example-full-backup"></a>Example: Full Backup

```
pg_backrest --stanza=db --type=full backup
```
Run a `full` backup on the `db` stanza.  `--type` can also be set to `incr` or `diff` for incremental or differential backups.  However, if no `full` backup exists then a `full` backup will be forced even if `incr` or `diff` is requested.

#### <a name="archive-push-command"></a>`archive-push` command

Push a WAL segment to the archive.

The WAL segment may be pushed immediately to the archive or stored locally depending on the value of `archive-async`

##### <a name="archive-push-command-example"></a>Example

```
pg_backrest --stanza=db archive-push %p
```
Accepts a WAL segment from PostgreSQL and archives it in the repository defined by `repo-path`.  `%p` is how PostgreSQL specifies the location of the WAL segment to be archived.

#### <a name="archive-get-command"></a>`archive-get` command

Get a WAL segment from the archive.

WAL segments are required for restoring a PostgreSQL cluster or maintaining a replica.

##### <a name="archive-get-command-example"></a>Example

```
pg_backrest --stanza=db archive-get %f %p
```
Retrieves a WAL segment from the repository.  This command is used in `recovery.conf` to restore a backup, perform PITR, or as an alternative to streaming for keeping a replica up to date.  `%f` is how PostgreSQL specifies the WAL segment it needs and `%p` is the location where it should be copied.

#### <a name="expire-command"></a>`expire` command

Expire backups that exceed retention.

pgBackRest does backup rotation but is not concerned with when the backups were created.  If two full backups are configured for retention, pgBackRest will keep two full backups no matter whether they occur two hours or two weeks apart.

##### <a name="expire-command-example"></a>Example

```
pg_backrest --stanza=db expire
```
Expire (rotate) any backups that exceed the defined retention.  Expiration is run automatically after every successful backup, so there is no need to run this command separately unless you have reduced retention, usually to free up some space.

#### <a name="restore-command"></a>`restore` command

Restore a database cluster.

This command is generally run manually, but there are instances where it might be automated.

##### <a name="set-option"></a>`set` option

Backup set to restore.

The backup set to be restored.  `latest` will restore the latest backup, otherwise provide the name of the backup to restore.
```
default: latest
example: --set=20150131-153358F_20150131-153401I
```

##### <a name="delta-option"></a>`delta` option

Restore using delta.

By default the PostgreSQL data and tablespace directories are expected to be present but empty.  This option performs a delta restore using checksums.
```
default: n
```

##### <a name="force-option"></a>`force` option

Force a restore.

By itself this option forces the PostgreSQL data and tablespace paths to be completely overwritten.  In combination with `--delta` a timestamp/size delta will be performed instead of using checksums.
```
default: n
```

##### <a name="type-option"></a>`type` option

Recovery type.

The following recovery types are supported:

- `default` - recover to the end of the archive stream.
- `name` - recover the restore point specified in `--target`.
- `xid` - recover to the transaction id specified in `--target`.
- `time` - recover to the time specified in `--target`.
- `preserve` - preserve the existing `recovery.conf` file.
- `none` - no `recovery.conf` file is written so PostgreSQL will attempt to achieve consistency using WAL segments present in `pg_xlog`.  Provide the required WAL segments or use the `archive-copy` setting to include them with the backup.

```
default: default
example: --type=xid
```

##### <a name="target-option"></a>`target` option

Recovery target.

Defines the recovery target when `--type` is `name`, `xid`, or `time`.
```
required: y
example: "--target=2015-01-30 14:15:11 EST"
```

##### <a name="target-exclusive-option"></a>`target-exclusive` option

Stop just before the recovery target is reached.

Defines whether recovery to the target would be exclusive (the default is inclusive) and is only valid when `--type` is `time` or `xid`.  For example, using `--target-exclusive` would exclude the contents of transaction `1007` when `--type=xid` and `--target=1007`.  See the `recovery_target_inclusive` option in the PostgreSQL docs for more information.
```
default: n
```

##### <a name="target-resume-option"></a>`target-resume` option

Resume when recovery target is reached.

Specifies whether recovery should resume when the recovery target is reached.  See `pause_at_recovery_target` in the PostgreSQL docs for more information.
```
default: n
```

##### <a name="target-timeline-option"></a>`target-timeline` option

Recover along a timeline.

See `recovery_target_timeline` in the PostgreSQL docs for more information.
```
example: --target-timeline=3
```

##### <a name="recovery-option-option"></a>`recovery-option` option

Set an option in `recovery.conf`.

See http://www.postgresql.org/docs/X.X/static/recovery-config.html for details on recovery.conf options (replace X.X with your PostgreSQL version).  This option can be used multiple times.

Note: The `restore_command` option will be automatically generated but can be overridden with this option.  Be careful about specifying your own `restore_command` as pgBackRest is designed to handle this for you.  Target Recovery options (recovery_target_name, recovery_target_time, etc.) are generated automatically by pgBackRest and should not be set with this option.

Recovery settings can also be set in the `restore:recovery-option` section of pg_backrest.conf.  For example:
```
[restore:recovery-option]
primary_conn_info=db.mydomain.com
standby_mode=on
```
Since pgBackRest does not start PostgreSQL after writing the `recovery.conf` file, it is always possible to edit/check `recovery.conf` before manually restarting.
```
example: --recovery-option primary_conninfo=db.mydomain.com
```

##### <a name="tablespace-map-option"></a>`tablespace-map` option

Modify a tablespace path.

Moves a tablespace to a new location during the restore.  This is useful when tablespace locations are not the same on a replica, or an upgraded system has different mount points.

Since PostgreSQL 9.2 tablespace locations are not stored in pg_tablespace so moving tablespaces can be done with impunity.  However, moving a tablespace to the `data_directory` is not recommended and may cause problems.  For more information on moving tablespaces http://www.databasesoup.com/2013/11/moving-tablespaces.html is a good resource.
```
example: --tablespace-map ts_01=/db/ts_01
```

##### <a name="restore-latest-example"></a>Example: Restore Latest

```
pg_backrest --stanza=db --type=name --target=release restore
```
Restores the latest database cluster backup and then recovers to the `release` restore point.

#### <a name="info-command"></a>`info` command

Retrieve information about backups.

The `info` command operates on a single stanza or all stanzas.  Text output is the default and gives a human-readable summary of backups for the stanza(s) requested.  This format is subject to change with any release.

For machine-readable output use `--output=json`.  The JSON output contains far more information than the text output, however **this feature is currently experimental so the format may change between versions**.

##### <a name="info-output-option"></a>`output` option

Output format.

The following output types are supported:

- `text` - Human-readable summary of backup information.
- `json` - Exhaustive machine-readable backup information in JSON format.

```
default: text
example: --output=json
```

##### <a name="info-single-stanza-example"></a>Example: Information for a single stanza

```
pg_backrest --stanza=db --output=json info
```

Get information about backups in the `db` stanza.

##### <a name="info-all-stanza-example"></a>Example: Information for all stanzas

```
pg_backrest --output=json info
```

Get information about backups for all stanzas in the repository.

#### <a name="help-command"></a>`help` command

Get help.

Three levels of help are provided.  If no command is specified then general help will be displayed.  If a command is specified then a full description of the command will be displayed along with a list of valid options.  If an option is specified in addition to a command then the a full description of the option as it applies to the command will be displayed.

##### <a name="help-backup-command-example"></a>Example: Help for the backup command

```
pg_backrest help backup
```

Get help for the backup command.

##### <a name="help-backup-command-force-example"></a>Example: Help for backup command, --force option

```
pg_backrest help backup force
```

Get help for the force option of the backup command.

#### <a name="version-command"></a>`version` command

Get version.

Displays installed pgBackRest version.

##### <a name="get-version-example"></a>Example: Get version

```
pg_backrest version
```

Get pgBackRest version.
