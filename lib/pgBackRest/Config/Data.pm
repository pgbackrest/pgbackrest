####################################################################################################################################
# Configuration Rule Data
#
# Contains the rules for options: which commands the option can/cannot be specified, for which commands it is required, default
# settings, types, ranges, whether the option is negatable, whether it has dependencies, etc. The initial section is the global
# section meaning the rules defined there apply to all commands listed for the option.
#
# CFGBLDDEF_RULE_COMMAND:
#     List of commands the option can be used with this option.  An empty hash signifies that the command does no deviate from the
#     option defaults.  Otherwise, overrides can be specified.
#
# NOTE: If the option (A) has a dependency on another option (B) then the CFGCMD_ must also be specified in the other option
#         (B), else it will still error on the option (A).
#
# CFGBLDDEF_RULE_REQUIRED:
#   In global section:
#       true - if the option does not have a default, then setting CFGBLDDEF_RULE_REQUIRED in the global section means all commands
#              listed in CFGBLDDEF_RULE_COMMAND require the user to set it.
#       false - no commands listed require it as an option but it can be set. This can be overridden for individual commands by
#               setting CFGBLDDEF_RULE_REQUIRED in the CFGBLDDEF_RULE_COMMAND section.
#   In CFGBLDDEF_RULE_COMMAND section:
#       true - the option must be set somehow for the command, either by default (CFGBLDDEF_RULE_DEFAULT) or by the user.
# 	        &CFGCMD_CHECK =>
#             {
#                 &CFGBLDDEF_RULE_REQUIRED => true
#             },
#       false - mainly used for overriding the CFGBLDDEF_RULE_REQUIRED in the global section.
#
# CFGBLDDEF_RULE_DEFAULT:
#   Sets a default for the option for all commands if listed in the global section, or for specific commands if listed in the
#   CFGBLDDEF_RULE_COMMAND section.
#
# CFGBLDDEF_RULE_NEGATE:
#   The option can be negated with "no" e.g. --no-compress.  This applies tp options that are only valid on the command line (i.e.
#   no config section defined).  All config options are automatically negatable.
#
# CFGBLDDEF_RULE_DEPEND:
#   Specify the dependencies this option has on another option. All commands listed for this option must also be listed in the
#   dependent option(s).
#   CFGBLDDEF_RULE_DEPEND_LIST further defines the allowable settings for the depended option.
#
# CFGBLDDEF_RULE_ALLOW_LIST:
#   Lists the allowable settings for the option.
####################################################################################################################################
package pgBackRest::Config::Data;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Cwd qw(abs_path);
use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname basename);
use Getopt::Long qw(GetOptions);
use Storable qw(dclone);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Io::Base;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;
use pgBackRest::Version;

####################################################################################################################################
# Command constants - commands that are allowed in pgBackRest
####################################################################################################################################
use constant CFGCMD_ARCHIVE_GET                                     => 'archive-get';
    push @EXPORT, qw(CFGCMD_ARCHIVE_GET);
use constant CFGCMD_ARCHIVE_PUSH                                    => 'archive-push';
    push @EXPORT, qw(CFGCMD_ARCHIVE_PUSH);
use constant CFGCMD_BACKUP                                          => 'backup';
    push @EXPORT, qw(CFGCMD_BACKUP);
use constant CFGCMD_CHECK                                           => 'check';
    push @EXPORT, qw(CFGCMD_CHECK);
use constant CFGCMD_EXPIRE                                          => 'expire';
    push @EXPORT, qw(CFGCMD_EXPIRE);
use constant CFGCMD_HELP                                            => 'help';
    push @EXPORT, qw(CFGCMD_HELP);
use constant CFGCMD_INFO                                            => 'info';
    push @EXPORT, qw(CFGCMD_INFO);
use constant CFGCMD_LOCAL                                           => 'local';
    push @EXPORT, qw(CFGCMD_LOCAL);
use constant CFGCMD_REMOTE                                          => 'remote';
    push @EXPORT, qw(CFGCMD_REMOTE);
use constant CFGCMD_RESTORE                                         => 'restore';
    push @EXPORT, qw(CFGCMD_RESTORE);
use constant CFGCMD_STANZA_CREATE                                   => 'stanza-create';
    push @EXPORT, qw(CFGCMD_STANZA_CREATE);
use constant CFGCMD_STANZA_UPGRADE                                  => 'stanza-upgrade';
    push @EXPORT, qw(CFGCMD_STANZA_UPGRADE);
use constant CFGCMD_START                                           => 'start';
    push @EXPORT, qw(CFGCMD_START);
use constant CFGCMD_STOP                                            => 'stop';
    push @EXPORT, qw(CFGCMD_STOP);
use constant CFGCMD_VERSION                                         => 'version';
    push @EXPORT, qw(CFGCMD_VERSION);

####################################################################################################################################
# Option constants - options that are allowed for commands
####################################################################################################################################

# Command-line only options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_CONFIG                                          => 'config';
    push @EXPORT, qw(CFGOPT_CONFIG);
use constant CFGOPT_DELTA                                           => 'delta';
    push @EXPORT, qw(CFGOPT_DELTA);
use constant CFGOPT_FORCE                                           => 'force';
    push @EXPORT, qw(CFGOPT_FORCE);
use constant CFGOPT_ONLINE                                          => 'online';
    push @EXPORT, qw(CFGOPT_ONLINE);
use constant CFGOPT_SET                                             => 'set';
    push @EXPORT, qw(CFGOPT_SET);
use constant CFGOPT_STANZA                                          => 'stanza';
    push @EXPORT, qw(CFGOPT_STANZA);
