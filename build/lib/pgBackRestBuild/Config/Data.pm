####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package pgBackRestBuild::Config::Data;

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
use constant CFGBLDCMD_ARCHIVE_GET                                  => 'archive-get';
    push @EXPORT, qw(CFGBLDCMD_ARCHIVE_GET);
use constant CFGBLDCMD_ARCHIVE_PUSH                                 => 'archive-push';
    push @EXPORT, qw(CFGBLDCMD_ARCHIVE_PUSH);
use constant CFGBLDCMD_BACKUP                                       => 'backup';
    push @EXPORT, qw(CFGBLDCMD_BACKUP);
use constant CFGBLDCMD_CHECK                                        => 'check';
    push @EXPORT, qw(CFGBLDCMD_CHECK);
use constant CFGBLDCMD_EXPIRE                                       => 'expire';
    push @EXPORT, qw(CFGBLDCMD_EXPIRE);
use constant CFGBLDCMD_HELP                                         => 'help';
    push @EXPORT, qw(CFGBLDCMD_HELP);
use constant CFGBLDCMD_INFO                                         => 'info';
    push @EXPORT, qw(CFGBLDCMD_INFO);
use constant CFGBLDCMD_LOCAL                                        => 'local';
    push @EXPORT, qw(CFGBLDCMD_LOCAL);
use constant CFGBLDCMD_REMOTE                                       => 'remote';
    push @EXPORT, qw(CFGBLDCMD_REMOTE);
use constant CFGBLDCMD_RESTORE                                      => 'restore';
    push @EXPORT, qw(CFGBLDCMD_RESTORE);
use constant CFGBLDCMD_STANZA_CREATE                                => 'stanza-create';
    push @EXPORT, qw(CFGBLDCMD_STANZA_CREATE);
use constant CFGBLDCMD_STANZA_UPGRADE                               => 'stanza-upgrade';
    push @EXPORT, qw(CFGBLDCMD_STANZA_UPGRADE);
use constant CFGBLDCMD_START                                        => 'start';
    push @EXPORT, qw(CFGBLDCMD_START);
use constant CFGBLDCMD_STOP                                         => 'stop';
    push @EXPORT, qw(CFGBLDCMD_STOP);
use constant CFGBLDCMD_VERSION                                      => 'version';
    push @EXPORT, qw(CFGBLDCMD_VERSION);

####################################################################################################################################
# Option constants - options that are allowed for commands
####################################################################################################################################

# Command-line only options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_CONFIG                                       => 'config';
    push @EXPORT, qw(CFGBLDOPT_CONFIG);
use constant CFGBLDOPT_DELTA                                        => 'delta';
    push @EXPORT, qw(CFGBLDOPT_DELTA);
use constant CFGBLDOPT_FORCE                                        => 'force';
    push @EXPORT, qw(CFGBLDOPT_FORCE);
use constant CFGBLDOPT_ONLINE                                       => 'online';
    push @EXPORT, qw(CFGBLDOPT_ONLINE);
use constant CFGBLDOPT_SET                                          => 'set';
    push @EXPORT, qw(CFGBLDOPT_SET);
use constant CFGBLDOPT_STANZA                                       => 'stanza';
    push @EXPORT, qw(CFGBLDOPT_STANZA);
use constant CFGBLDOPT_TARGET                                       => 'target';
    push @EXPORT, qw(CFGBLDOPT_TARGET);
use constant CFGBLDOPT_TARGET_EXCLUSIVE                             => 'target-exclusive';
    push @EXPORT, qw(CFGBLDOPT_TARGET_EXCLUSIVE);
use constant CFGBLDOPT_TARGET_ACTION                                => 'target-action';
    push @EXPORT, qw(CFGBLDOPT_TARGET_ACTION);
use constant CFGBLDOPT_TARGET_TIMELINE                              => 'target-timeline';
    push @EXPORT, qw(CFGBLDOPT_TARGET_TIMELINE);
use constant CFGBLDOPT_TYPE                                         => 'type';
    push @EXPORT, qw(CFGBLDOPT_TYPE);
use constant CFGBLDOPT_OUTPUT                                       => 'output';
    push @EXPORT, qw(CFGBLDOPT_OUTPUT);

# Command-line only local/remote optiosn
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_COMMAND                                      => 'command';
    push @EXPORT, qw(CFGBLDOPT_COMMAND);
