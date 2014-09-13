# pg_backrest installation

## parameters

## configuration

BackRest takes some command-line parameters, but depends on a configuration file for most of the settings.  The default location for the configuration file is /etc/pg_backrest.conf.

### configuration sections

Each section defines important aspects of the backup.

#### command section

The command section defines external commands that are used by BackRest.

-------------------------------
Key    | Required | Description
-------|:--------:| -----------
psql   |N         | This is going to be a mult-line description.
                    Not sure exactly how that _works_.

remote |N         |
-------------------------------

##### psql key (optional)

Defines the full path to psql.  psql is used to call pg\_start\_backup() and pg\_stop\_backup().

Example:
psql=/usr/bin/psql

##### remote key

Defines the file path to pg\_backrest\_remote.pl. If this parameter is omitted 


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
    CONFIG_KEY_HARDLINK           => "hardlink",
    CONFIG_KEY_ARCHIVE_REQUIRED   => "archive-required",
    CONFIG_KEY_ARCHIVE_MAX_MB     => "archive-max-mb",
    CONFIG_KEY_START_FAST         => "start_fast",
    CONFIG_KEY_COMPRESS_ASYNC     => "compress-async",

    CONFIG_KEY_LEVEL_FILE         => "level-file",
    CONFIG_KEY_LEVEL_CONSOLE      => "level-console",

    CONFIG_KEY_COMPRESS           => "compress",
    CONFIG_KEY_CHECKSUM           => "checksum",
    CONFIG_KEY_PSQL               => "psql",
    CONFIG_KEY_REMOTE             => "remote"

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