use constant CFGOPT_TARGET                                          => 'target';
    push @EXPORT, qw(CFGOPT_TARGET);
use constant CFGOPT_TARGET_EXCLUSIVE                                => 'target-exclusive';
    push @EXPORT, qw(CFGOPT_TARGET_EXCLUSIVE);
use constant CFGOPT_TARGET_ACTION                                   => 'target-action';
    push @EXPORT, qw(CFGOPT_TARGET_ACTION);
use constant CFGOPT_TARGET_TIMELINE                                 => 'target-timeline';
    push @EXPORT, qw(CFGOPT_TARGET_TIMELINE);
use constant CFGOPT_TYPE                                            => 'type';
    push @EXPORT, qw(CFGOPT_TYPE);
use constant CFGOPT_OUTPUT                                          => 'output';
    push @EXPORT, qw(CFGOPT_OUTPUT);

# Command-line only local/remote optiosn
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_COMMAND                                         => 'command';
    push @EXPORT, qw(CFGOPT_COMMAND);
use constant CFGOPT_PROCESS                                         => 'process';
    push @EXPORT, qw(CFGOPT_PROCESS);
use constant CFGOPT_HOST_ID                                         => 'host-id';
    push @EXPORT, qw(CFGOPT_HOST_ID);

# Command-line only test options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_TEST                                            => 'test';
    push @EXPORT, qw(CFGOPT_TEST);
use constant CFGOPT_TEST_DELAY                                      => 'test-delay';
    push @EXPORT, qw(CFGOPT_TEST_DELAY);
use constant CFGOPT_TEST_POINT                                      => 'test-point';
    push @EXPORT, qw(CFGOPT_TEST_POINT);

# General options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_TIMEOUT                                 => 'archive-timeout';
    push @EXPORT, qw(CFGOPT_ARCHIVE_TIMEOUT);
use constant CFGOPT_BUFFER_SIZE                                     => 'buffer-size';
    push @EXPORT, qw(CFGOPT_BUFFER_SIZE);
use constant CFGOPT_DB_TIMEOUT                                      => 'db-timeout';
    push @EXPORT, qw(CFGOPT_DB_TIMEOUT);
use constant CFGOPT_COMPRESS                                        => 'compress';
    push @EXPORT, qw(CFGOPT_COMPRESS);
use constant CFGOPT_COMPRESS_LEVEL                                  => 'compress-level';
    push @EXPORT, qw(CFGOPT_COMPRESS_LEVEL);
use constant CFGOPT_COMPRESS_LEVEL_NETWORK                          => 'compress-level-network';
    push @EXPORT, qw(CFGOPT_COMPRESS_LEVEL_NETWORK);
use constant CFGOPT_NEUTRAL_UMASK                                   => 'neutral-umask';
    push @EXPORT, qw(CFGOPT_NEUTRAL_UMASK);
use constant CFGOPT_PROTOCOL_TIMEOUT                                => 'protocol-timeout';
    push @EXPORT, qw(CFGOPT_PROTOCOL_TIMEOUT);
use constant CFGOPT_PROCESS_MAX                                     => 'process-max';
    push @EXPORT, qw(CFGOPT_PROCESS_MAX);

# Commands
use constant CFGOPT_CMD_SSH                                         => 'cmd-ssh';
    push @EXPORT, qw(CFGOPT_CMD_SSH);

# Paths
use constant CFGOPT_LOCK_PATH                                       => 'lock-path';
    push @EXPORT, qw(CFGOPT_LOCK_PATH);
use constant CFGOPT_LOG_PATH                                        => 'log-path';
    push @EXPORT, qw(CFGOPT_LOG_PATH);
use constant CFGOPT_SPOOL_PATH                                      => 'spool-path';
    push @EXPORT, qw(CFGOPT_SPOOL_PATH);

# Repository
use constant CFGOPT_REPO_PATH                                       => 'repo-path';
    push @EXPORT, qw(CFGOPT_REPO_PATH);
use constant CFGOPT_REPO_TYPE                                       => 'repo-type';
    push @EXPORT, qw(CFGOPT_REPO_TYPE);

# Repository S3
use constant CFGOPT_REPO_S3_KEY                                     => 'repo-s3-key';
    push @EXPORT, qw(CFGOPT_REPO_S3_KEY);
use constant CFGOPT_REPO_S3_KEY_SECRET                              => 'repo-s3-key-secret';
    push @EXPORT, qw(CFGOPT_REPO_S3_KEY_SECRET);
use constant CFGOPT_REPO_S3_BUCKET                                  => 'repo-s3-bucket';
    push @EXPORT, qw(CFGOPT_REPO_S3_BUCKET);
use constant CFGOPT_REPO_S3_CA_FILE                                 => 'repo-s3-ca-file';
    push @EXPORT, qw(CFGOPT_REPO_S3_CA_FILE);
use constant CFGOPT_REPO_S3_CA_PATH                                 => 'repo-s3-ca-path';
    push @EXPORT, qw(CFGOPT_REPO_S3_CA_PATH);
use constant CFGOPT_REPO_S3_ENDPOINT                                => 'repo-s3-endpoint';
    push @EXPORT, qw(CFGOPT_REPO_S3_ENDPOINT);
use constant CFGOPT_REPO_S3_HOST                                    => 'repo-s3-host';
    push @EXPORT, qw(CFGOPT_REPO_S3_HOST);
use constant CFGOPT_REPO_S3_REGION                                  => 'repo-s3-region';
    push @EXPORT, qw(CFGOPT_REPO_S3_REGION);