use constant CFGBLDOPT_PROCESS                                      => 'process';
    push @EXPORT, qw(CFGBLDOPT_PROCESS);
use constant CFGBLDOPT_HOST_ID                                      => 'host-id';
    push @EXPORT, qw(CFGBLDOPT_HOST_ID);

# Command-line only test options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_TEST                                         => 'test';
    push @EXPORT, qw(CFGBLDOPT_TEST);
use constant CFGBLDOPT_TEST_DELAY                                   => 'test-delay';
    push @EXPORT, qw(CFGBLDOPT_TEST_DELAY);
use constant CFGBLDOPT_TEST_POINT                                   => 'test-point';
    push @EXPORT, qw(CFGBLDOPT_TEST_POINT);

# General options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_TIMEOUT                              => 'archive-timeout';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_TIMEOUT);
use constant CFGBLDOPT_BUFFER_SIZE                                  => 'buffer-size';
    push @EXPORT, qw(CFGBLDOPT_BUFFER_SIZE);
use constant CFGBLDOPT_DB_TIMEOUT                                   => 'db-timeout';
    push @EXPORT, qw(CFGBLDOPT_DB_TIMEOUT);
use constant CFGBLDOPT_COMPRESS                                     => 'compress';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS);
use constant CFGBLDOPT_COMPRESS_LEVEL                               => 'compress-level';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS_LEVEL);
use constant CFGBLDOPT_COMPRESS_LEVEL_NETWORK                       => 'compress-level-network';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS_LEVEL_NETWORK);
use constant CFGBLDOPT_NEUTRAL_UMASK                                => 'neutral-umask';
    push @EXPORT, qw(CFGBLDOPT_NEUTRAL_UMASK);
use constant CFGBLDOPT_PROTOCOL_TIMEOUT                             => 'protocol-timeout';
    push @EXPORT, qw(CFGBLDOPT_PROTOCOL_TIMEOUT);
use constant CFGBLDOPT_PROCESS_MAX                                  => 'process-max';
    push @EXPORT, qw(CFGBLDOPT_PROCESS_MAX);

# Commands
use constant CFGBLDOPT_CMD_SSH                                      => 'cmd-ssh';
    push @EXPORT, qw(CFGBLDOPT_CMD_SSH);

# Paths
use constant CFGBLDOPT_LOCK_PATH                                    => 'lock-path';
    push @EXPORT, qw(CFGBLDOPT_LOCK_PATH);
use constant CFGBLDOPT_LOG_PATH                                     => 'log-path';
    push @EXPORT, qw(CFGBLDOPT_LOG_PATH);
use constant CFGBLDOPT_SPOOL_PATH                                   => 'spool-path';
    push @EXPORT, qw(CFGBLDOPT_SPOOL_PATH);

# Repository
use constant CFGBLDOPT_REPO_PATH                                    => 'repo-path';
    push @EXPORT, qw(CFGBLDOPT_REPO_PATH);
use constant CFGBLDOPT_REPO_TYPE                                    => 'repo-type';
    push @EXPORT, qw(CFGBLDOPT_REPO_TYPE);

# Repository S3
use constant CFGBLDOPT_REPO_S3_KEY                                  => 'repo-s3-key';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_KEY);
use constant CFGBLDOPT_REPO_S3_KEY_SECRET                           => 'repo-s3-key-secret';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_KEY_SECRET);
use constant CFGBLDOPT_REPO_S3_BUCKET                               => 'repo-s3-bucket';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_BUCKET);
use constant CFGBLDOPT_REPO_S3_CA_FILE                              => 'repo-s3-ca-file';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_CA_FILE);
use constant CFGBLDOPT_REPO_S3_CA_PATH                              => 'repo-s3-ca-path';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_CA_PATH);
use constant CFGBLDOPT_REPO_S3_ENDPOINT                             => 'repo-s3-endpoint';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_ENDPOINT);
use constant CFGBLDOPT_REPO_S3_HOST                                 => 'repo-s3-host';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_HOST);
use constant CFGBLDOPT_REPO_S3_REGION                               => 'repo-s3-region';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_REGION);
use constant CFGBLDOPT_REPO_S3_VERIFY_SSL                           => 'repo-s3-verify-ssl';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_VERIFY_SSL);

