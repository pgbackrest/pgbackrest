####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package pgBackRest::Config::Config;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Getopt::Long qw(GetOptions);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::RemoteMaster;
use pgBackRest::Version;

####################################################################################################################################
# DB/BACKUP Constants
####################################################################################################################################
use constant DB                                                     => 'db';
    push @EXPORT, qw(DB);
use constant BACKUP                                                 => 'backup';
    push @EXPORT, qw(BACKUP);
use constant NONE                                                   => 'none';
    push @EXPORT, qw(NONE);

####################################################################################################################################
# Command constants - basic commands that are allowed in backrest
####################################################################################################################################
my %oCommandHash;

use constant CMD_ARCHIVE_GET                                        => 'archive-get';
    push @EXPORT, qw(CMD_ARCHIVE_GET);
    $oCommandHash{&CMD_ARCHIVE_GET} = true;
use constant CMD_ARCHIVE_PUSH                                       => 'archive-push';
    push @EXPORT, qw(CMD_ARCHIVE_PUSH);
    $oCommandHash{&CMD_ARCHIVE_PUSH} = true;
use constant CMD_BACKUP                                             => 'backup';
    push @EXPORT, qw(CMD_BACKUP);
    $oCommandHash{&CMD_BACKUP} = true;
use constant CMD_CHECK                                              => 'check';
    push @EXPORT, qw(CMD_CHECK);
    $oCommandHash{&CMD_CHECK} = true;
use constant CMD_EXPIRE                                             => 'expire';
    push @EXPORT, qw(CMD_EXPIRE);
    $oCommandHash{&CMD_EXPIRE} = true;
use constant CMD_HELP                                               => 'help';
    push @EXPORT, qw(CMD_HELP);
    $oCommandHash{&CMD_HELP} = true;
use constant CMD_INFO                                               => 'info';
    push @EXPORT, qw(CMD_INFO);
    $oCommandHash{&CMD_INFO} = true;
use constant CMD_REMOTE                                             => 'remote';
    push @EXPORT, qw(CMD_REMOTE);
    $oCommandHash{&CMD_REMOTE} = true;
use constant CMD_RESTORE                                            => 'restore';
    push @EXPORT, qw(CMD_RESTORE);
    $oCommandHash{&CMD_RESTORE} = true;
use constant CMD_START                                              => 'start';
    push @EXPORT, qw(CMD_START);
    $oCommandHash{&CMD_START} = true;
use constant CMD_STOP                                               => 'stop';
    push @EXPORT, qw(CMD_STOP);
    $oCommandHash{&CMD_STOP} = true;
use constant CMD_VERSION                                            => 'version';
    push @EXPORT, qw(CMD_VERSION);
    $oCommandHash{&CMD_VERSION} = true;

####################################################################################################################################
# BACKUP Type Constants
####################################################################################################################################
use constant BACKUP_TYPE_FULL                                       => 'full';
    push @EXPORT, qw(BACKUP_TYPE_FULL);
use constant BACKUP_TYPE_DIFF                                       => 'diff';
    push @EXPORT, qw(BACKUP_TYPE_DIFF);
use constant BACKUP_TYPE_INCR                                       => 'incr';
    push @EXPORT, qw(BACKUP_TYPE_INCR);

####################################################################################################################################
# INFO Output Constants
####################################################################################################################################
use constant INFO_OUTPUT_TEXT                                       => 'text';
    push @EXPORT, qw(INFO_OUTPUT_TEXT);
use constant INFO_OUTPUT_JSON                                       => 'json';
    push @EXPORT, qw(INFO_OUTPUT_JSON);

####################################################################################################################################
# SOURCE Constants
####################################################################################################################################
use constant SOURCE_CONFIG                                          => 'config';
    push @EXPORT, qw(SOURCE_CONFIG);
use constant SOURCE_PARAM                                           => 'param';
    push @EXPORT, qw(SOURCE_PARAM);
use constant SOURCE_DEFAULT                                         => 'default';
    push @EXPORT, qw(SOURCE_DEFAULT);

####################################################################################################################################
# RECOVERY Type Constants
####################################################################################################################################
use constant RECOVERY_TYPE_NAME                                     => 'name';
    push @EXPORT, qw(RECOVERY_TYPE_NAME);
use constant RECOVERY_TYPE_TIME                                     => 'time';
    push @EXPORT, qw(RECOVERY_TYPE_TIME);
use constant RECOVERY_TYPE_XID                                      => 'xid';
    push @EXPORT, qw(RECOVERY_TYPE_XID);
use constant RECOVERY_TYPE_PRESERVE                                 => 'preserve';
    push @EXPORT, qw(RECOVERY_TYPE_PRESERVE);
use constant RECOVERY_TYPE_NONE                                     => 'none';
    push @EXPORT, qw(RECOVERY_TYPE_NONE);
use constant RECOVERY_TYPE_IMMEDIATE                                => 'immediate';
    push @EXPORT, qw(RECOVERY_TYPE_IMMEDIATE);
use constant RECOVERY_TYPE_DEFAULT                                  => 'default';
    push @EXPORT, qw(RECOVERY_TYPE_DEFAULT);

####################################################################################################################################
# RECOVERY Action Constants
####################################################################################################################################
use constant RECOVERY_ACTION_PAUSE                                  => 'pause';
    push @EXPORT, qw(RECOVERY_ACTION_PAUSE);
use constant RECOVERY_ACTION_PROMOTE                                => 'promote';
    push @EXPORT, qw(RECOVERY_ACTION_PROMOTE);
use constant RECOVERY_ACTION_SHUTDOWN                               => 'shutdown';
    push @EXPORT, qw(RECOVERY_ACTION_SHUTDOWN);

####################################################################################################################################
# Option Rules
####################################################################################################################################
use constant OPTION_RULE_ALLOW_LIST                                 => 'allow-list';
    push @EXPORT, qw(OPTION_RULE_ALLOW_LIST);
use constant OPTION_RULE_ALLOW_RANGE                                => 'allow-range';
    push @EXPORT, qw(OPTION_RULE_ALLOW_RANGE);
use constant OPTION_RULE_DEFAULT                                    => 'default';
    push @EXPORT, qw(OPTION_RULE_DEFAULT);
use constant OPTION_RULE_DEPEND                                     => 'depend';
    push @EXPORT, qw(OPTION_RULE_DEPEND);
use constant OPTION_RULE_DEPEND_OPTION                              => 'depend-option';
    push @EXPORT, qw(OPTION_RULE_DEPEND_OPTION);
use constant OPTION_RULE_DEPEND_LIST                                => 'depend-list';
    push @EXPORT, qw(OPTION_RULE_DEPEND_LIST);
use constant OPTION_RULE_DEPEND_VALUE                               => 'depend-value';
    push @EXPORT, qw(OPTION_RULE_DEPEND_VALUE);
use constant OPTION_RULE_HASH_VALUE                                 => 'hash-value';
    push @EXPORT, qw(OPTION_RULE_HASH_VALUE);
use constant OPTION_RULE_HINT                                       => 'hint';
    push @EXPORT, qw(OPTION_RULE_HINT);
use constant OPTION_RULE_NEGATE                                     => 'negate';
    push @EXPORT, qw(OPTION_RULE_NEGATE);
use constant OPTION_RULE_COMMAND                                    => 'command';
    push @EXPORT, qw(OPTION_RULE_COMMAND);
use constant OPTION_RULE_REQUIRED                                   => 'required';
    push @EXPORT, qw(OPTION_RULE_REQUIRED);
use constant OPTION_RULE_SECTION                                    => 'section';
    push @EXPORT, qw(OPTION_RULE_SECTION);
use constant OPTION_RULE_TYPE                                       => 'type';
    push @EXPORT, qw(OPTION_RULE_TYPE);

####################################################################################################################################
# Option Types
####################################################################################################################################
use constant OPTION_TYPE_BOOLEAN                                    => 'boolean';
    push @EXPORT, qw(OPTION_TYPE_BOOLEAN);
use constant OPTION_TYPE_FLOAT                                      => 'float';
    push @EXPORT, qw(OPTION_TYPE_FLOAT);
use constant OPTION_TYPE_HASH                                       => 'hash';
    push @EXPORT, qw(OPTION_TYPE_HASH);
use constant OPTION_TYPE_INTEGER                                    => 'integer';
    push @EXPORT, qw(OPTION_TYPE_INTEGER);