use constant CFGOPT_REPO_S3_VERIFY_SSL                              => 'repo-s3-verify-ssl';
    push @EXPORT, qw(CFGOPT_REPO_S3_VERIFY_SSL);

# Logging
use constant CFGOPT_LOG_LEVEL_CONSOLE                               => 'log-level-console';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_CONSOLE);
use constant CFGOPT_LOG_LEVEL_FILE                                  => 'log-level-file';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_FILE);
use constant CFGOPT_LOG_LEVEL_STDERR                                => 'log-level-stderr';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_STDERR);
use constant CFGOPT_LOG_TIMESTAMP                                   => 'log-timestamp';
    push @EXPORT, qw(CFGOPT_LOG_TIMESTAMP);

# Archive options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_ASYNC                                   => 'archive-async';
    push @EXPORT, qw(CFGOPT_ARCHIVE_ASYNC);
# Deprecated and to be removed
use constant CFGOPT_ARCHIVE_MAX_MB                                  => 'archive-max-mb';
    push @EXPORT, qw(CFGOPT_ARCHIVE_MAX_MB);
use constant CFGOPT_ARCHIVE_QUEUE_MAX                               => 'archive-queue-max';
    push @EXPORT, qw(CFGOPT_ARCHIVE_QUEUE_MAX);

# Backup options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_CHECK                                   => 'archive-check';
    push @EXPORT, qw(CFGOPT_ARCHIVE_CHECK);
use constant CFGOPT_ARCHIVE_COPY                                    => 'archive-copy';
    push @EXPORT, qw(CFGOPT_ARCHIVE_COPY);
use constant CFGOPT_BACKUP_CMD                                      => 'backup-cmd';
    push @EXPORT, qw(CFGOPT_BACKUP_CMD);
use constant CFGOPT_BACKUP_CONFIG                                   => 'backup-config';
    push @EXPORT, qw(CFGOPT_BACKUP_CONFIG);
use constant CFGOPT_BACKUP_HOST                                     => 'backup-host';
    push @EXPORT, qw(CFGOPT_BACKUP_HOST);
use constant CFGOPT_BACKUP_SSH_PORT                                 => 'backup-ssh-port';
    push @EXPORT, qw(CFGOPT_BACKUP_SSH_PORT);
use constant CFGOPT_BACKUP_STANDBY                                  => 'backup-standby';
    push @EXPORT, qw(CFGOPT_BACKUP_STANDBY);
use constant CFGOPT_BACKUP_USER                                     => 'backup-user';
    push @EXPORT, qw(CFGOPT_BACKUP_USER);
use constant CFGOPT_CHECKSUM_PAGE                                   => 'checksum-page';
    push @EXPORT, qw(CFGOPT_CHECKSUM_PAGE);
use constant CFGOPT_HARDLINK                                        => 'hardlink';
    push @EXPORT, qw(CFGOPT_HARDLINK);
use constant CFGOPT_MANIFEST_SAVE_THRESHOLD                         => 'manifest-save-threshold';
    push @EXPORT, qw(CFGOPT_MANIFEST_SAVE_THRESHOLD);
use constant CFGOPT_RESUME                                          => 'resume';
    push @EXPORT, qw(CFGOPT_RESUME);
use constant CFGOPT_START_FAST                                      => 'start-fast';
    push @EXPORT, qw(CFGOPT_START_FAST);
use constant CFGOPT_STOP_AUTO                                       => 'stop-auto';
    push @EXPORT, qw(CFGOPT_STOP_AUTO);

# Expire options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_RETENTION_ARCHIVE                               => 'retention-archive';
    push @EXPORT, qw(CFGOPT_RETENTION_ARCHIVE);
use constant CFGOPT_RETENTION_ARCHIVE_TYPE                          => 'retention-archive-type';
    push @EXPORT, qw(CFGOPT_RETENTION_ARCHIVE_TYPE);
use constant CFGOPT_RETENTION_DIFF                                  => 'retention-diff';
    push @EXPORT, qw(CFGOPT_RETENTION_DIFF);
use constant CFGOPT_RETENTION_FULL                                  => 'retention-full';
    push @EXPORT, qw(CFGOPT_RETENTION_FULL);

# Restore options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_DB_INCLUDE                                      => 'db-include';
    push @EXPORT, qw(CFGOPT_DB_INCLUDE);
use constant CFGOPT_LINK_ALL                                        => 'link-all';
    push @EXPORT, qw(CFGOPT_LINK_ALL);
use constant CFGOPT_LINK_MAP                                        => 'link-map';
    push @EXPORT, qw(CFGOPT_LINK_MAP);
use constant CFGOPT_TABLESPACE_MAP_ALL                              => 'tablespace-map-all';
    push @EXPORT, qw(CFGOPT_TABLESPACE_MAP_ALL);
use constant CFGOPT_TABLESPACE_MAP                                  => 'tablespace-map';
    push @EXPORT, qw(CFGOPT_TABLESPACE_MAP);
use constant CFGOPT_RECOVERY_OPTION                                 => 'recovery-option';
    push @EXPORT, qw(CFGOPT_RECOVERY_OPTION);

# Stanza options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many databases can be configured
use constant CFGDEF_INDEX_DB                                        => 2;

# Prefix that must be used by all db options that allow multiple configurations
use constant CFGDEF_PREFIX_DB                                       => 'db';

use constant CFGOPT_DB_CMD                                          => CFGDEF_PREFIX_DB . '-cmd';
    push @EXPORT, qw(CFGOPT_DB_CMD);