# Logging
use constant CFGBLDOPT_LOG_LEVEL_CONSOLE                            => 'log-level-console';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_CONSOLE);
use constant CFGBLDOPT_LOG_LEVEL_FILE                               => 'log-level-file';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_FILE);
use constant CFGBLDOPT_LOG_LEVEL_STDERR                             => 'log-level-stderr';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_STDERR);
use constant CFGBLDOPT_LOG_TIMESTAMP                                => 'log-timestamp';
    push @EXPORT, qw(CFGBLDOPT_LOG_TIMESTAMP);

# Archive options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_ASYNC                                => 'archive-async';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_ASYNC);
# Deprecated and to be removed
use constant CFGBLDOPT_ARCHIVE_MAX_MB                               => 'archive-max-mb';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_MAX_MB);
use constant CFGBLDOPT_ARCHIVE_QUEUE_MAX                            => 'archive-queue-max';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_QUEUE_MAX);

# Backup options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_CHECK                                => 'archive-check';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_CHECK);
use constant CFGBLDOPT_BACKUP_ARCHIVE_COPY                          => 'archive-copy';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_ARCHIVE_COPY);
use constant CFGBLDOPT_BACKUP_CMD                                   => 'backup-cmd';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_CMD);
use constant CFGBLDOPT_BACKUP_CONFIG                                => 'backup-config';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_CONFIG);
use constant CFGBLDOPT_BACKUP_HOST                                  => 'backup-host';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_HOST);
use constant CFGBLDOPT_BACKUP_SSH_PORT                              => 'backup-ssh-port';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_SSH_PORT);
use constant CFGBLDOPT_BACKUP_STANDBY                               => 'backup-standby';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_STANDBY);
use constant CFGBLDOPT_BACKUP_USER                                  => 'backup-user';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_USER);
use constant CFGBLDOPT_CHECKSUM_PAGE                                => 'checksum-page';
    push @EXPORT, qw(CFGBLDOPT_CHECKSUM_PAGE);
use constant CFGBLDOPT_HARDLINK                                     => 'hardlink';
    push @EXPORT, qw(CFGBLDOPT_HARDLINK);
use constant CFGBLDOPT_MANIFEST_SAVE_THRESHOLD                      => 'manifest-save-threshold';
    push @EXPORT, qw(CFGBLDOPT_MANIFEST_SAVE_THRESHOLD);
use constant CFGBLDOPT_RESUME                                       => 'resume';
    push @EXPORT, qw(CFGBLDOPT_RESUME);
use constant CFGBLDOPT_START_FAST                                   => 'start-fast';
    push @EXPORT, qw(CFGBLDOPT_START_FAST);
use constant CFGBLDOPT_STOP_AUTO                                    => 'stop-auto';
    push @EXPORT, qw(CFGBLDOPT_STOP_AUTO);

# Expire options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_RETENTION_ARCHIVE                            => 'retention-archive';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_ARCHIVE);
use constant CFGBLDOPT_RETENTION_ARCHIVE_TYPE                       => 'retention-archive-type';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_ARCHIVE_TYPE);
use constant CFGBLDOPT_RETENTION_DIFF                               => 'retention-diff';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_DIFF);
use constant CFGBLDOPT_RETENTION_FULL                               => 'retention-full';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_FULL);

# Restore options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_DB_INCLUDE                                   => 'db-include';
    push @EXPORT, qw(CFGBLDOPT_DB_INCLUDE);
use constant CFGBLDOPT_LINK_ALL                                     => 'link-all';
    push @EXPORT, qw(CFGBLDOPT_LINK_ALL);
use constant CFGBLDOPT_LINK_MAP                                     => 'link-map';
    push @EXPORT, qw(CFGBLDOPT_LINK_MAP);
use constant CFGBLDOPT_TABLESPACE_MAP_ALL                           => 'tablespace-map-all';
    push @EXPORT, qw(CFGBLDOPT_TABLESPACE_MAP_ALL);
use constant CFGBLDOPT_TABLESPACE_MAP                               => 'tablespace-map';
    push @EXPORT, qw(CFGBLDOPT_TABLESPACE_MAP);
use constant CFGBLDOPT_RECOVERY_OPTION                              => 'recovery-option';
    push @EXPORT, qw(CFGBLDOPT_RECOVERY_OPTION);

# Stanza options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many databases can be configured
use constant CFGBLDOPTDEF_INDEX_DB                                  => 8;