use constant OPTION_TYPE_STRING                                     => 'string';
    push @EXPORT, qw(OPTION_TYPE_STRING);

####################################################################################################################################
# Configuration section constants
####################################################################################################################################
use constant CONFIG_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CONFIG_SECTION_GLOBAL);
use constant CONFIG_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CONFIG_SECTION_STANZA);

####################################################################################################################################
# Option constants
####################################################################################################################################
# Command-line only
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_CONFIG                                          => 'config';
    push @EXPORT, qw(OPTION_CONFIG);
use constant OPTION_DELTA                                           => 'delta';
    push @EXPORT, qw(OPTION_DELTA);
use constant OPTION_FORCE                                           => 'force';
    push @EXPORT, qw(OPTION_FORCE);
use constant OPTION_ONLINE                                          => 'online';
    push @EXPORT, qw(OPTION_ONLINE);
use constant OPTION_SET                                             => 'set';
    push @EXPORT, qw(OPTION_SET);
use constant OPTION_STANZA                                          => 'stanza';
    push @EXPORT, qw(OPTION_STANZA);
use constant OPTION_TARGET                                          => 'target';
    push @EXPORT, qw(OPTION_TARGET);
use constant OPTION_TARGET_EXCLUSIVE                                => 'target-exclusive';
    push @EXPORT, qw(OPTION_TARGET_EXCLUSIVE);
use constant OPTION_TARGET_ACTION                                   => 'target-action';
    push @EXPORT, qw(OPTION_TARGET_ACTION);
use constant OPTION_TARGET_TIMELINE                                 => 'target-timeline';
    push @EXPORT, qw(OPTION_TARGET_TIMELINE);
use constant OPTION_TYPE                                            => 'type';
    push @EXPORT, qw(OPTION_TYPE);
use constant OPTION_OUTPUT                                          => 'output';
    push @EXPORT, qw(OPTION_OUTPUT);
use constant OPTION_LOCK                                            => 'lock';
    push @EXPORT, qw(OPTION_LOCK);

# Command-line only help/version
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_HELP                                            => 'help';
    push @EXPORT, qw(OPTION_HELP);
use constant OPTION_VERSION                                         => 'version';
    push @EXPORT, qw(OPTION_VERSION);

# Command-line only remote
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_COMMAND                                          => 'command';
    push @EXPORT, qw(OPTION_COMMAND);
use constant OPTION_PROCESS                                          => 'process';
    push @EXPORT, qw(OPTION_PROCESS);

# Command-line only test
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_TEST                                            => 'test';
    push @EXPORT, qw(OPTION_TEST);
use constant OPTION_TEST_DELAY                                      => 'test-delay';
    push @EXPORT, qw(OPTION_TEST_DELAY);
use constant OPTION_TEST_NO_FORK                                    => 'no-fork';
    push @EXPORT, qw(OPTION_TEST_NO_FORK);
use constant OPTION_TEST_POINT                                      => 'test-point';
    push @EXPORT, qw(OPTION_TEST_POINT);

# GENERAL Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_ARCHIVE_TIMEOUT                                 => 'archive-timeout';
    push @EXPORT, qw(OPTION_ARCHIVE_TIMEOUT);
use constant OPTION_BUFFER_SIZE                                     => 'buffer-size';
    push @EXPORT, qw(OPTION_BUFFER_SIZE);
use constant OPTION_CONFIG_REMOTE                                   => 'config-remote';
    push @EXPORT, qw(OPTION_CONFIG_REMOTE);
use constant OPTION_DB_TIMEOUT                                      => 'db-timeout';
    push @EXPORT, qw(OPTION_DB_TIMEOUT);
use constant OPTION_COMPRESS                                        => 'compress';
    push @EXPORT, qw(OPTION_COMPRESS);
use constant OPTION_COMPRESS_LEVEL                                  => 'compress-level';
    push @EXPORT, qw(OPTION_COMPRESS_LEVEL);
use constant OPTION_COMPRESS_LEVEL_NETWORK                          => 'compress-level-network';
    push @EXPORT, qw(OPTION_COMPRESS_LEVEL_NETWORK);
use constant OPTION_NEUTRAL_UMASK                                   => 'neutral-umask';
    push @EXPORT, qw(OPTION_NEUTRAL_UMASK);
use constant OPTION_THREAD_MAX                                      => 'thread-max';
    push @EXPORT, qw(OPTION_THREAD_MAX);
use constant OPTION_THREAD_TIMEOUT                                  => 'thread-timeout';
    push @EXPORT, qw(OPTION_THREAD_TIMEOUT);

# Paths
use constant OPTION_LOCK_PATH                                       => 'lock-path';
    push @EXPORT, qw(OPTION_LOCK_PATH);
use constant OPTION_LOG_PATH                                        => 'log-path';
    push @EXPORT, qw(OPTION_LOG_PATH);
use constant OPTION_REPO_PATH                                       => 'repo-path';
    push @EXPORT, qw(OPTION_REPO_PATH);
use constant OPTION_SPOOL_PATH                                      => 'spool-path';
    push @EXPORT, qw(OPTION_SPOOL_PATH);

# Remote command
use constant OPTION_COMMAND_REMOTE                                  => 'cmd-remote';
    push @EXPORT, qw(OPTION_COMMAND_REMOTE);

# Log level
use constant OPTION_LOG_LEVEL_CONSOLE                               => 'log-level-console';
    push @EXPORT, qw(OPTION_LOG_LEVEL_CONSOLE);
use constant OPTION_LOG_LEVEL_FILE                                  => 'log-level-file';
    push @EXPORT, qw(OPTION_LOG_LEVEL_FILE);

# ARCHIVE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_ARCHIVE_ASYNC                                   => 'archive-async';
    push @EXPORT, qw(OPTION_ARCHIVE_ASYNC);
use constant OPTION_ARCHIVE_MAX_MB                                  => 'archive-max-mb';
    push @EXPORT, qw(OPTION_ARCHIVE_MAX_MB);

# BACKUP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_BACKUP_ARCHIVE_CHECK                            => 'archive-check';
    push @EXPORT, qw(OPTION_BACKUP_ARCHIVE_CHECK);
use constant OPTION_BACKUP_ARCHIVE_COPY                             => 'archive-copy';
    push @EXPORT, qw(OPTION_BACKUP_ARCHIVE_COPY);
use constant OPTION_BACKUP_HOST                                     => 'backup-host';
    push @EXPORT, qw(OPTION_BACKUP_HOST);
use constant OPTION_BACKUP_USER                                     => 'backup-user';
    push @EXPORT, qw(OPTION_BACKUP_USER);
use constant OPTION_HARDLINK                                        => 'hardlink';
    push @EXPORT, qw(OPTION_HARDLINK);
use constant OPTION_MANIFEST_SAVE_THRESHOLD                         => 'manifest-save-threshold';
    push @EXPORT, qw(OPTION_MANIFEST_SAVE_THRESHOLD);
use constant OPTION_RESUME                                          => 'resume';
    push @EXPORT, qw(OPTION_RESUME);
use constant OPTION_START_FAST                                      => 'start-fast';
    push @EXPORT, qw(OPTION_START_FAST);
use constant OPTION_STOP_AUTO                                       => 'stop-auto';
    push @EXPORT, qw(OPTION_STOP_AUTO);

# EXPIRE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_RETENTION_ARCHIVE                               => 'retention-archive';
    push @EXPORT, qw(OPTION_RETENTION_ARCHIVE);
use constant OPTION_RETENTION_ARCHIVE_TYPE                          => 'retention-archive-type';
    push @EXPORT, qw(OPTION_RETENTION_ARCHIVE_TYPE);
use constant OPTION_RETENTION_DIFF                                  => 'retention-' . BACKUP_TYPE_DIFF;
    push @EXPORT, qw(OPTION_RETENTION_DIFF);
use constant OPTION_RETENTION_FULL                                  => 'retention-' . BACKUP_TYPE_FULL;
    push @EXPORT, qw(OPTION_RETENTION_FULL);

# RESTORE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DB_INCLUDE                                      => 'db-include';
    push @EXPORT, qw(OPTION_DB_INCLUDE);
use constant OPTION_LINK_ALL                                        => 'link-all';
    push @EXPORT, qw(OPTION_LINK_ALL);