use constant CFGOPT_DB_CONFIG                                       => CFGDEF_PREFIX_DB . '-config';
    push @EXPORT, qw(CFGOPT_DB_CONFIG);
use constant CFGOPT_DB_HOST                                         => CFGDEF_PREFIX_DB . '-host';
    push @EXPORT, qw(CFGOPT_DB_HOST);
use constant CFGOPT_DB_PATH                                         => CFGDEF_PREFIX_DB . '-path';
    push @EXPORT, qw(CFGOPT_DB_PATH);
use constant CFGOPT_DB_PORT                                         => CFGDEF_PREFIX_DB . '-port';
    push @EXPORT, qw(CFGOPT_DB_PORT);
use constant CFGOPT_DB_SSH_PORT                                     => CFGDEF_PREFIX_DB . '-ssh-port';
    push @EXPORT, qw(CFGOPT_DB_SSH_PORT);
use constant CFGOPT_DB_SOCKET_PATH                                  => CFGDEF_PREFIX_DB . '-socket-path';
    push @EXPORT, qw(CFGOPT_DB_SOCKET_PATH);
use constant CFGOPT_DB_USER                                         => CFGDEF_PREFIX_DB . '-user';
    push @EXPORT, qw(CFGOPT_DB_USER);

####################################################################################################################################
# Option values - for options that have a specific list of allowed values
####################################################################################################################################

# Local/remote types
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_LOCAL_TYPE_DB                                => 'db';
    push @EXPORT, qw(CFGOPTVAL_LOCAL_TYPE_DB);
use constant CFGOPTVAL_LOCAL_TYPE_BACKUP                            => 'backup';
    push @EXPORT, qw(CFGOPTVAL_LOCAL_TYPE_BACKUP);
use constant CFGOPTVAL_REMOTE_TYPE_DB                               => CFGOPTVAL_LOCAL_TYPE_DB;
    push @EXPORT, qw(CFGOPTVAL_REMOTE_TYPE_DB);
use constant CFGOPTVAL_REMOTE_TYPE_BACKUP                           => CFGOPTVAL_LOCAL_TYPE_BACKUP;
    push @EXPORT, qw(CFGOPTVAL_REMOTE_TYPE_BACKUP);

# Backup type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_BACKUP_TYPE_FULL                             => 'full';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_FULL);
use constant CFGOPTVAL_BACKUP_TYPE_DIFF                             => 'diff';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_DIFF);
use constant CFGOPTVAL_BACKUP_TYPE_INCR                             => 'incr';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_INCR);

# Repo type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_REPO_TYPE_CIFS                               => 'cifs';
    push @EXPORT, qw(CFGOPTVAL_REPO_TYPE_CIFS);
use constant CFGOPTVAL_REPO_TYPE_POSIX                              => 'posix';
    push @EXPORT, qw(CFGOPTVAL_REPO_TYPE_POSIX);
use constant CFGOPTVAL_REPO_TYPE_S3                                 => 's3';
    push @EXPORT, qw(CFGOPTVAL_REPO_TYPE_S3);

# Info output
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_INFO_OUTPUT_TEXT                             => 'text';
    push @EXPORT, qw(CFGOPTVAL_INFO_OUTPUT_TEXT);
use constant CFGOPTVAL_INFO_OUTPUT_JSON                             => 'json';
    push @EXPORT, qw(CFGOPTVAL_INFO_OUTPUT_JSON);

# Restore type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_RESTORE_TYPE_NAME                            => 'name';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_NAME);
use constant CFGOPTVAL_RESTORE_TYPE_TIME                            => 'time';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_TIME);
use constant CFGOPTVAL_RESTORE_TYPE_XID                             => 'xid';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_XID);
use constant CFGOPTVAL_RESTORE_TYPE_PRESERVE                        => 'preserve';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_PRESERVE);
use constant CFGOPTVAL_RESTORE_TYPE_NONE                            => 'none';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_NONE);
use constant CFGOPTVAL_RESTORE_TYPE_IMMEDIATE                       => 'immediate';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_IMMEDIATE);
use constant CFGOPTVAL_RESTORE_TYPE_DEFAULT                         => 'default';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TYPE_DEFAULT);

# Restore target action
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE                  => 'pause';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE);
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE                => 'promote';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE);
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN               => 'shutdown';
    push @EXPORT, qw(CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN);

####################################################################################################################################
# Option defaults - only defined here when the default is used in more than one place
####################################################################################################################################
use constant CFGDEF_DEFAULT_BUFFER_SIZE_MIN                         => 16384;

use constant CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN                      => 0;
use constant CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX                      => 9;

use constant CFGDEF_DEFAULT_CONFIG                                  => '/etc/' . BACKREST_CONF;

use constant CFGDEF_DEFAULT_DB_TIMEOUT                              => 1800;
use constant CFGDEF_DEFAULT_DB_TIMEOUT_MIN                          => WAIT_TIME_MINIMUM;
use constant CFGDEF_DEFAULT_DB_TIMEOUT_MAX                          => 86400 * 7;

use constant CFGDEF_DEFAULT_RETENTION_MIN                           => 1;
use constant CFGDEF_DEFAULT_RETENTION_MAX                           => 999999999;

####################################################################################################################################
# Option definition constants - rules, types, sections, etc.
####################################################################################################################################

# Option rules
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDDEF_RULE_ALT_NAME                                => 'alt-name';
    push @EXPORT, qw(CFGBLDDEF_RULE_ALT_NAME);
use constant CFGBLDDEF_RULE_ALLOW_LIST                              => 'allow-list';
    push @EXPORT, qw(CFGBLDDEF_RULE_ALLOW_LIST);
