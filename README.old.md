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
cpanm Compress::ZLib
```
5. Install PgBackRest

Backrest can be installed by downloading the most recent release:

https://github.com/pgmasters/backrest/releases

6. To run unit tests:

* Create backrest_dev user
* Setup trusted ssh between test user account and backrest_dev
* Backrest user and test user must be in the same group

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

#### restore options

##### set

The backup set to be restored.  `latest` will restore the latest backup, otherwise provide the name of the backup to restore.  For example: `20150131-153358F` or `20150131-153358F_20150131-153401I`.

##### delta

By default the database base and tablespace directories are expected to be present but empty.  This option performs a delta restore using checksums.

##### force

By itself this option forces the database base and tablespace paths to be completely overwritten.  In combination with `--delta` a timestamp/size delta will be performed instead of using checksums.

#### recovery options

##### type

The following recovery types are supported:

- `default` - recover to the end of the archive stream.
- `name` - recover the restore point specified in `--target`.
- `xid` - recover to the transaction id specified in `--target`.
- `time` - recover to the time specified in `--target`.
- `preserve` - preserve the existing `recovery.conf` file.
- `none` - no recovery past database becoming consistent.

Note that the `none` option may produce duplicate WAL if the database is started with archive logging enabled.  It is recommended that a new stanza be created for production databases restored in this way.

##### target

Defines the recovery target when `--type` is `name`, `xid`, or `time`.  For example, `--type=time` and `--target=2015-01-30 14:15:11 EST`.

##### target-exclusive

Defines whether recovery to the target whould be exclusive (the default is inclusive) and is only valid when `--type` is `time` or `xid`.  For example, using `--target-exclusive` would exclude the contents of transaction `1007` when `--type=xid` and `-target=1007`.  See `recovery_target_inclusive` option in Postgres docs for more information.

##### target-resume

Specifies whether recovery should resume when the recovery target is reached.  See `pause_at_recovery_target` in Postgres docs for more information.

##### target-timeline

Recovers along the specified timeline.  See `recovery_target_timeline` in Postgres docs for more information.

#### usage examples

```
/path/to/pg_backrest.pl --stanza=db --set=latest --type=name --target=release
```
Restores the latest database backup and then recovers to the `release` restore point.

**MORE TO BE ADDED HERE**

PITR should start after the stop time in the .backup file.

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