use constant OPTION_LINK_MAP                                        => 'link-map';
    push @EXPORT, qw(OPTION_LINK_MAP);
use constant OPTION_TABLESPACE_MAP_ALL                              => 'tablespace-map-all';
    push @EXPORT, qw(OPTION_TABLESPACE_MAP_ALL);
use constant OPTION_TABLESPACE_MAP                                  => 'tablespace-map';
    push @EXPORT, qw(OPTION_TABLESPACE_MAP);
use constant OPTION_RESTORE_RECOVERY_OPTION                         => 'recovery-option';
    push @EXPORT, qw(OPTION_RESTORE_RECOVERY_OPTION);

# STANZA Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DB_HOST                                         => 'db-host';
    push @EXPORT, qw(OPTION_DB_HOST);
use constant OPTION_DB_PATH                                         => 'db-path';
    push @EXPORT, qw(OPTION_DB_PATH);
use constant OPTION_DB_PORT                                         => 'db-port';
    push @EXPORT, qw(OPTION_DB_PORT);
use constant OPTION_DB_SOCKET_PATH                                  => 'db-socket-path';
    push @EXPORT, qw(OPTION_DB_SOCKET_PATH);
use constant OPTION_DB_USER                                         => 'db-user';
    push @EXPORT, qw(OPTION_DB_USER);

####################################################################################################################################
# Option Defaults
####################################################################################################################################
# Command-line only
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_BACKUP_TYPE                             => BACKUP_TYPE_INCR;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_TYPE);
use constant OPTION_DEFAULT_INFO_OUTPUT                             => INFO_OUTPUT_TEXT;
    push @EXPORT, qw(OPTION_DEFAULT_INFO_OUTPUT);
use constant OPTION_DEFAULT_RESTORE_DELTA                           => false;
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_DELTA);
use constant OPTION_DEFAULT_RESTORE_FORCE                           => false;
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_FORCE);
use constant OPTION_DEFAULT_RESTORE_SET                             => 'latest';
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_SET);
use constant OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE                => false;
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE);
use constant OPTION_DEFAULT_RESTORE_TARGET_ACTION                   => RECOVERY_ACTION_PAUSE;
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_TARGET_ACTION);
use constant OPTION_DEFAULT_RESTORE_TYPE                            => RECOVERY_TYPE_DEFAULT;
    push @EXPORT, qw(OPTION_DEFAULT_RESTORE_TYPE);
use constant OPTION_DEFAULT_LOCK                                    => true;
    push @EXPORT, qw(OPTION_DEFAULT_LOCK);

# Command-line only test
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_TEST                                    => false;
    push @EXPORT, qw(OPTION_DEFAULT_TEST);
use constant OPTION_DEFAULT_TEST_DELAY                              => 5;
    push @EXPORT, qw(OPTION_DEFAULT_TEST_DELAY);
use constant OPTION_DEFAULT_TEST_NO_FORK                            => false;
    push @EXPORT, qw(OPTION_DEFAULT_TEST_NO_FORK);

# GENERAL Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_ARCHIVE_TIMEOUT                         => 60;
    push @EXPORT, qw(OPTION_DEFAULT_ARCHIVE_TIMEOUT);
use constant OPTION_DEFAULT_ARCHIVE_TIMEOUT_MIN                     => WAIT_TIME_MINIMUM;
    push @EXPORT, qw(OPTION_DEFAULT_ARCHIVE_TIMEOUT_MIN);
use constant OPTION_DEFAULT_ARCHIVE_TIMEOUT_MAX                     => 86400;
    push @EXPORT, qw(OPTION_DEFAULT_ARCHIVE_TIMEOUT_MAX);

use constant OPTION_DEFAULT_BUFFER_SIZE                             => 4194304;
    push @EXPORT, qw(OPTION_DEFAULT_BUFFER_SIZE);
use constant OPTION_DEFAULT_BUFFER_SIZE_MIN                         => 16384;
    push @EXPORT, qw(OPTION_DEFAULT_BUFFER_SIZE_MIN);
use constant OPTION_DEFAULT_BUFFER_SIZE_MAX                         => 8388608;
    push @EXPORT, qw(OPTION_DEFAULT_BUFFER_SIZE_MAX);

use constant OPTION_DEFAULT_COMPRESS                                => true;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS);

use constant OPTION_DEFAULT_COMPRESS_LEVEL                          => 6;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL);
use constant OPTION_DEFAULT_COMPRESS_LEVEL_MIN                      => 0;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL_MIN);
use constant OPTION_DEFAULT_COMPRESS_LEVEL_MAX                      => 9;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL_MAX);

use constant OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK                  => 3;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK);
use constant OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN              => 0;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN);
use constant OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX              => 9;
    push @EXPORT, qw(OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX);

use constant OPTION_DEFAULT_DB_TIMEOUT                              => 1800;
    push @EXPORT, qw(OPTION_DEFAULT_DB_TIMEOUT);
use constant OPTION_DEFAULT_DB_TIMEOUT_MIN                          => WAIT_TIME_MINIMUM;
    push @EXPORT, qw(OPTION_DEFAULT_DB_TIMEOUT_MIN);
use constant OPTION_DEFAULT_DB_TIMEOUT_MAX                          => 86400 * 7;
    push @EXPORT, qw(OPTION_DEFAULT_DB_TIMEOUT_MAX);

use constant OPTION_DEFAULT_CONFIG                                  => '/etc/' . BACKREST_EXE . '.conf';
    push @EXPORT, qw(OPTION_DEFAULT_CONFIG);
use constant OPTION_DEFAULT_LOCK_PATH                               => '/tmp/' . BACKREST_EXE;
    push @EXPORT, qw(OPTION_DEFAULT_LOCK_PATH);
use constant OPTION_DEFAULT_LOG_PATH                                => '/var/log/' . BACKREST_EXE;
    push @EXPORT, qw(OPTION_DEFAULT_LOG_PATH);
use constant OPTION_DEFAULT_NEUTRAL_UMASK                           => true;
    push @EXPORT, qw(OPTION_DEFAULT_NEUTRAL_UMASK);
use constant OPTION_DEFAULT_REPO_PATH                               => '/var/lib/' . BACKREST_EXE;
    push @EXPORT, qw(OPTION_DEFAULT_REPO_PATH);
use constant OPTION_DEFAULT_SPOOL_PATH                              => '/var/spool/' . BACKREST_EXE;
    push @EXPORT, qw(OPTION_DEFAULT_SPOOL_PATH);
use constant OPTION_DEFAULT_THREAD_MAX                              => 1;
    push @EXPORT, qw(OPTION_DEFAULT_THREAD_MAX);
use constant OPTION_DEFAULT_THREAD_MAX_MIN                          => 1;
    push @EXPORT, qw(OPTION_DEFAULT_THREAD_MAX_MIN);
use constant OPTION_DEFAULT_THREAD_MAX_MAX                          => 256;
    push @EXPORT, qw(OPTION_DEFAULT_THREAD_MAX_MAX);

# COMMAND Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_COMMAND_REMOTE                          => abs_path($0);
    push @EXPORT, qw(OPTION_DEFAULT_COMMAND_REMOTE);

# LOG Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_LOG_LEVEL_CONSOLE                       => lc(WARN);
    push @EXPORT, qw(OPTION_DEFAULT_LOG_LEVEL_CONSOLE);
use constant OPTION_DEFAULT_LOG_LEVEL_FILE                          => lc(INFO);
    push @EXPORT, qw(OPTION_DEFAULT_LOG_LEVEL_FILE);

# ARCHIVE SECTION
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_ARCHIVE_ASYNC                           => false;
    push @EXPORT, qw(OPTION_DEFAULT_ARCHIVE_ASYNC);

# BACKUP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_BACKUP_ARCHIVE_CHECK                    => true;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_ARCHIVE_CHECK);
use constant OPTION_DEFAULT_BACKUP_ARCHIVE_COPY                     => false;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_ARCHIVE_COPY);
use constant OPTION_DEFAULT_BACKUP_FORCE                            => false;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_FORCE);
use constant OPTION_DEFAULT_BACKUP_HARDLINK                         => false;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_HARDLINK);
use constant OPTION_DEFAULT_BACKUP_ONLINE                           => true;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_ONLINE);
use constant OPTION_DEFAULT_BACKUP_MANIFEST_SAVE_THRESHOLD          => 1073741824;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_MANIFEST_SAVE_THRESHOLD);
use constant OPTION_DEFAULT_BACKUP_RESUME                           => true;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_RESUME);
use constant OPTION_DEFAULT_BACKUP_STOP_AUTO                        => false;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_STOP_AUTO);
use constant OPTION_DEFAULT_BACKUP_START_FAST                       => false;
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_START_FAST);
use constant OPTION_DEFAULT_BACKUP_USER                             => 'backrest';
    push @EXPORT, qw(OPTION_DEFAULT_BACKUP_USER);