use constant CFGBLDDEF_RULE_ALLOW_RANGE                             => 'allow-range';
    push @EXPORT, qw(CFGBLDDEF_RULE_ALLOW_RANGE);
use constant CFGBLDDEF_RULE_DEFAULT                                 => 'default';
    push @EXPORT, qw(CFGBLDDEF_RULE_DEFAULT);
use constant CFGBLDDEF_RULE_DEPEND                                  => 'depend';
    push @EXPORT, qw(CFGBLDDEF_RULE_DEPEND);
use constant CFGBLDDEF_RULE_DEPEND_OPTION                           => 'depend-option';
    push @EXPORT, qw(CFGBLDDEF_RULE_DEPEND_OPTION);
use constant CFGBLDDEF_RULE_DEPEND_LIST                             => 'depend-list';
    push @EXPORT, qw(CFGBLDDEF_RULE_DEPEND_LIST);
use constant CFGBLDDEF_RULE_HASH_VALUE                              => 'hash-value';
    push @EXPORT, qw(CFGBLDDEF_RULE_HASH_VALUE);
use constant CFGBLDDEF_RULE_HINT                                    => 'hint';
    push @EXPORT, qw(CFGBLDDEF_RULE_HINT);
use constant CFGBLDDEF_RULE_INDEX                                   => 'index';
    push @EXPORT, qw(CFGBLDDEF_RULE_INDEX);
use constant CFGBLDDEF_RULE_NEGATE                                  => 'negate';
    push @EXPORT, qw(CFGBLDDEF_RULE_NEGATE);
use constant CFGBLDDEF_RULE_PREFIX                                  => 'prefix';
    push @EXPORT, qw(CFGBLDDEF_RULE_PREFIX);
use constant CFGBLDDEF_RULE_COMMAND                                 => 'command';
    push @EXPORT, qw(CFGBLDDEF_RULE_COMMAND);
use constant CFGBLDDEF_RULE_REQUIRED                                => 'required';
    push @EXPORT, qw(CFGBLDDEF_RULE_REQUIRED);
use constant CFGBLDDEF_RULE_SECTION                                 => 'section';
    push @EXPORT, qw(CFGBLDDEF_RULE_SECTION);
use constant CFGBLDDEF_RULE_SECURE                                  => 'secure';
    push @EXPORT, qw(CFGBLDDEF_RULE_SECURE);
use constant CFGBLDDEF_RULE_TYPE                                    => 'type';
    push @EXPORT, qw(CFGBLDDEF_RULE_TYPE);

# Option rules
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTDEF_TYPE_BOOLEAN                                 => 'boolean';
    push @EXPORT, qw(CFGOPTDEF_TYPE_BOOLEAN);
use constant CFGOPTDEF_TYPE_FLOAT                                   => 'float';
    push @EXPORT, qw(CFGOPTDEF_TYPE_FLOAT);
use constant CFGOPTDEF_TYPE_HASH                                    => 'hash';
    push @EXPORT, qw(CFGOPTDEF_TYPE_HASH);
use constant CFGOPTDEF_TYPE_INTEGER                                 => 'integer';
    push @EXPORT, qw(CFGOPTDEF_TYPE_INTEGER);
use constant CFGOPTDEF_TYPE_STRING                                  => 'string';
    push @EXPORT, qw(CFGOPTDEF_TYPE_STRING);

# Option config sections
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_SECTION_GLOBAL                               => 'global';
    push @EXPORT, qw(CFGDEF_SECTION_GLOBAL);
use constant CFGDEF_SECTION_STANZA                               => 'stanza';
    push @EXPORT, qw(CFGDEF_SECTION_STANZA);