# Prefix that must be used by all db options that allow multiple configurations
use constant CFGBLDOPT_PREFIX_DB                                    => 'db';

use constant CFGBLDOPT_DB_CMD                                       => CFGBLDOPT_PREFIX_DB . '-cmd';
    push @EXPORT, qw(CFGBLDOPT_DB_CMD);
use constant CFGBLDOPT_DB_CONFIG                                    => CFGBLDOPT_PREFIX_DB . '-config';
    push @EXPORT, qw(CFGBLDOPT_DB_CONFIG);
use constant CFGBLDOPT_DB_HOST                                      => CFGBLDOPT_PREFIX_DB . '-host';
    push @EXPORT, qw(CFGBLDOPT_DB_HOST);
use constant CFGBLDOPT_DB_PATH                                      => CFGBLDOPT_PREFIX_DB . '-path';
    push @EXPORT, qw(CFGBLDOPT_DB_PATH);
use constant CFGBLDOPT_DB_PORT                                      => CFGBLDOPT_PREFIX_DB . '-port';
    push @EXPORT, qw(CFGBLDOPT_DB_PORT);
use constant CFGBLDOPT_DB_SSH_PORT                                  => CFGBLDOPT_PREFIX_DB . '-ssh-port';
    push @EXPORT, qw(CFGBLDOPT_DB_SSH_PORT);
use constant CFGBLDOPT_DB_SOCKET_PATH                               => CFGBLDOPT_PREFIX_DB . '-socket-path';
    push @EXPORT, qw(CFGBLDOPT_DB_SOCKET_PATH);
use constant CFGBLDOPT_DB_USER                                      => CFGBLDOPT_PREFIX_DB . '-user';
    push @EXPORT, qw(CFGBLDOPT_DB_USER);

####################################################################################################################################
# Option values - for options that have a specific list of allowed values
####################################################################################################################################

# Local/remote types
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_TYPE_DB                                   => 'db';
use constant CFGBLDOPTVAL_TYPE_BACKUP                               => 'backup';

# Backup type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_BACKUP_TYPE_FULL                          => 'full';
use constant CFGBLDOPTVAL_BACKUP_TYPE_DIFF                          => 'diff';
use constant CFGBLDOPTVAL_BACKUP_TYPE_INCR                          => 'incr';

# Repo type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_REPO_TYPE_CIFS                            => 'cifs';
use constant CFGBLDOPTVAL_REPO_TYPE_POSIX                           => 'posix';
use constant CFGBLDOPTVAL_REPO_TYPE_S3                              => 's3';

# Info output
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_INFO_OUTPUT_TEXT                          => 'text';
use constant CFGBLDOPTVAL_INFO_OUTPUT_JSON                          => 'json';

# Restore type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_RESTORE_TYPE_NAME                         => 'name';
use constant CFGBLDOPTVAL_RESTORE_TYPE_TIME                         => 'time';
use constant CFGBLDOPTVAL_RESTORE_TYPE_XID                          => 'xid';
use constant CFGBLDOPTVAL_RESTORE_TYPE_PRESERVE                     => 'preserve';
use constant CFGBLDOPTVAL_RESTORE_TYPE_NONE                         => 'none';
use constant CFGBLDOPTVAL_RESTORE_TYPE_IMMEDIATE                    => 'immediate';
use constant CFGBLDOPTVAL_RESTORE_TYPE_DEFAULT                      => 'default';

# Restore target action
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTVAL_RESTORE_TARGET_ACTION_PAUSE               => 'pause';
use constant CFGBLDOPTVAL_RESTORE_TARGET_ACTION_PROMOTE             => 'promote';
use constant CFGBLDOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN            => 'shutdown';

####################################################################################################################################
# Option defaults - only defined here when the default is used in more than one place
####################################################################################################################################
use constant CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN                      => 16384;

use constant CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MIN                   => 0;
use constant CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MAX                   => 9;

use constant CFGBLDDEF_DEFAULT_CONFIG                               => '/etc/' . BACKREST_CONF;

use constant CFGBLDDEF_DEFAULT_DB_TIMEOUT                           => 1800;
use constant CFGBLDDEF_DEFAULT_DB_TIMEOUT_MIN                       => WAIT_TIME_MINIMUM;
use constant CFGBLDDEF_DEFAULT_DB_TIMEOUT_MAX                       => 86400 * 7;