# START/STOP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_STOP_FORCE                              => false;
    push @EXPORT, qw(OPTION_DEFAULT_STOP_FORCE);

# EXPIRE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE                  => BACKUP_TYPE_FULL;
    push @EXPORT, qw(OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE);
use constant OPTION_DEFAULT_RETENTION_MIN                           => 1;
    push @EXPORT, qw(OPTION_DEFAULT_RETENTION_MIN);
use constant OPTION_DEFAULT_RETENTION_MAX                           => 999999999;
    push @EXPORT, qw(OPTION_DEFAULT_RETENTION_MAX);

# RESTORE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_LINK_ALL                                => false;
    push @EXPORT, qw(OPTION_DEFAULT_LINK_ALL);

# STANZA Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant OPTION_DEFAULT_DB_PORT                                 => 5432;
    push @EXPORT, qw(OPTION_DEFAULT_DB_PORT);
use constant OPTION_DEFAULT_DB_USER                                 => 'postgres';
    push @EXPORT, qw(OPTION_DEFAULT_DB_USER);

####################################################################################################################################
# Option Rule Hash
#
# pgbackrest will throw an error if:
#   1) an option is provided when executing the command that is not listed in the OPTION_RULE_COMMAND section of the Option Rule Hash
#   2) or an option is not provided when executing the command and it is listed in the OPTION_RULE_COMMAND section as "true"
# If an OPTION_RULE_COMMAND is set to "false" then pgbackrest will not throw an error if the option is missing and also will not throw an
# error if it exists.
####################################################################################################################################
my %oOptionRule =
(
    # Command-line only
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_CONFIG =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_CONFIG,
        &OPTION_RULE_NEGATE => true,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => true,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true
        }
    },

    &OPTION_DELTA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_DELTA,
            }
        }
    },

    &OPTION_FORCE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_FORCE,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION  => OPTION_ONLINE,
                    &OPTION_RULE_DEPEND_VALUE   => false
                }
            },

            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_FORCE,
            },

            &CMD_STOP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_STOP_FORCE
            }
        }
    },

    &OPTION_LOCK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOCK,
        &OPTION_RULE_NEGATE => true,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        }
    },

    &OPTION_ONLINE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_NEGATE => true,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_ONLINE
            }
        }
    },

    &OPTION_SET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_SET,
            }
        }
    },

    &OPTION_STANZA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_ARCHIVE_PUSH =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_BACKUP =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_CHECK =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_EXPIRE =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_INFO =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &CMD_REMOTE =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &CMD_RESTORE =>
            {
                &OPTION_RULE_REQUIRED => true
            },
            &CMD_START =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &CMD_STOP =>
            {
                &OPTION_RULE_REQUIRED => false
            }
        }
    },

    &OPTION_TARGET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_EXCLUSIVE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_ACTION =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TARGET_ACTION,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &OPTION_TARGET_TIMELINE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE =>
            {
                &OPTION_RULE_REQUIRED => false,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
                    &OPTION_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_DEFAULT  => true,
                        &RECOVERY_TYPE_NAME     => true,
                        &RECOVERY_TYPE_TIME     => true,
                        &RECOVERY_TYPE_XID      => true
                    }
                }
            }
        }
    },

    &OPTION_TYPE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &BACKUP_TYPE_FULL => true,
                    &BACKUP_TYPE_DIFF => true,
                    &BACKUP_TYPE_INCR => true,
                }
            },

            &CMD_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &RECOVERY_TYPE_NAME      => true,
                    &RECOVERY_TYPE_TIME      => true,
                    &RECOVERY_TYPE_XID       => true,
                    &RECOVERY_TYPE_PRESERVE  => true,
                    &RECOVERY_TYPE_NONE      => true,
                    &RECOVERY_TYPE_IMMEDIATE => true,
                    &RECOVERY_TYPE_DEFAULT   => true
                }
            }
        }
    },

    &OPTION_OUTPUT =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_INFO =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_INFO_OUTPUT,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &INFO_OUTPUT_TEXT => true,
                    &INFO_OUTPUT_JSON => true
                }
            }
        }
    },

    # Command-line only remote
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_COMMAND =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_REMOTE => true
        }
    },

    &OPTION_PROCESS =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_THREAD_MAX_MIN, OPTION_DEFAULT_THREAD_MAX_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_REMOTE => true
        }
    },

    # Command-line only test
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_TEST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true
        }
    },

    &OPTION_TEST_DELAY =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_FLOAT,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_DELAY,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST,
            &OPTION_RULE_DEPEND_VALUE => true
        },
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true
        }
    },

    &OPTION_TEST_POINT =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST,
            &OPTION_RULE_DEPEND_VALUE => true
        },
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true
        }
    },

    &OPTION_TEST_NO_FORK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_NO_FORK,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true
        }
    },

    # GENERAL Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_ARCHIVE_TIMEOUT =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_FLOAT,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_ARCHIVE_TIMEOUT,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_ARCHIVE_TIMEOUT_MIN, OPTION_DEFAULT_ARCHIVE_TIMEOUT_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
        },
    },

    &OPTION_BUFFER_SIZE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BUFFER_SIZE,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_BUFFER_SIZE_MIN, OPTION_DEFAULT_BUFFER_SIZE_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => false,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true
        }
    },

    &OPTION_DB_TIMEOUT =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_FLOAT,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_DB_TIMEOUT,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_DB_TIMEOUT_MIN, OPTION_DEFAULT_DB_TIMEOUT_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => false,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true
        }
    },

    &OPTION_COMPRESS =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_EXPIRE => false,
            &CMD_RESTORE => true
        }
    },

    &OPTION_COMPRESS_LEVEL =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS_LEVEL,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_COMPRESS_LEVEL_MIN, OPTION_DEFAULT_COMPRESS_LEVEL_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => false,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true
        }
    },

    &OPTION_COMPRESS_LEVEL_NETWORK =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN, OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => false,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true
        }
    },

    &OPTION_CONFIG_REMOTE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_CONFIG,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true
        },
    },

    &OPTION_NEUTRAL_UMASK =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_NEUTRAL_UMASK,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_INFO => false,
            &CMD_EXPIRE => false,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true,
            &CMD_START => false,
            &CMD_STOP => false
        }
    },

    &OPTION_LOCK_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOCK_PATH,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true,
            &CMD_EXPIRE => true
        },
    },

    &OPTION_LOG_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOG_PATH,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true,
            &CMD_EXPIRE => true
        },
    },

    &OPTION_REPO_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_REPO_PATH,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_REMOTE => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true,
            &CMD_EXPIRE => true
        },
    },

    &OPTION_SPOOL_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_SPOOL_PATH,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true
        },
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION  => OPTION_ARCHIVE_ASYNC,
            &OPTION_RULE_DEPEND_VALUE   => true
        },
    },

    &OPTION_THREAD_MAX =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_THREAD_MAX,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_THREAD_MAX_MIN, OPTION_DEFAULT_THREAD_MAX_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_RESTORE => true
        }
    },

    &OPTION_THREAD_TIMEOUT =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_RESTORE => true
        }
    },

    # COMMAND Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_COMMAND_REMOTE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMMAND_REMOTE,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true
        }
    },

    # LOG Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_LOG_LEVEL_CONSOLE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOG_LEVEL_CONSOLE,
        &OPTION_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DETAIL) => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        },
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true
        }
    },

    &OPTION_LOG_LEVEL_FILE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOG_LEVEL_FILE,
        &OPTION_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DEBUG)  => true,
            lc(DETAIL) => true,
            lc(TRACE)  => true
        },
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_EXPIRE => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true,
            &CMD_START => true,
            &CMD_STOP => true
        }
    },

    # ARCHIVE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_ARCHIVE_ASYNC =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_ARCHIVE_ASYNC,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true
        }
    },

    &OPTION_ARCHIVE_MAX_MB =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_PUSH => true
        }
    },

    # BACKUP Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_BACKUP_ARCHIVE_CHECK =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_ARCHIVE_CHECK,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP =>
            {
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION  => OPTION_ONLINE,
                    &OPTION_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &OPTION_BACKUP_ARCHIVE_COPY =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_ARCHIVE_COPY,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP =>
            {
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION  => OPTION_BACKUP_ARCHIVE_CHECK,
                    &OPTION_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &OPTION_BACKUP_HOST =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true
        },
    },

    &OPTION_BACKUP_USER =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_USER,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET => true,
            &CMD_ARCHIVE_PUSH => true,
            &CMD_CHECK => true,
            &CMD_INFO => true,
            &CMD_RESTORE => true
        },
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_BACKUP_HOST
        }
    },

    &OPTION_HARDLINK =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_HARDLINK,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    &OPTION_MANIFEST_SAVE_THRESHOLD =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_MANIFEST_SAVE_THRESHOLD,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    &OPTION_RESUME =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_RESUME,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    &OPTION_START_FAST =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_START_FAST,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    &OPTION_STOP_AUTO =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_STOP_AUTO,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    # EXPIRE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_RETENTION_ARCHIVE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_EXPIRE => true
        }
    },

    &OPTION_RETENTION_ARCHIVE_TYPE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => true,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_EXPIRE => true
        },
        &OPTION_RULE_ALLOW_LIST =>
        {
            &BACKUP_TYPE_FULL => 1,
            &BACKUP_TYPE_DIFF => 1,
            &BACKUP_TYPE_INCR => 1
        },
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_RETENTION_ARCHIVE
        }
    },

    &OPTION_RETENTION_DIFF =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_EXPIRE => true
        }
    },

    &OPTION_RETENTION_FULL =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_EXPIRE => true
        }
    },

    # RESTORE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_DB_INCLUDE =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_HASH_VALUE => false,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        },
    },

    &OPTION_LINK_ALL =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LINK_ALL,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        }
    },

    &OPTION_LINK_MAP =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        },
    },

    &OPTION_TABLESPACE_MAP_ALL =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        }
    },

    &OPTION_TABLESPACE_MAP =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        },
    },

    &OPTION_RESTORE_RECOVERY_OPTION =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_RESTORE => true
        },
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TYPE,
            &OPTION_RULE_DEPEND_LIST =>
            {
                &RECOVERY_TYPE_DEFAULT  => true,
                &RECOVERY_TYPE_NAME     => true,
                &RECOVERY_TYPE_TIME     => true,
                &RECOVERY_TYPE_XID      => true
            }
        }
    },

    # STANZA Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_DB_HOST =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true
        }
    },

    &OPTION_DB_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_HINT => "does this stanza exist?",
        &OPTION_RULE_COMMAND =>
        {
            &CMD_ARCHIVE_GET =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &CMD_ARCHIVE_PUSH =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &CMD_BACKUP => true,
            &CMD_CHECK =>
            {
                &OPTION_RULE_REQUIRED => false
            }
        },
    },

    &OPTION_DB_PORT =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_DB_PORT,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_REMOTE => true
        }
    },

    &OPTION_DB_SOCKET_PATH =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
            &CMD_REMOTE => true
        }
    },

    &OPTION_DB_USER =>
    {
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_DB_USER,
        &OPTION_RULE_COMMAND =>
        {
            &CMD_BACKUP => true,
            &CMD_CHECK => true,
        },
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_DB_HOST
        }
    }
);

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oOption;                # Option hash
my $strCommand;             # Command (backup, archive-get, ...)
my $strRemoteType;          # Remote type (DB, BACKUP, NONE)
my $oProtocol;              # Global remote object that is created on first request (NOT THREADSAFE!)