####################################################################################################################################
# Option rules
####################################################################################################################################
my %hOptionRule =
(
    # Command-line only options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_CONFIG =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_NEGATE => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_DELTA =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            }
        }
    },

    &CFGOPT_FORCE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_ONLINE,
                    &CFGBLDDEF_RULE_DEPEND_LIST => [false],
                },
            },

            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            },

            &CFGCMD_STANZA_CREATE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            },

            &CFGCMD_STOP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false
            }
        }
    },

    &CFGOPT_ONLINE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_NEGATE => true,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_SET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => 'latest',
            }
        }
    },

    &CFGOPT_STANZA =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_STOP =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            }
        }
    },

    &CFGOPT_TARGET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    [
                        &CFGOPTVAL_RESTORE_TYPE_NAME,
                        &CFGOPTVAL_RESTORE_TYPE_TIME,
                        &CFGOPTVAL_RESTORE_TYPE_XID,
                    ],
                },
            },
        },
    },

    &CFGOPT_TARGET_EXCLUSIVE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    [
                        &CFGOPTVAL_RESTORE_TYPE_TIME,
                        &CFGOPTVAL_RESTORE_TYPE_XID,
                    ],
                },
            },
        },
    },

    &CFGOPT_TARGET_ACTION =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE,

                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE,
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE,
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN,
                ],

                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    [
                        &CFGOPTVAL_RESTORE_TYPE_NAME,
                        &CFGOPTVAL_RESTORE_TYPE_TIME,
                        &CFGOPTVAL_RESTORE_TYPE_XID,
                    ],
                },
            },
        },
    },

    &CFGOPT_TARGET_TIMELINE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    [
                        &CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                        &CFGOPTVAL_RESTORE_TYPE_NAME,
                        &CFGOPTVAL_RESTORE_TYPE_TIME,
                        &CFGOPTVAL_RESTORE_TYPE_XID,
                    ],
                },
            },
        },
    },

    &CFGOPT_TYPE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_BACKUP_TYPE_INCR,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_BACKUP_TYPE_FULL,
                    &CFGOPTVAL_BACKUP_TYPE_DIFF,
                    &CFGOPTVAL_BACKUP_TYPE_INCR,
                ]
            },

            &CFGCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_LOCAL_TYPE_DB,
                    &CFGOPTVAL_LOCAL_TYPE_BACKUP,
                ],
            },

            &CFGCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_REMOTE_TYPE_DB,
                    &CFGOPTVAL_REMOTE_TYPE_BACKUP,
                ],
            },

            &CFGCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_RESTORE_TYPE_NAME,
                    &CFGOPTVAL_RESTORE_TYPE_TIME,
                    &CFGOPTVAL_RESTORE_TYPE_XID,
                    &CFGOPTVAL_RESTORE_TYPE_PRESERVE,
                    &CFGOPTVAL_RESTORE_TYPE_NONE,
                    &CFGOPTVAL_RESTORE_TYPE_IMMEDIATE,
                    &CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                ]
            }
        }
    },

    &CFGOPT_OUTPUT =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_INFO =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_INFO_OUTPUT_TEXT,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                [
                    &CFGOPTVAL_INFO_OUTPUT_TEXT,
                    &CFGOPTVAL_INFO_OUTPUT_JSON,
                ]
            }
        }
    },

    # Command-line only local/remote options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_COMMAND =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
        }
    },

    &CFGOPT_HOST_ID =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_LOCAL => {},
        },
    },

    &CFGOPT_PROCESS =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true,
            },
            &CFGCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false,
            },
        },
    },

    # Command-line only test options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_TEST =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_TEST_DELAY =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => 5,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TEST,
            &CFGBLDDEF_RULE_DEPEND_LIST => [true],
        },
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_TEST,
    },

    &CFGOPT_TEST_POINT =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND => CFGOPT_TEST_DELAY,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_TEST,
    },

    # General options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => 60,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [WAIT_TIME_MINIMUM, 86400],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
        },
    },

    &CFGOPT_BUFFER_SIZE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => COMMON_IO_BUFFER_MAX,
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        [
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 2,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 4,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 8,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 16,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 32,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 64,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 128,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 256,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 512,
            &CFGDEF_DEFAULT_BUFFER_SIZE_MIN * 1024,
        ],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_DB_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => CFGDEF_DEFAULT_DB_TIMEOUT,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_DB_TIMEOUT_MIN, CFGDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_COMPRESS =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_COMPRESS_LEVEL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 6,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_COMPRESS_LEVEL_NETWORK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 3,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_NEUTRAL_UMASK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_CMD_SSH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'ssh',
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_LOCK_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/tmp/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_LOG_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/log/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_PROTOCOL_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => CFGDEF_DEFAULT_DB_TIMEOUT + 30,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_DB_TIMEOUT_MIN, CFGDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_REPO_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/lib/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_REPO_S3_BUCKET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_REPO_TYPE,
            &CFGBLDDEF_RULE_DEPEND_LIST => [CFGOPTVAL_REPO_TYPE_S3],
        },
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_CA_FILE => &CFGOPT_REPO_S3_HOST,
    &CFGOPT_REPO_S3_CA_PATH => &CFGOPT_REPO_S3_HOST,

    &CFGOPT_REPO_S3_KEY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_SECURE => true,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_REPO_TYPE,
            &CFGBLDDEF_RULE_DEPEND_LIST => [CFGOPTVAL_REPO_TYPE_S3],
        },
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_KEY_SECRET => CFGOPT_REPO_S3_KEY,

    &CFGOPT_REPO_S3_ENDPOINT => CFGOPT_REPO_S3_BUCKET,

    &CFGOPT_REPO_S3_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED  => false,
        &CFGBLDDEF_RULE_DEPEND => CFGOPT_REPO_S3_BUCKET,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_REGION => CFGOPT_REPO_S3_BUCKET,

    &CFGOPT_REPO_S3_VERIFY_SSL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_REPO_TYPE,
        &CFGBLDDEF_RULE_DEPEND => CFGOPT_REPO_S3_BUCKET,
    },

    &CFGOPT_REPO_TYPE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_REPO_TYPE_POSIX,
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        [
            &CFGOPTVAL_REPO_TYPE_CIFS,
            &CFGOPTVAL_REPO_TYPE_POSIX,
            &CFGOPTVAL_REPO_TYPE_S3,
        ],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_SPOOL_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/spool/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_ARCHIVE_ASYNC,
            &CFGBLDDEF_RULE_DEPEND_LIST => [true],
        },
    },

    &CFGOPT_PROCESS_MAX =>
    {
        &CFGBLDDEF_RULE_ALT_NAME => 'thread-max',
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 1,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [1, 96],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_RESTORE => {},
        }
    },

    # Logging options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_LOG_LEVEL_CONSOLE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(WARN),
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        [
            lc(OFF),
            lc(ERROR),
            lc(WARN),
            lc(INFO),
            lc(DETAIL),
            lc(DEBUG),
            lc(TRACE),
        ],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_LOG_LEVEL_FILE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(INFO),
        &CFGBLDDEF_RULE_ALLOW_LIST => CFGOPT_LOG_LEVEL_CONSOLE,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_LOG_LEVEL_CONSOLE,
    },

    &CFGOPT_LOG_LEVEL_STDERR =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(WARN),
        &CFGBLDDEF_RULE_ALLOW_LIST => CFGOPT_LOG_LEVEL_CONSOLE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_LOG_TIMESTAMP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_LOG_LEVEL_CONSOLE,
    },

    # Archive options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_ASYNC =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        }
    },

    # Deprecated and to be removed
    &CFGOPT_ARCHIVE_MAX_MB =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        }
    },

    &CFGOPT_ARCHIVE_QUEUE_MAX =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        },
    },

    # Backup options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_CHECK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_ONLINE,
            &CFGBLDDEF_RULE_DEPEND_LIST => [true],
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
        },
    },

    &CFGOPT_ARCHIVE_COPY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_ARCHIVE_CHECK,
                    &CFGBLDDEF_RULE_DEPEND_LIST => [true],
                }
            }
        }
    },

    &CFGOPT_BACKUP_CMD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_BACKUP_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_BACKUP_HOST
        },
    },

    &CFGOPT_BACKUP_CONFIG =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_BACKUP_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_BACKUP_HOST
        },
    },

    &CFGOPT_BACKUP_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_BACKUP_SSH_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_BACKUP_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_BACKUP_HOST
        }
    },

    &CFGOPT_BACKUP_STANDBY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGOPT_BACKUP_USER =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'backrest',
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_BACKUP_HOST,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_BACKUP_HOST
        }
    },

    &CFGOPT_CHECKSUM_PAGE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_HARDLINK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_MANIFEST_SAVE_THRESHOLD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 1073741824,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_RESUME =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_START_FAST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_STOP_AUTO =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    # Expire options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_RETENTION_ARCHIVE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_RETENTION_ARCHIVE_TYPE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGOPTVAL_BACKUP_TYPE_FULL,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        },
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        [
            &CFGOPTVAL_BACKUP_TYPE_FULL,
            &CFGOPTVAL_BACKUP_TYPE_DIFF,
            &CFGOPTVAL_BACKUP_TYPE_INCR,
        ]
    },

    &CFGOPT_RETENTION_DIFF =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_RETENTION_FULL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    # Restore options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_DB_INCLUDE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_HASH_VALUE => false,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_LINK_ALL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_LINK_MAP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_TABLESPACE_MAP_ALL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_TABLESPACE_MAP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_RECOVERY_OPTION =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_TYPE,
            &CFGBLDDEF_RULE_DEPEND_LIST =>
            [
                &CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                &CFGOPTVAL_RESTORE_TYPE_NAME,
                &CFGOPTVAL_RESTORE_TYPE_TIME,
                &CFGOPTVAL_RESTORE_TYPE_XID,
            ],
        },
    },

    # Stanza options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_DB_CMD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_DB_HOST
        },
    },

    &CFGOPT_DB_CONFIG =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_DB_HOST
        },
    },

    &CFGOPT_DB_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_DB_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => true,
        &CFGBLDDEF_RULE_HINT => "does this stanza exist?",
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_ARCHIVE_PUSH =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGOPT_DB_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_DEFAULT => 5432,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_DB_SSH_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND => CFGOPT_DB_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_DB_HOST
        },
    },

    &CFGOPT_DB_SOCKET_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_DB_USER =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_PREFIX => CFGDEF_PREFIX_DB,
        &CFGBLDDEF_RULE_TYPE => CFGOPTDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'postgres',
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGOPT_DB_HOST
        },
    },
);

