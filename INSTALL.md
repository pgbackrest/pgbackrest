# pg_backrest installation

## parameters

## configuration

BackRest takes some command-line parameters, but depends on a configuration file for most of the settings.  The default location for the configuration file is /etc/pg_backrest.conf.

### configuration sections

Each section defines important aspects of the backup.

#### command section

The command section defines external commands that are used by BackRest.

##### psql key

Defines the full path to psql.  psql is used to call pg_start_backup() and pg_stop_backup().

Required on whichever server is doing the backup, but can be omitted if the --no-start-stop backup parameter is used.  

_required_: N
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

##### level-file key

__default__: info

Sets file log level.

_Example_: level-file=warn

##### level-console key

Sets console log level.

_default_: error
_example_: level-file=info

#### backup section

The backup section defines settings related to backup and archiving.

##### host key

__Required__: N (but must be set if user is defined)

Sets the backup host.

_Example_: host=backup.mydomain.com

##### user key

__Required__: N (but must be set if host is defined)

Sets user account on the backup host.

_Example_: user=backrest

##### path key

__Required__: Y

Path where backups are stored on the local or remote host.

_Example_: path=/backup/backrest

##### compress key

__Default__: Y

Enable gzip compression.  Files stored in the backup are compatible with command-line gzip tools.

_Example_: compress=n

##### checksum key

__Default__: Y

Enable SHA-1 checksums.  Backup checksums are stored in backup.manifest while archive checksums are stored in the filename.

_Example_: checksum=n

##### hardlink key

__Default__: N

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each
backup is a full backup.  Be care though, because modifying files that are hard-linked can affect all the backups in the set.

_Example_: hardlink=y

##### thread_max key

__default__: 1

Enable hard-linking of files in differential and incremental backups to their full backups.  This gives the appearance that each
backup is a full backup.  Be care though, because modifying files that are hard-linked can affect all the backups in the set.

_Example_: hardlink=y



```
    CONFIG_SECTION_COMMAND        => "command",
    CONFIG_SECTION_COMMAND_OPTION => "command:option",
    CONFIG_SECTION_LOG            => "log",
    CONFIG_SECTION_BACKUP         => "backup",
    CONFIG_SECTION_ARCHIVE        => "archive",
    CONFIG_SECTION_RETENTION      => "retention",
    CONFIG_SECTION_STANZA         => "stanza",

    CONFIG_KEY_USER               => "user",
    CONFIG_KEY_HOST               => "host",
    CONFIG_KEY_PATH               => "path",

    CONFIG_KEY_THREAD_MAX         => "thread-max",
    CONFIG_KEY_THREAD_TIMEOUT     => "thread-timeout",
    CONFIG_KEY_ARCHIVE_REQUIRED   => "archive-required",
    CONFIG_KEY_ARCHIVE_MAX_MB     => "archive-max-mb",
    CONFIG_KEY_START_FAST         => "start_fast",
    CONFIG_KEY_COMPRESS_ASYNC     => "compress-async",
```

### configuration examples

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

create backrest user
setup trusted ssh between test user account and backrest
backrest user and test user must be in the same group