####################################################################################################################################
# configLoad
#
# Load configuration.
####################################################################################################################################
sub configLoad
{
    # Clear option in case it was loaded before
    %oOption = ();

    # Build hash with all valid command-line options
    my %oOptionAllow;

    foreach my $strKey (keys(%oOptionRule))
    {
        my $strOption = $strKey;

        if (!defined($oOptionRule{$strKey}{&OPTION_RULE_TYPE}))
        {
            confess  &log(ASSERT, "Option ${strKey} does not have a defined type", ERROR_ASSERT);
        }
        elsif ($oOptionRule{$strKey}{&OPTION_RULE_TYPE} eq OPTION_TYPE_HASH)
        {
            $strOption .= '=s@';
        }
        elsif ($oOptionRule{$strKey}{&OPTION_RULE_TYPE} ne OPTION_TYPE_BOOLEAN)
        {
            $strOption .= '=s';
        }

        $oOptionAllow{$strOption} = $strOption;

        # Check if the option can be negated
        if ((defined($oOptionRule{$strKey}{&OPTION_RULE_NEGATE}) &&
             $oOptionRule{$strKey}{&OPTION_RULE_NEGATE}) ||
            ($oOptionRule{$strKey}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN &&
             defined($oOptionRule{$strKey}{&OPTION_RULE_SECTION})))
        {
            $strOption = "no-${strKey}";
            $oOptionAllow{$strOption} = $strOption;
            $oOptionRule{$strKey}{&OPTION_RULE_NEGATE} = true;
        }
    }

    # Get command-line options
    my %oOptionTest;

    # If nothing was passed on the command line then display help
    if (@ARGV == 0)
    {
        commandSet(CMD_HELP);
    }
    # Else process command line options
    else
    {
        # Parse command line options
        if (!GetOptions(\%oOptionTest, %oOptionAllow))
        {
            commandSet(CMD_HELP);
            return false;
        }
        # Validate and store options
        else
        {
            my $bHelp = false;

            if (defined($ARGV[0]) && $ARGV[0] eq CMD_HELP && defined($ARGV[1]))
            {
                $bHelp = true;
                $ARGV[0] = $ARGV[1];
            }

            optionValid(\%oOptionTest, $bHelp);

            if ($bHelp)
            {
                commandSet(CMD_HELP);
            }
        }
    }

    # Return and display version and help in main
    if (commandTest(CMD_HELP) || commandTest(CMD_VERSION))
    {
        return true;
    }

    # Neutralize the umask to make the repository file/path modes more consistent
    if (optionGet(OPTION_NEUTRAL_UMASK))
    {
        umask(0000);
    }

    # Check if the backup host is remote
    if (optionTest(OPTION_BACKUP_HOST))
    {
        $strRemoteType = BACKUP;
    }
    # Else check if db is remote
    elsif (optionTest(OPTION_DB_HOST))
    {
        # Don't allow both sides to be remote
        if (defined($strRemoteType))
        {
            confess &log(ERROR, 'db and backup cannot both be configured as remote', ERROR_CONFIG);
        }

        $strRemoteType = DB;
    }
    else
    {
        $strRemoteType = NONE;
    }

    return true;
}

push @EXPORT, qw(configLoad);