####################################################################################################################################
# Process rule defaults
####################################################################################################################################
foreach my $strKey (sort(keys(%hOptionRule)))
{
    # If the rule is a scalar then copy the entire rule from the referenced option
    if (!ref($hOptionRule{$strKey}))
    {
        $hOptionRule{$strKey} = dclone($hOptionRule{$hOptionRule{$strKey}});
    }

    # If the command section is a scalar then copy the section from the referenced option
    if (defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_COMMAND}) && !ref($hOptionRule{$strKey}{&CFGBLDDEF_RULE_COMMAND}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_COMMAND} =
            dclone($hOptionRule{$hOptionRule{$strKey}{&CFGBLDDEF_RULE_COMMAND}}{&CFGBLDDEF_RULE_COMMAND});
    }

    # If the required section is a scalar then copy the section from the referenced option
    if (defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_DEPEND}) && !ref($hOptionRule{$strKey}{&CFGBLDDEF_RULE_DEPEND}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_DEPEND} =
            dclone($hOptionRule{$hOptionRule{$strKey}{&CFGBLDDEF_RULE_DEPEND}}{&CFGBLDDEF_RULE_DEPEND});
    }

    # If the allow list is a scalar then copy the list from the referenced option
    if (defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_ALLOW_LIST}) && !ref($hOptionRule{$strKey}{&CFGBLDDEF_RULE_ALLOW_LIST}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_ALLOW_LIST} =
            dclone($hOptionRule{$hOptionRule{$strKey}{&CFGBLDDEF_RULE_ALLOW_LIST}}{&CFGBLDDEF_RULE_ALLOW_LIST});
    }

    # Default type is string
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE}))
    {
        &log(ASSERT, "type is required for option '${strKey}'");
    }

    # Hash types by default have hash values (rather than just a boolean list)
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_HASH_VALUE}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_HASH_VALUE} =
            $hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE} eq CFGOPTDEF_TYPE_HASH ? true : false;
    }

    # All boolean config options can be negated.  Boolean command-line options must be marked for negation individually.
    if ($hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE} eq CFGOPTDEF_TYPE_BOOLEAN &&
        defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_SECTION}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_NEGATE} = true;
    }

    # Default for negation is false
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_NEGATE}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_NEGATE} = false;
    }

    # By default options are not secure
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_SECURE}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_SECURE} = false;
    }

    # Set all indices to 1 by default - this defines how many copies of any option there can be
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_INDEX}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_INDEX} = 1;
    }
}