use constant CFGBLDDEF_DEFAULT_RETENTION_MIN                        => 1;
use constant CFGBLDDEF_DEFAULT_RETENTION_MAX                        => 999999999;

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
use constant CFGBLDDEF_RULE_DEPEND_VALUE                            => 'depend-value';
    push @EXPORT, qw(CFGBLDDEF_RULE_DEPEND_VALUE);
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
use constant CFGBLDDEF_TYPE_BOOLEAN                                 => 'boolean';
    push @EXPORT, qw(CFGBLDDEF_TYPE_BOOLEAN);
use constant CFGBLDDEF_TYPE_FLOAT                                   => 'float';
    push @EXPORT, qw(CFGBLDDEF_TYPE_FLOAT);
use constant CFGBLDDEF_TYPE_HASH                                    => 'hash';
    push @EXPORT, qw(CFGBLDDEF_TYPE_HASH);
use constant CFGBLDDEF_TYPE_INTEGER                                 => 'integer';
    push @EXPORT, qw(CFGBLDDEF_TYPE_INTEGER);
use constant CFGBLDDEF_TYPE_STRING                                  => 'string';
    push @EXPORT, qw(CFGBLDDEF_TYPE_STRING);

# Option config sections
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDDEF_SECTION_GLOBAL                               => 'global';
    push @EXPORT, qw(CFGBLDDEF_SECTION_GLOBAL);
use constant CFGBLDDEF_SECTION_STANZA                               => 'stanza';
    push @EXPORT, qw(CFGBLDDEF_SECTION_STANZA);