####################################################################################################################################
# optionValid
#
# Make sure the command-line options are valid based on the command.
####################################################################################################################################
sub optionValid
{
    my $oOptionTest = shift;
    my $bHelp = shift;

    # Check that the command is present and valid
    $strCommand = $ARGV[0];

    if (!defined($strCommand))
    {
        confess &log(ERROR, "command must be specified", ERROR_COMMAND_REQUIRED);
    }

    if (!defined($oCommandHash{$strCommand}))
    {
        confess &log(ERROR, "invalid command ${strCommand}", ERROR_COMMAND_INVALID);
    }

    # Hash to store contents of the config file.  The file will be loaded once the config dependency is resolved unless all options
    # are set on the command line or --no-config is specified.
    my $oConfig;
    my $bConfigExists = true;

    # Keep track of unresolved dependencies
    my $bDependUnresolved = true;
    my %oOptionResolved;

    # Loop through all possible options
    while ($bDependUnresolved)
    {
        # Assume that all dependencies will be resolved in this loop
        $bDependUnresolved = false;

        foreach my $strOption (sort(keys(%oOptionRule)))
        {
            # Skip the option if it has been resolved in a prior loop
            if (defined($oOptionResolved{$strOption}))
            {
                next;
            }

            # Store the option value since it is used a lot
            my $strValue = $$oOptionTest{$strOption};

            # Check to see if an option can be negated.  Make sure that it is not set and negated at the same time.
            my $bNegate = false;

            if (defined($oOptionRule{$strOption}{&OPTION_RULE_NEGATE}) && $oOptionRule{$strOption}{&OPTION_RULE_NEGATE})
            {
                $bNegate = defined($$oOptionTest{'no-' . $strOption});

                if ($bNegate && defined($strValue))
                {
                    confess &log(ERROR, "option '${strOption}' cannot be both set and negated", ERROR_OPTION_NEGATE);
                }

                if ($bNegate && $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
                {
                    $strValue = false;
                }
            }

            # If the command has rules store them for later evaluation
            my $oCommandRule = optionCommandRule($strOption, $strCommand);

            # Check dependency for the command then for the option
            my $bDependResolved = true;
            my $oDepend = defined($oCommandRule) ? $$oCommandRule{&OPTION_RULE_DEPEND} :
                                                   $oOptionRule{$strOption}{&OPTION_RULE_DEPEND};
            my $strDependOption;
            my $strDependValue;
            my $strDependType;

            if (defined($oDepend))
            {
                # Check if the depend option has a value
                $strDependOption = $$oDepend{&OPTION_RULE_DEPEND_OPTION};
                $strDependValue = $oOption{$strDependOption}{value};

                # Make sure the depend option has been resolved, otherwise skip this option for now
                if (!defined($oOptionResolved{$strDependOption}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                if (!defined($strDependValue))
                {
                    $bDependResolved = false;
                    $strDependType = 'source';
                }

                # If a depend value exists, make sure the option value matches
                if ($bDependResolved && defined($$oDepend{&OPTION_RULE_DEPEND_VALUE}) &&
                    $$oDepend{&OPTION_RULE_DEPEND_VALUE} ne $strDependValue)
                {
                    $bDependResolved = false;
                    $strDependType = 'value';
                }

                # If a depend list exists, make sure the value is in the list
                if ($bDependResolved && defined($$oDepend{&OPTION_RULE_DEPEND_LIST}) &&
                    !defined($$oDepend{&OPTION_RULE_DEPEND_LIST}{$strDependValue}))
                {
                    $bDependResolved = false;
                    $strDependType = 'list';
                }
            }

            # If the option value is undefined and not negated, see if it can be loaded from the config file
            if (!defined($strValue) && !$bNegate && $strOption ne OPTION_CONFIG &&
                $oOptionRule{$strOption}{&OPTION_RULE_SECTION} && $bDependResolved)
            {
                # If the config option has not been resolved yet then continue processing
                if (!defined($oOptionResolved{&OPTION_CONFIG}) || !defined($oOptionResolved{&OPTION_STANZA}))
                {
                    $bDependUnresolved = true;
                    next;
                }

                # If the config option is defined try to get the option from the config file
                if ($bConfigExists && defined($oOption{&OPTION_CONFIG}{value}))
                {
                    # Attempt to load the config file if it has not been loaded
                    if (!defined($oConfig))
                    {
                        my $strConfigFile = $oOption{&OPTION_CONFIG}{value};
                        $bConfigExists = -e $strConfigFile;

                        if ($bConfigExists)
                        {
                            if (!-f $strConfigFile)
                            {
                                confess &log(ERROR, "'${strConfigFile}' is not a file", ERROR_FILE_INVALID);
                            }

                            $oConfig = iniLoad($strConfigFile, undef, true);
                        }
                    }

                    # Get the section that the value should be in
                    my $strSection = $oOptionRule{$strOption}{&OPTION_RULE_SECTION};

                    # Always search the stanza section when it exists
                    if (optionTest(OPTION_STANZA))
                    {
                        $strValue = $$oConfig{optionGet(OPTION_STANZA)}{$strOption};
                    }

                    # Only continue searching when strSection != CONFIG_SECTION_STANZA.  Some options (e.g. db-path) can only be
                    # configured in the stanza section.
                    if (!defined($strValue) && $strSection ne CONFIG_SECTION_STANZA)
                    {
                        # Check command sections for a value.  This allows options (e.g. compress-level) to be set on a per-command
                        # basis.
                        if (defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand}) &&
                            $oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand} != false)
                        {
                            # Check the stanza command section
                            if (optionTest(OPTION_STANZA))
                            {
                                $strValue = $$oConfig{optionGet(OPTION_STANZA) . ":${strCommand}"}{$strOption};
                            }

                            # Check the global command section
                            if (!defined($strValue))
                            {
                                $strValue = $$oConfig{&CONFIG_SECTION_GLOBAL . ":${strCommand}"}{$strOption};
                            }
                        }

                        # Finally check the global section
                        if (!defined($strValue))
                        {
                            $strValue = $$oConfig{&CONFIG_SECTION_GLOBAL}{$strOption};
                        }
                    }

                    # Fix up data types
                    if (defined($strValue))
                    {
                        # The empty string is undefined
                        if ($strValue eq '')
                        {
                            $strValue = undef;
                        }
                        # Convert Y or N to boolean
                        elsif ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
                        {
                            if ($strValue eq 'y')
                            {
                                $strValue = true;
                            }
                            elsif ($strValue eq 'n')
                            {
                                $strValue = false;
                            }
                            else
                            {
                                confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option",
                                             ERROR_OPTION_INVALID_VALUE);
                            }
                        }
                        # Convert a list of key/value pairs to a hash
                        elsif ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_HASH)
                        {
                            my @oValue = ();

                            # If there is only one key/value
                            if (ref(\$strValue) eq 'SCALAR')
                            {
                                push(@oValue, $strValue);
                            }
                            # Else if there is an array of values
                            else
                            {
                                @oValue = @{$strValue};
                            }

                            # Reset the value hash
                            $strValue = {};

                            # Iterate and parse each key/value pair
                            foreach my $strHash (@oValue)
                            {
                                my $iEqualIdx = index($strHash, '=');

                                if ($iEqualIdx < 1 || $iEqualIdx == length($strHash) - 1)
                                {
                                    confess &log(ERROR, "'${strHash}' is not valid for '${strOption}' option",
                                                 ERROR_OPTION_INVALID_VALUE);
                                }

                                my $strHashKey = substr($strHash, 0, $iEqualIdx);
                                my $strHashValue = substr($strHash, length($strHashKey) + 1);

                                $$strValue{$strHashKey} = $strHashValue;
                            }
                        }

                        $oOption{$strOption}{source} = SOURCE_CONFIG;
                    }
                }
            }

            if (defined($oDepend) && !$bDependResolved && defined($strValue))
            {
                my $strError = "option '${strOption}' not valid without option ";

                if ($strDependType eq 'source')
                {
                    confess &log(ERROR, "${strError}'${strDependOption}'", ERROR_OPTION_INVALID);
                }

                # If a depend value exists, make sure the option value matches
                if ($strDependType eq 'value')
                {
                    if ($oOptionRule{$strDependOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
                    {
                        $strError .= "'" . ($$oDepend{&OPTION_RULE_DEPEND_VALUE} ? '' : 'no-') . "${strDependOption}'";
                    }
                    else
                    {
                        $strError .= "'${strDependOption}' = '$$oDepend{&OPTION_RULE_DEPEND_VALUE}'";
                    }

                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                $strError .= "'${strDependOption}'";

                # If a depend list exists, make sure the value is in the list
                if ($strDependType eq 'list')
                {
                    my @oyValue;

                    foreach my $strValue (sort(keys(%{$$oDepend{&OPTION_RULE_DEPEND_LIST}})))
                    {
                        push(@oyValue, "'${strValue}'");
                    }

                    $strError .= @oyValue == 1 ? " = $oyValue[0]" : " in (" . join(", ", @oyValue) . ")";
                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }
            }

            # Is the option defined?
            if (defined($strValue))
            {
                # Check that floats and integers are valid
                if ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_INTEGER ||
                    $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_FLOAT)
                {
                    # Test that the string is a valid float or integer  by adding 1 to it.  It's pretty hokey but it works and it
                    # beats requiring Scalar::Util::Numeric to do it properly.
                    eval
                    {
                        my $strTest = $strValue + 1;
                    };

                    my $bError = $@ ? true : false;

                    # Check that integers are really integers
                    if (!$bError && $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_INTEGER &&
                        (int($strValue) . 'S') ne ($strValue . 'S'))
                    {
                        $bError = true;
                    }

                    # Error if the value did not pass tests
                    !$bError
                        or confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow list for the command then for the option
                my $oAllow = defined($oCommandRule) ? $$oCommandRule{&OPTION_RULE_ALLOW_LIST} :
                                                      $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_LIST};

                if (defined($oAllow) && !defined($$oAllow{$strValue}))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow range for the command then for the option
                $oAllow = defined($oCommandRule) ? $$oCommandRule{&OPTION_RULE_ALLOW_RANGE} :
                                                     $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_RANGE};

                if (defined($oAllow) && ($strValue < $$oAllow[0] || $strValue > $$oAllow[1]))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_RANGE);
                }

                # Set option value
                if ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_HASH && ref($strValue) eq 'ARRAY')
                {
                    foreach my $strItem (@{$strValue})
                    {
                        my $strKey;
                        my $strValue;

                        # If the keys are expected to have values
                        if (!defined($oOptionRule{$strOption}{&OPTION_RULE_HASH_VALUE}) ||
                            $oOptionRule{$strOption}{&OPTION_RULE_HASH_VALUE})
                        {
                            # Check for = and make sure there is a least one character on each side
                            my $iEqualPos = index($strItem, '=');

                            if ($iEqualPos < 1 || length($strItem) <= $iEqualPos + 1)
                            {
                                confess &log(ERROR, "'${strItem}' not valid key/value for '${strOption}' option",
                                                    ERROR_OPTION_INVALID_PAIR);
                            }

                            $strKey = substr($strItem, 0, $iEqualPos);
                            $strValue = substr($strItem, $iEqualPos + 1);
                        }
                        # Else no values are expected so set value to true
                        else
                        {
                            $strKey = $strItem;
                            $strValue = true;
                        }

                        # Check that the key has not already been set
                        if (defined($oOption{$strOption}{$strKey}{value}))
                        {
                            confess &log(ERROR, "'${$strItem}' already defined for '${strOption}' option",
                                                ERROR_OPTION_DUPLICATE_KEY);
                        }

                        # Set key/value
                        $oOption{$strOption}{value}{$strKey} = $strValue;
                    }
                }
                else
                {
                    $oOption{$strOption}{value} = $strValue;
                }

                # If not config sourced then it must be a param
                if (!defined($oOption{$strOption}{source}))
                {
                    $oOption{$strOption}{source} = SOURCE_PARAM;
                }
            }
            # Else try to set a default
            elsif ($bDependResolved &&
                   (!defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}) ||
                    defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand})))
            {
                # Source is default for this option
                $oOption{$strOption}{source} = SOURCE_DEFAULT;

                # Check for default in command then option
                my $strDefault = optionDefault($strOption, $strCommand);

                # If default is defined
                if (defined($strDefault))
                {
                    # Only set default if dependency is resolved
                    $oOption{$strOption}{value} = $strDefault if !$bNegate;
                }
                # Else check required
                elsif (optionRequired($strOption, $strCommand) && !$bHelp)
                {
                    confess &log(ERROR, "${strCommand} command requires option: ${strOption}" .
                                        (defined($oOptionRule{$strOption}{&OPTION_RULE_HINT}) ?
                                         "\nHINT: " . $oOptionRule{$strOption}{&OPTION_RULE_HINT} : ''),
                                        ERROR_OPTION_REQUIRED);
                }
            }

            $oOptionResolved{$strOption} = true;
        }
    }
}