####################################################################################################################################
# Generate indexed rules
####################################################################################################################################
my $rhOptionRuleIndex = dclone(\%hOptionRule);

foreach my $strKey (sort(keys(%{$rhOptionRuleIndex})))
{
    # Build options for all possible db configurations
    if (defined($rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX}) &&
        $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX} eq CFGDEF_PREFIX_DB)
    {
        my $strPrefix = $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX};
        $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_INDEX} = CFGDEF_INDEX_DB;

        for (my $iIndex = 1; $iIndex <= CFGDEF_INDEX_DB; $iIndex++)
        {
            my $strKeyNew = "${strPrefix}${iIndex}" . substr($strKey, length($strPrefix));

            $rhOptionRuleIndex->{$strKeyNew} = dclone($rhOptionRuleIndex->{$strKey});

            # Create the alternate name for option index 1
            if ($iIndex == 1)
            {
                $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_ALT_NAME} = $strKey;
            }
            else
            {
                $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_REQUIRED} = false;
            }

            if (defined($rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_DEPEND}) &&
                defined($rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_DEPEND}{&CFGBLDDEF_RULE_DEPEND_OPTION}))
            {
                $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_DEPEND}{&CFGBLDDEF_RULE_DEPEND_OPTION} =
                    "${strPrefix}${iIndex}" .
                    substr(
                        $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDDEF_RULE_DEPEND}{&CFGBLDDEF_RULE_DEPEND_OPTION},
                        length($strPrefix));
            }
        }

        delete($rhOptionRuleIndex->{$strKey});
    }
}

####################################################################################################################################
# cfgbldCommandRule - returns the option rules based on the command.
####################################################################################################################################
sub cfgbldCommandRule
{
    my $strOption = shift;
    my $strCommand = shift;

    if (defined($strCommand))
    {
        return defined($hOptionRule{$strOption}{&CFGBLDDEF_RULE_COMMAND}) &&
               defined($hOptionRule{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}) &&
               ref($hOptionRule{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand}) eq 'HASH' ?
               $hOptionRule{$strOption}{&CFGBLDDEF_RULE_COMMAND}{$strCommand} : undef;
    }

    return;
}

####################################################################################################################################
# cfgbldOptionDefault - does the option have a default for this command?
####################################################################################################################################
sub cfgbldOptionDefault
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = cfgbldCommandRule($strOption, $strCommand);

    # Check for default in command
    my $strDefault = defined($oCommandRule) ? $$oCommandRule{&CFGBLDDEF_RULE_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $hOptionRule{$strOption}{&CFGBLDDEF_RULE_DEFAULT};
}

push @EXPORT, qw(cfgbldOptionDefault);

####################################################################################################################################
# cfgbldOptionRange - get the allowed setting range for the option if it exists
####################################################################################################################################
sub cfgbldOptionRange
{
    my $strOption = shift;
    my $strCommand = shift;

    # Get the command rule
    my $oCommandRule = cfgbldCommandRule($strOption, $strCommand);

    # Check for default in command
    if (defined($oCommandRule) && defined($$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}))
    {
        return $$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}[0], $$oCommandRule{&CFGBLDDEF_RULE_ALLOW_RANGE}[1];
    }

    # If defined return, else try to grab the global default
    return $hOptionRule{$strOption}{&CFGBLDDEF_RULE_ALLOW_RANGE}[0], $hOptionRule{$strOption}{&CFGBLDDEF_RULE_ALLOW_RANGE}[1];
}

push @EXPORT, qw(cfgbldOptionRange);

####################################################################################################################################
# cfgbldOptionType - get the option type
####################################################################################################################################
sub cfgbldOptionType
{
    my $strOption = shift;

    return $hOptionRule{$strOption}{&CFGBLDDEF_RULE_TYPE};
}

push @EXPORT, qw(cfgbldOptionType);

####################################################################################################################################
# cfgbldOptionTypeTest - test the option type
####################################################################################################################################
sub cfgbldOptionTypeTest
{
    my $strOption = shift;
    my $strType = shift;

    return cfgbldOptionType($strOption) eq $strType;
}

push @EXPORT, qw(cfgbldOptionTypeTest);

####################################################################################################################################
# cfgdefRule - get option rules without indexed options
####################################################################################################################################
sub cfgdefRule
{
    return dclone(\%hOptionRule);
}

push @EXPORT, qw(cfgdefRule);

####################################################################################################################################
# cfgdefRuleIndex - get option rules
####################################################################################################################################
sub cfgdefRuleIndex
{
    return dclone($rhOptionRuleIndex);
}

push @EXPORT, qw(cfgdefRuleIndex);

####################################################################################################################################
# cfgbldCommandGet - get the hash that contains all valid commands
####################################################################################################################################
sub cfgbldCommandGet
{
    my $rhCommand;

    # Get commands from the rule hash
    foreach my $strOption (sort(keys(%hOptionRule)))
    {
        foreach my $strCommand (sort(keys(%{$hOptionRule{$strOption}{&CFGBLDDEF_RULE_COMMAND}})))
        {
            $rhCommand->{$strCommand} = true;
        }
    }

    # Add special commands
    $rhCommand->{&CFGCMD_HELP} = true;
    $rhCommand->{&CFGCMD_VERSION} = true;

    return $rhCommand;
}

push @EXPORT, qw(cfgbldCommandGet);

1;