####################################################################################################################################
# Option Rule Hash
#
# Contains the rules for options: which commands the option can/cannot be specified, for which commands it is required, default
# settings, types, ranges, whether the option is negatable, whether it has dependencies, etc. The initial section is the global
# section meaning the rules defined there apply to all commands listed for the option.
#
# CFGBLDDEF_RULE_COMMAND:
#     List of commands the option can be used with this option.  An empty hash signifies that the command does no deviate from the
#     option defaults.  Otherwise, overrides can be specified.
#
# NOTE: If the option (A) has a dependency on another option (B) then the CFGBLDCMD_ must also be specified in the other option
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
# 	        &CFGBLDCMD_CHECK =>
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
my %hOptionRule =
(
    # Command-line only options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_CONFIG =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_NEGATE => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_DELTA =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            }
        }
    },

    &CFGBLDOPT_FORCE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_ONLINE,
                    &CFGBLDDEF_RULE_DEPEND_VALUE   => false
                }
            },

            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            },

            &CFGBLDCMD_STANZA_CREATE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
            },

            &CFGBLDCMD_STOP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false
            }
        }
    },

    &CFGBLDOPT_ONLINE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_NEGATE => true,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_SET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => 'latest',
            }
        }
    },

    &CFGBLDOPT_STANZA =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_ARCHIVE_PUSH =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_CHECK =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_EXPIRE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_INFO =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_STANZA_CREATE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_STANZA_UPGRADE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true
            },
            &CFGBLDCMD_START =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_STOP =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            }
        }
    },

    &CFGBLDOPT_TARGET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    {
                        &CFGBLDOPTVAL_RESTORE_TYPE_NAME => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_TIME => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_EXCLUSIVE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    {
                        &CFGBLDOPTVAL_RESTORE_TYPE_TIME => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_ACTION =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_RESTORE_TARGET_ACTION_PAUSE,

                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_RESTORE_TARGET_ACTION_PAUSE => true,
                    &CFGBLDOPTVAL_RESTORE_TARGET_ACTION_PROMOTE => true,
                    &CFGBLDOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN => true,
                },

                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    {
                        &CFGBLDOPTVAL_RESTORE_TYPE_NAME => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_TIME => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_TIMELINE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false,
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDDEF_RULE_DEPEND_LIST =>
                    {
                        &CFGBLDOPTVAL_RESTORE_TYPE_DEFAULT  => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_NAME     => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_TIME     => true,
                        &CFGBLDOPTVAL_RESTORE_TYPE_XID      => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TYPE =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_BACKUP_TYPE_INCR,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_BACKUP_TYPE_FULL => true,
                    &CFGBLDOPTVAL_BACKUP_TYPE_DIFF => true,
                    &CFGBLDOPTVAL_BACKUP_TYPE_INCR => true,
                }
            },

            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_TYPE_DB     => true,
                    &CFGBLDOPTVAL_TYPE_BACKUP => true,
                },
            },

            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_TYPE_DB     => true,
                    &CFGBLDOPTVAL_TYPE_BACKUP => true,
                },
            },

            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_RESTORE_TYPE_DEFAULT,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_RESTORE_TYPE_NAME      => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_TIME      => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_XID       => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_PRESERVE  => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_NONE      => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_IMMEDIATE => true,
                    &CFGBLDOPTVAL_RESTORE_TYPE_DEFAULT   => true
                }
            }
        }
    },

    &CFGBLDOPT_OUTPUT =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_INFO =>
            {
                &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_INFO_OUTPUT_TEXT,
                &CFGBLDDEF_RULE_ALLOW_LIST =>
                {
                    &CFGBLDOPTVAL_INFO_OUTPUT_TEXT => true,
                    &CFGBLDOPTVAL_INFO_OUTPUT_JSON => true
                }
            }
        }
    },

    # Command-line only local/remote options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_COMMAND =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
        }
    },

    &CFGBLDOPT_HOST_ID =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL => {},
        },
    },

    &CFGBLDOPT_PROCESS =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_REQUIRED => true,
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false,
            },
        },
    },

    # Command-line only test options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_TEST =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_TEST_DELAY =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => 5,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TEST,
            &CFGBLDDEF_RULE_DEPEND_VALUE => true
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_TEST_POINT =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TEST,
            &CFGBLDDEF_RULE_DEPEND_VALUE => true
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    # General options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => 60,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [WAIT_TIME_MINIMUM, 86400],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
        },
    },

    &CFGBLDOPT_BUFFER_SIZE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => COMMON_IO_BUFFER_MAX,
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN         => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 2     => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 4     => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 8     => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 16    => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 32    => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 64    => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 128   => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 256   => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 512   => true,
            &CFGBLDDEF_DEFAULT_BUFFER_SIZE_MIN * 1024  => true,
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_DB_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDDEF_DEFAULT_DB_TIMEOUT,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_DB_TIMEOUT_MIN, CFGBLDDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_COMPRESS =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_RESTORE => {},
        }
    },

    &CFGBLDOPT_COMPRESS_LEVEL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 6,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_COMPRESS_LEVEL_NETWORK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 3,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGBLDDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_NEUTRAL_UMASK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_CMD_SSH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'ssh',
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_LOCK_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/tmp/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_LOG_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/log/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_PROTOCOL_TIMEOUT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_FLOAT,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDDEF_DEFAULT_DB_TIMEOUT + 30,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_DB_TIMEOUT_MIN, CFGBLDDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_REPO_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/lib/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_REPO_S3_BUCKET =>
    {
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_REPO_TYPE,
            &CFGBLDDEF_RULE_DEPEND_VALUE   => CFGBLDOPTVAL_REPO_TYPE_S3,
        },
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_CA_FILE => &CFGBLDOPT_REPO_S3_HOST,
    &CFGBLDOPT_REPO_S3_CA_PATH => &CFGBLDOPT_REPO_S3_HOST,

    &CFGBLDOPT_REPO_S3_KEY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_SECURE => true,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_REPO_TYPE,
            &CFGBLDDEF_RULE_DEPEND_VALUE   => CFGBLDOPTVAL_REPO_TYPE_S3,
        },
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_KEY_SECRET => CFGBLDOPT_REPO_S3_KEY,

    &CFGBLDOPT_REPO_S3_ENDPOINT => CFGBLDOPT_REPO_S3_BUCKET,

    &CFGBLDOPT_REPO_S3_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED  => false,
        &CFGBLDDEF_RULE_DEPEND => CFGBLDOPT_REPO_S3_BUCKET,
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_REGION => CFGBLDOPT_REPO_S3_BUCKET,

    &CFGBLDOPT_REPO_S3_VERIFY_SSL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
        &CFGBLDDEF_RULE_DEPEND => CFGBLDOPT_REPO_S3_BUCKET,
    },

    &CFGBLDOPT_REPO_TYPE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_REPO_TYPE_POSIX,
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            &CFGBLDOPTVAL_REPO_TYPE_CIFS     => true,
            &CFGBLDOPTVAL_REPO_TYPE_POSIX    => true,
            &CFGBLDOPTVAL_REPO_TYPE_S3       => true,
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_SPOOL_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => '/var/spool/' . BACKREST_EXE,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_ARCHIVE_ASYNC,
            &CFGBLDDEF_RULE_DEPEND_VALUE   => true
        },
    },

    &CFGBLDOPT_PROCESS_MAX =>
    {
        &CFGBLDDEF_RULE_ALT_NAME => 'thread-max',
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 1,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [1, 96],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_RESTORE => {},
        }
    },

    # Logging options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_LOG_LEVEL_CONSOLE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(WARN),
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DETAIL) => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_LOG_LEVEL_FILE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(INFO),
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DEBUG)  => true,
            lc(DETAIL) => true,
            lc(TRACE)  => true
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_LOG_LEVEL_STDERR =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => lc(WARN),
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DETAIL) => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_LOG_TIMESTAMP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    # Archive options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_ASYNC =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        }
    },

    # Deprecated and to be removed
    &CFGBLDOPT_ARCHIVE_MAX_MB =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        }
    },

    &CFGBLDOPT_ARCHIVE_QUEUE_MAX =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        },
    },

    # Backup options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_CHECK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_ONLINE,
            &CFGBLDDEF_RULE_DEPEND_VALUE   => true,
        },
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
        },
    },

    &CFGBLDOPT_BACKUP_ARCHIVE_COPY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDDEF_RULE_DEPEND =>
                {
                    &CFGBLDDEF_RULE_DEPEND_OPTION  => CFGBLDOPT_ARCHIVE_CHECK,
                    &CFGBLDDEF_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &CFGBLDOPT_BACKUP_CMD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        },
    },

    &CFGBLDOPT_BACKUP_CONFIG =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        },
    },

    &CFGBLDOPT_BACKUP_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
    },

    &CFGBLDOPT_BACKUP_SSH_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_BACKUP_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        }
    },

    &CFGBLDOPT_BACKUP_STANDBY =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGBLDOPT_BACKUP_USER =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'backrest',
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET => {},
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_INFO => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        }
    },

    &CFGBLDOPT_CHECKSUM_PAGE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_HARDLINK =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_MANIFEST_SAVE_THRESHOLD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_DEFAULT => 1073741824,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_RESUME =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => true,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_START_FAST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_STOP_AUTO =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    # Expire options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_RETENTION_ARCHIVE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_RETENTION_MIN, CFGBLDDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    &CFGBLDOPT_RETENTION_ARCHIVE_TYPE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDOPTVAL_BACKUP_TYPE_FULL,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        },
        &CFGBLDDEF_RULE_ALLOW_LIST =>
        {
            &CFGBLDOPTVAL_BACKUP_TYPE_FULL => 1,
            &CFGBLDOPTVAL_BACKUP_TYPE_DIFF => 1,
            &CFGBLDOPTVAL_BACKUP_TYPE_INCR => 1
        }
    },

    &CFGBLDOPT_RETENTION_DIFF =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_RETENTION_MIN, CFGBLDDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    &CFGBLDOPT_RETENTION_FULL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_ALLOW_RANGE => [CFGBLDDEF_DEFAULT_RETENTION_MIN, CFGBLDDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    # Restore options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_DB_INCLUDE =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_HASH_VALUE => false,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_LINK_ALL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_BOOLEAN,
        &CFGBLDDEF_RULE_DEFAULT => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        }
    },

    &CFGBLDOPT_LINK_MAP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_TABLESPACE_MAP_ALL =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        }
    },

    &CFGBLDOPT_TABLESPACE_MAP =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_RECOVERY_OPTION =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_HASH,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
            &CFGBLDDEF_RULE_DEPEND_LIST =>
            {
                &CFGBLDOPTVAL_RESTORE_TYPE_DEFAULT  => true,
                &CFGBLDOPTVAL_RESTORE_TYPE_NAME     => true,
                &CFGBLDOPTVAL_RESTORE_TYPE_TIME     => true,
                &CFGBLDOPTVAL_RESTORE_TYPE_XID      => true
            }
        }
    },

    # Stanza options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_DB_CMD =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
        },
    },

    &CFGBLDOPT_DB_CONFIG =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_GLOBAL,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_DEFAULT => CFGBLDDEF_DEFAULT_CONFIG,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        },
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
        },
    },

    &CFGBLDOPT_DB_HOST =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_EXPIRE => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
            &CFGBLDCMD_START => {},
            &CFGBLDCMD_STOP => {},
        }
    },

    &CFGBLDOPT_DB_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => true,
        &CFGBLDDEF_RULE_HINT => "does this stanza exist?",
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_ARCHIVE_PUSH =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDDEF_RULE_REQUIRED => false
            },
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGBLDOPT_DB_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_DEFAULT => 5432,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_DB_SSH_PORT =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_INTEGER,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND => CFGBLDOPT_DB_HOST,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
        },
    },

    &CFGBLDOPT_DB_SOCKET_PATH =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_DB_USER =>
    {
        &CFGBLDDEF_RULE_SECTION => CFGBLDDEF_SECTION_STANZA,
        &CFGBLDDEF_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDDEF_RULE_TYPE => CFGBLDDEF_TYPE_STRING,
        &CFGBLDDEF_RULE_DEFAULT => 'postgres',
        &CFGBLDDEF_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
        &CFGBLDDEF_RULE_REQUIRED => false,
        &CFGBLDDEF_RULE_DEPEND =>
        {
            &CFGBLDDEF_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
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

    # Default type is string
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE}))
    {
        &log(ASSERT, "type is required for option '${strKey}'");
    }

    # Hash types by default have hash values (rather than just a boolean list)
    if (!defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_HASH_VALUE}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_HASH_VALUE} =
            $hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE} eq CFGBLDDEF_TYPE_HASH ? true : false;
    }

    # All config options can be negated.  Command-line options must be marked for negation individually.
    if ($hOptionRule{$strKey}{&CFGBLDDEF_RULE_TYPE} eq CFGBLDDEF_TYPE_BOOLEAN &&
        defined($hOptionRule{$strKey}{&CFGBLDDEF_RULE_SECTION}))
    {
        $hOptionRule{$strKey}{&CFGBLDDEF_RULE_NEGATE} = true;
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
# cfgbldCommandRule
#
# Returns the option rules based on the command.
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
# cfgbldOptionDefault
#
# Does the option have a default for this command?
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
# cfgbldOptionRange
#
# Gets the allowed setting range for the option if it exists.
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
# cfgbldOptionType
#
# Get the option type.
####################################################################################################################################
sub cfgbldOptionType
{
    my $strOption = shift;

    return $hOptionRule{$strOption}{&CFGBLDDEF_RULE_TYPE};
}

push @EXPORT, qw(cfgbldOptionType);

####################################################################################################################################
# cfgbldOptionTypeTest
#
# Test the option type.
####################################################################################################################################
sub cfgbldOptionTypeTest
{
    my $strOption = shift;
    my $strType = shift;

    return cfgbldOptionType($strOption) eq $strType;
}

push @EXPORT, qw(cfgbldOptionTypeTest);

####################################################################################################################################
# cfgbldOptionRuleHelpGet - get option rules without indexed options
####################################################################################################################################
sub cfgbldOptionRuleHelpGet
{
    return dclone(\%hOptionRule);
}

push @EXPORT, qw(cfgbldOptionRuleHelpGet);

####################################################################################################################################
# cfgbldOptionRuleGet - get option rules
####################################################################################################################################
sub cfgbldOptionRuleGet
{
    my $rhOptionRuleIndex = dclone(\%hOptionRule);

    foreach my $strKey (sort(keys(%{$rhOptionRuleIndex})))
    {
        # Build options for all possible db configurations
        if (defined($rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX}) &&
            $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX} eq CFGBLDOPT_PREFIX_DB)
        {
            my $strPrefix = $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_PREFIX};
            $rhOptionRuleIndex->{$strKey}{&CFGBLDDEF_RULE_INDEX} = CFGBLDOPTDEF_INDEX_DB;

            for (my $iIndex = 1; $iIndex <= CFGBLDOPTDEF_INDEX_DB; $iIndex++)
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

    return $rhOptionRuleIndex;
}

push @EXPORT, qw(cfgbldOptionRuleGet);

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
    $rhCommand->{&CFGBLDCMD_HELP} = true;
    $rhCommand->{&CFGBLDCMD_VERSION} = true;

    return $rhCommand;
}

push @EXPORT, qw(cfgbldCommandGet);

1;