####################################################################################################################################
# optionCommandRule
#
# Returns the option rules based on the command.
####################################################################################################################################
sub optionCommandRule
{
    my $strOption = shift;
    my $strCommand = shift;

    if (defined($strCommand))
    {
        return defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}) &&
               defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand}) &&
               ref($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand}) eq 'HASH' ?
               $oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strCommand} : undef;
    }

    return;
}

####################################################################################################################################
# optionRequired
#
# Is the option required for this command?
####################################################################################################################################
sub optionRequired
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = optionCommandRule($strOption, $strCommand);

    # Check for required in command then option
    my $bRequired = defined($oCommandRule) ? $$oCommandRule{&OPTION_RULE_REQUIRED} :
                                             $oOptionRule{$strOption}{&OPTION_RULE_REQUIRED};

    # Return required
    return !defined($bRequired) || $bRequired;
}

push @EXPORT, qw(optionRequired);

####################################################################################################################################
# optionType
#
# Get the option type.
####################################################################################################################################
sub optionType
{
    my $strOption = shift;

    return $oOptionRule{$strOption}{&OPTION_RULE_TYPE};
}

push @EXPORT, qw(optionType);

####################################################################################################################################
# optionTypeTest
#
# Test the option type.
####################################################################################################################################
sub optionTypeTest
{
    my $strOption = shift;
    my $strType = shift;

    return optionType($strOption) eq $strType;
}

push @EXPORT, qw(optionTypeTest);

####################################################################################################################################
# optionDefault
#
# Does the option have a default for this command?
####################################################################################################################################
sub optionDefault
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = optionCommandRule($strOption, $strCommand);

    # Check for default in command
    my $strDefault = defined($oCommandRule) ? $$oCommandRule{&OPTION_RULE_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $oOptionRule{$strOption}{&OPTION_RULE_DEFAULT};
}

push @EXPORT, qw(optionDefault);

####################################################################################################################################
# optionRange
#
# Gets the allowed setting range for the option if it exists.
####################################################################################################################################
sub optionRange
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = optionCommandRule($strOption, $strCommand);

    # Check for default in command
    if (defined($oCommandRule) && defined($$oCommandRule{&OPTION_RULE_ALLOW_RANGE}))
    {
        return $$oCommandRule{&OPTION_RULE_ALLOW_RANGE}[0], $$oCommandRule{&OPTION_RULE_ALLOW_RANGE}[1];
    }

    # If defined return, else try to grab the global default
    return $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_RANGE}[0], $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_RANGE}[1];
}

push @EXPORT, qw(optionRange);

####################################################################################################################################
# optionSource
#
# How was the option set?
####################################################################################################################################
sub optionSource
{
    my $strOption = shift;

    return $oOption{$strOption}{source};
}

push @EXPORT, qw(optionSource);

####################################################################################################################################
# optionGet
#
# Get option value.
####################################################################################################################################
sub optionGet
{
    my $strOption = shift;
    my $bRequired = shift;

    if (!defined($oOption{$strOption}{value}) && (!defined($bRequired) || $bRequired))
    {
        confess &log(ASSERT, "option ${strOption} is required");
    }

    return $oOption{$strOption}{value};
}

push @EXPORT, qw(optionGet);

####################################################################################################################################
# optionSet
#
# Set option value.
####################################################################################################################################
sub optionSet
{
    my $strOption = shift;
    my $oValue = shift;

    if (!defined($oOption{$strOption}{value}))
    {
        confess &log(ASSERT, "option ${strOption} is not defined");
    }

    $oOption{$strOption}{value} = $oValue;
}

push @EXPORT, qw(optionSet);

####################################################################################################################################
# optionTest
#
# Test a option value.
####################################################################################################################################
sub optionTest
{
    my $strOption = shift;
    my $strValue = shift;

    if (defined($strValue))
    {
        return optionGet($strOption) eq $strValue;
    }

    return defined($oOption{$strOption}{value});
}

push @EXPORT, qw(optionTest);

####################################################################################################################################
# optionRuleGet
#
# Get the option rules.
####################################################################################################################################
sub optionRuleGet
{
    use Storable qw(dclone);
    return dclone(\%oOptionRule);
}

push @EXPORT, qw(optionRuleGet);

####################################################################################################################################
# optionRemoteType
#
# Returns the remote type.
####################################################################################################################################
sub optionRemoteType
{
    return $strRemoteType;
}

push @EXPORT, qw(optionRemoteType);

####################################################################################################################################
# optionRemoteTypeTest
#
# Test the remote type.
####################################################################################################################################
sub optionRemoteTypeTest
{
    my $strTest = shift;

    return $strRemoteType eq $strTest ? true : false;
}

push @EXPORT, qw(optionRemoteTypeTest);

####################################################################################################################################
# optionRemoteTest
#
# Test if the remote DB or BACKUP.
####################################################################################################################################
sub optionRemoteTest
{
    return $strRemoteType ne NONE ? true : false;
}

push @EXPORT, qw(optionRemoteTest);

####################################################################################################################################
# protocolGet
#
# Get the protocol object or create it if does not exist.  Shared protocol objects are used because they create an SSH connection
# to the remote host and the number of these connections should be minimized.  A protocol object can be shared within a single
# thread - for new threads clone() should be called on the shared protocol object.
####################################################################################################################################
sub protocolGet
{
    my $bForceLocal = shift;
    my $bStore = shift;
    my $iProcessIdx = shift;

    # If force local or remote = NONE then create a local remote and return it
    if ((defined($bForceLocal) && $bForceLocal) || optionRemoteTypeTest(NONE))
    {
        return new pgBackRest::Protocol::Common
        (
            optionGet(OPTION_BUFFER_SIZE),
            commandTest(CMD_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL : optionGet(OPTION_COMPRESS_LEVEL),
            commandTest(CMD_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK : optionGet(OPTION_COMPRESS_LEVEL_NETWORK),
            protocolTimeoutGet()
        );
    }

    # Return the remote if is already defined
    if (defined($oProtocol))
    {
        return $oProtocol;
    }

    # Return the remote when required
    my $oProtocolTemp = new pgBackRest::Protocol::RemoteMaster
    (
        commandWrite(CMD_REMOTE, true, optionGet(OPTION_COMMAND_REMOTE), undef,
                     {&OPTION_COMMAND => {value => commandGet()}, &OPTION_PROCESS => {value => $iProcessIdx}, &OPTION_CONFIG =>
                     {value => optionSource(OPTION_CONFIG_REMOTE) eq SOURCE_DEFAULT ? undef : optionGet(OPTION_CONFIG_REMOTE)},
                     &OPTION_LOG_PATH => {}, &OPTION_LOCK_PATH => {}}),
        optionGet(OPTION_BUFFER_SIZE),
        commandTest(CMD_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL : optionGet(OPTION_COMPRESS_LEVEL),
        commandTest(CMD_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK : optionGet(OPTION_COMPRESS_LEVEL_NETWORK),
        optionRemoteTypeTest(DB) ? optionGet(OPTION_DB_HOST) : optionGet(OPTION_BACKUP_HOST),
        optionRemoteTypeTest(DB) ? optionGet(OPTION_DB_USER) : optionGet(OPTION_BACKUP_USER),
        protocolTimeoutGet()
    );

    if (!defined($bStore) || $bStore)
    {
        $oProtocol = $oProtocolTemp;
    }

    return $oProtocolTemp;
}

push @EXPORT, qw(protocolGet);

####################################################################################################################################
# protocolTimeoutGet
#
# Get the protocol time - for the moment this is based on the db timeout + 30 seconds.
####################################################################################################################################
sub protocolTimeoutGet
{
    return optionGet(OPTION_DB_TIMEOUT) >= OPTION_DEFAULT_DB_TIMEOUT ?
               optionGet(OPTION_DB_TIMEOUT) + 30 : optionGet(OPTION_DB_TIMEOUT);
}

push @EXPORT, qw(protocolTimeoutGet);

####################################################################################################################################
# protocolDestroy
#
# Undefine the protocol if it is stored locally.
####################################################################################################################################
sub protocolDestroy
{
    if (defined($oProtocol))
    {
        $oProtocol->close();
        undef($oProtocol);
    }
}

push @EXPORT, qw(protocolDestroy);

####################################################################################################################################
# commandGet
#
# Get the current command.
####################################################################################################################################
sub commandGet
{
    return $strCommand;
}

push @EXPORT, qw(commandGet);

####################################################################################################################################
# commandTest
#
# Test the current command.
####################################################################################################################################
sub commandTest
{
    my $strCommandTest = shift;

    return $strCommandTest eq $strCommand;
}

push @EXPORT, qw(commandTest);

####################################################################################################################################
# commandStart
#
# Log information about the command that was just started.
####################################################################################################################################
sub commandStart
{
    &log($strCommand eq CMD_INFO ? DEBUG : INFO, "${strCommand} start:" . commandWrite($strCommand, true, '', false));
}

push @EXPORT, qw(commandStart);

####################################################################################################################################
# commandStop
#
# Log information about the command that was just stopped.
####################################################################################################################################
sub commandStop
{
    if (defined($strCommand))
    {
        &log($strCommand eq CMD_INFO ? DEBUG : INFO, "${strCommand} stop");
    }
}

push @EXPORT, qw(commandStop);

####################################################################################################################################
# commandSet
#
# Set current command (usually for triggering follow-on commands).
####################################################################################################################################
sub commandSet
{
    my $strValue = shift;

    commandStop();

    $strCommand = $strValue;

    commandStart();
}

push @EXPORT, qw(commandSet);

####################################################################################################################################
# commandWrite
#
# Using the options that were passed to the current command, write the command string for another command.  For example, this
# can be used to write the archive-get command for recovery.conf during a restore.
####################################################################################################################################
sub commandWrite
{
    my $strNewCommand = shift;
    my $bIncludeConfig = shift;
    my $strExeString = shift;
    my $bIncludeCommand = shift;
    my $oOptionOverride = shift;

    # Set defaults
    $strExeString = defined($strExeString) ? $strExeString : abs_path($0);
    $bIncludeConfig = defined($bIncludeConfig) ? $bIncludeConfig : false;
    $bIncludeCommand = defined($bIncludeCommand) ? $bIncludeCommand : true;

    # if ($bIncludeConfig && $strExeString ne '')
    # {
    #     $strExeString .= ' --no-config';
    # }

    # Iterate the options to figure out which ones are not default and need to be written out to the new command string
    foreach my $strOption (sort(keys(%oOptionRule)))
    {
        # Skip the config option if it's already included
        # next if ($bIncludeConfig && $strOption eq OPTION_CONFIG);

        # Process any option overrides first
        if (defined($$oOptionOverride{$strOption}))
        {
            if (defined($$oOptionOverride{$strOption}{value}))
            {
                $strExeString .= commandWriteOptionFormat($strOption, false, {value => $$oOptionOverride{$strOption}{value}});
            }
        }
        # else look for non-default options in the current configuration
        elsif ((!defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}) ||
                defined($oOptionRule{$strOption}{&OPTION_RULE_COMMAND}{$strNewCommand})) &&
               defined($oOption{$strOption}{value}) &&
               ($bIncludeConfig ? $oOption{$strOption}{source} ne SOURCE_DEFAULT : $oOption{$strOption}{source} eq SOURCE_PARAM))
        {
            my $oValue;
            my $bMulti = false;

            # If this is a hash then it will break up into multple command-line options
            if (ref($oOption{$strOption}{value}) eq 'HASH')
            {
                $oValue = $oOption{$strOption}{value};
                $bMulti = true;
            }
            # Else a single value but store it in a hash anyway to make processing below simpler
            else
            {
                $oValue = {value => $oOption{$strOption}{value}};
            }

            $strExeString .= commandWriteOptionFormat($strOption, $bMulti, $oValue);
        }
    }

    if ($bIncludeCommand)
    {
        $strExeString .= " ${strNewCommand}";
    }

    return $strExeString;
}

push @EXPORT, qw(commandWrite);

# Helper function for commandWrite() to correctly format options for command-line usage
sub commandWriteOptionFormat
{
    my $strOption = shift;
    my $bMulti = shift;
    my $oValue = shift;

    # Loops though all keys in the hash
    my $strOptionFormat = '';
    my $strParam;

    foreach my $strKey (sort(keys(%$oValue)))
    {
        # Get the value - if the original value was a hash then the key must be prefixed
        my $strValue = ($bMulti ?  "${strKey}=" : '') . $$oValue{$strKey};

        # Handle the no- prefix for boolean values
        if ($oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
        {
            $strParam = '--' . ($strValue ? '' : 'no-') . $strOption;
        }
        else
        {
            $strParam = "--${strOption}=${strValue}";
        }

        # Add quotes if the value has spaces in it
        $strOptionFormat .= ' ' . (index($strValue, " ") != -1 ? "\"${strParam}\"" : $strParam);
    }

    return $strOptionFormat;
}

####################################################################################################################################
# commandHashGet
#
# Get the hash that contains all valid commands.
####################################################################################################################################
sub commandHashGet
{
    require Storable;
    Storable->import();

    return dclone(\%oCommandHash);
}

push @EXPORT, qw(commandHashGet);

1;
