####################################################################################################################################
# Configuration Definition Data
#
# Contains the defines for options: which commands the option can/cannot be specified, for which commands it is required, default
# settings, types, ranges, whether the option is negatable, whether it has dependencies, etc. The initial section is the global
# section meaning the defines defined there apply to all commands listed for the option.
#
# CFGDEF_INHERIT:
#     Inherit all definitions for the referenced option.  Any definitions can be overridden.
#
# CFGDEF_COMMAND:
#     List of commands the option can be used with this option.  An empty hash signifies that the command does not deviate from the
#     option defaults.  Otherwise, overrides can be specified.
#
# NOTE: If the option (A) has a dependency on another option (B) then the CFGCMD_ must also be specified in the other option
#         (B), else it will still error on the option (A).
#
# CFGDEF_REQUIRED:
#   In global section:
#       true - if the option does not have a default, then setting CFGDEF_REQUIRED in the global section means all commands
#              listed in CFGDEF_COMMAND require the user to set it.
#       false - no commands listed require it as an option but it can be set. This can be overridden for individual commands by
#               setting CFGDEF_REQUIRED in the CFGDEF_COMMAND section.
#   In CFGDEF_COMMAND section:
#       true - the option must be set somehow for the command, either by default (CFGDEF_DEFAULT) or by the user.
# 	        &CFGCMD_CHECK =>
#             {
#                 &CFGDEF_REQUIRED => true
#             },
#       false - mainly used for overriding the CFGDEF_REQUIRED in the global section.
#
# CFGDEF_DEFAULT:
#   Sets a default for the option for all commands if listed in the global section, or for specific commands if listed in the
#   CFGDEF_COMMAND section.
#
# CFGDEF_NEGATE:
#   The option can be negated with "no" e.g. --no-compress.  This applies tp options that are only valid on the command line (i.e.
#   no config section defined).  All config options are automatically negatable.
#
# CFGDEF_RESET:
#   The option can be reset to default even if the default is undefined.
#
# CFGDEF_DEPEND:
#   Specify the dependencies this option has on another option. All commands listed for this option must also be listed in the
#   dependent option(s).
#   CFGDEF_DEPEND_LIST further defines the allowable settings for the depended option.
#
# CFGDEF_ALLOW_LIST:
#   Lists the allowable settings for the option.
#
# CFGDEF_INTERNAL:
#   Option is used by the command internally but is not exposed in the documentation.  This is useful for commands that need to know
#   where they are running by looking at other options in the config.  Also good for test options.
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
use constant CFGCMD_STANZA_DELETE                                   => 'stanza-delete';
    push @EXPORT, qw(CFGCMD_STANZA_DELETE);
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

# Perl
use constant CFGOPT_PERL_OPTION                                     => 'perl-option';
    push @EXPORT, qw(CFGOPT_PERL_OPTION);

# Logging
use constant CFGOPT_LOG_LEVEL_CONSOLE                               => 'log-level-console';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_CONSOLE);
use constant CFGOPT_LOG_LEVEL_FILE                                  => 'log-level-file';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_FILE);
use constant CFGOPT_LOG_LEVEL_STDERR                                => 'log-level-stderr';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_STDERR);
use constant CFGOPT_LOG_TIMESTAMP                                   => 'log-timestamp';
    push @EXPORT, qw(CFGOPT_LOG_TIMESTAMP);


# Repository options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many repositories can be configured
use constant CFGDEF_INDEX_REPO                                      => 1;
    push @EXPORT, qw(CFGDEF_INDEX_PG);

# Prefix that must be used by all repo options that allow multiple configurations
use constant CFGDEF_PREFIX_REPO                                     => 'repo';
    push @EXPORT, qw(CFGDEF_PREFIX_REPO);

# Repository General
use constant CFGOPT_REPO_CIPHER_TYPE                                => 'repo-cipher-type';
    push @EXPORT, qw(CFGOPT_REPO_CIPHER_TYPE);
use constant CFGOPT_REPO_CIPHER_PASS                                => 'repo-cipher-pass';
    push @EXPORT, qw(CFGOPT_REPO_CIPHER_PASS);
use constant CFGOPT_REPO_HARDLINK                                   => 'repo-hardlink';
    push @EXPORT, qw(CFGOPT_REPO_HARDLINK);
use constant CFGOPT_REPO_PATH                                       => 'repo-path';
    push @EXPORT, qw(CFGOPT_REPO_PATH);
use constant CFGOPT_REPO_TYPE                                       => 'repo-type';
    push @EXPORT, qw(CFGOPT_REPO_TYPE);

# Repository Host
use constant CFGOPT_REPO_HOST                                       => 'repo-host';
    push @EXPORT, qw(CFGOPT_REPO_HOST);
use constant CFGOPT_REPO_HOST_CMD                                   => 'repo-host-cmd';
    push @EXPORT, qw(CFGOPT_REPO_HOST_CMD);
use constant CFGOPT_REPO_HOST_CONFIG                                => 'repo-host-config';
    push @EXPORT, qw(CFGOPT_REPO_HOST_CONFIG);
use constant CFGOPT_REPO_HOST_PORT                                  => 'repo-host-port';
    push @EXPORT, qw(CFGOPT_REPO_HOST_PORT);
use constant CFGOPT_REPO_HOST_USER                                  => 'repo-host-user';
    push @EXPORT, qw(CFGOPT_REPO_HOST_USER);

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

# Archive options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_ASYNC                                   => 'archive-async';
    push @EXPORT, qw(CFGOPT_ARCHIVE_ASYNC);
use constant CFGOPT_ARCHIVE_QUEUE_MAX                               => 'archive-queue-max';
    push @EXPORT, qw(CFGOPT_ARCHIVE_QUEUE_MAX);

# Backup options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_CHECK                                   => 'archive-check';
    push @EXPORT, qw(CFGOPT_ARCHIVE_CHECK);
use constant CFGOPT_ARCHIVE_COPY                                    => 'archive-copy';
    push @EXPORT, qw(CFGOPT_ARCHIVE_COPY);
use constant CFGOPT_BACKUP_STANDBY                                  => 'backup-standby';
    push @EXPORT, qw(CFGOPT_BACKUP_STANDBY);
use constant CFGOPT_CHECKSUM_PAGE                                   => 'checksum-page';
    push @EXPORT, qw(CFGOPT_CHECKSUM_PAGE);
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
use constant CFGDEF_INDEX_PG                                        => 8;
    push @EXPORT, qw(CFGDEF_INDEX_PG);

# Prefix that must be used by all db options that allow multiple configurations
use constant CFGDEF_PREFIX_PG                                       => 'pg';
    push @EXPORT, qw(CFGDEF_PREFIX_PG);

use constant CFGOPT_PG_HOST                                         => CFGDEF_PREFIX_PG . '-host';
    push @EXPORT, qw(CFGOPT_PG_HOST);
use constant CFGOPT_PG_HOST_CMD                                     => CFGOPT_PG_HOST . '-cmd';
    push @EXPORT, qw(CFGOPT_PG_HOST_CMD);
use constant CFGOPT_PG_HOST_CONFIG                                  => CFGOPT_PG_HOST . '-config';
    push @EXPORT, qw(CFGOPT_PG_HOST_CONFIG);
use constant CFGOPT_PG_HOST_PORT                                    => CFGOPT_PG_HOST . '-port';
    push @EXPORT, qw(CFGOPT_PG_HOST_PORT);
use constant CFGOPT_PG_HOST_USER                                    => CFGOPT_PG_HOST . '-user';
    push @EXPORT, qw(CFGOPT_PG_HOST_USER);

use constant CFGOPT_PG_PATH                                         => CFGDEF_PREFIX_PG . '-path';
    push @EXPORT, qw(CFGOPT_PG_PATH);
use constant CFGOPT_PG_PORT                                         => CFGDEF_PREFIX_PG . '-port';
    push @EXPORT, qw(CFGOPT_PG_PORT);
use constant CFGOPT_PG_SOCKET_PATH                                  => CFGDEF_PREFIX_PG . '-socket-path';
    push @EXPORT, qw(CFGOPT_PG_SOCKET_PATH);

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

# Repo encryption type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_REPO_CIPHER_TYPE_NONE                    => 'none';
    push @EXPORT, qw(CFGOPTVAL_REPO_CIPHER_TYPE_NONE);
use constant CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC             => 'aes-256-cbc';
    push @EXPORT, qw(CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC);

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

use constant CFGDEF_DEFAULT_PROTOCOL_PORT_MIN                       => 0;
use constant CFGDEF_DEFAULT_PROTOCOL_PORT_MAX                       => 65535;

use constant CFGDEF_DEFAULT_RETENTION_MIN                           => 1;
use constant CFGDEF_DEFAULT_RETENTION_MAX                           => 9999999;

####################################################################################################################################
# Option definition constants - defines, types, sections, etc.
####################################################################################################################################

# Option defines
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_ALLOW_LIST                                      => 'allow-list';
    push @EXPORT, qw(CFGDEF_ALLOW_LIST);
use constant CFGDEF_ALLOW_RANGE                                     => 'allow-range';
    push @EXPORT, qw(CFGDEF_ALLOW_RANGE);
use constant CFGDEF_DEFAULT                                         => 'default';
    push @EXPORT, qw(CFGDEF_DEFAULT);
use constant CFGDEF_DEPEND                                          => 'depend';
    push @EXPORT, qw(CFGDEF_DEPEND);
use constant CFGDEF_DEPEND_OPTION                                   => 'depend-option';
    push @EXPORT, qw(CFGDEF_DEPEND_OPTION);
use constant CFGDEF_DEPEND_LIST                                     => 'depend-list';
    push @EXPORT, qw(CFGDEF_DEPEND_LIST);
use constant CFGDEF_INDEX                                           => 'index';
    push @EXPORT, qw(CFGDEF_INDEX);
use constant CFGDEF_INDEX_TOTAL                                     => 'indexTotal';
    push @EXPORT, qw(CFGDEF_INDEX_TOTAL);
use constant CFGDEF_INHERIT                                         => 'inherit';
    push @EXPORT, qw(CFGDEF_INHERIT);
use constant CFGDEF_INTERNAL                                        => 'internal';
    push @EXPORT, qw(CFGDEF_INTERNAL);
use constant CFGDEF_NAME_ALT                                        => 'name-alt';
    push @EXPORT, qw(CFGDEF_NAME_ALT);
use constant CFGDEF_NEGATE                                          => 'negate';
    push @EXPORT, qw(CFGDEF_NEGATE);
use constant CFGDEF_PREFIX                                          => 'prefix';
    push @EXPORT, qw(CFGDEF_PREFIX);
use constant CFGDEF_COMMAND                                         => 'command';
    push @EXPORT, qw(CFGDEF_COMMAND);
use constant CFGDEF_REQUIRED                                        => 'required';
    push @EXPORT, qw(CFGDEF_REQUIRED);
use constant CFGDEF_RESET                                           => 'reset';
    push @EXPORT, qw(CFGDEF_RESET);
use constant CFGDEF_SECTION                                         => 'section';
    push @EXPORT, qw(CFGDEF_SECTION);
use constant CFGDEF_SECURE                                          => 'secure';
    push @EXPORT, qw(CFGDEF_SECURE);
use constant CFGDEF_TYPE                                            => 'type';
    push @EXPORT, qw(CFGDEF_TYPE);

# Option types
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_TYPE_BOOLEAN                                    => 'boolean';
    push @EXPORT, qw(CFGDEF_TYPE_BOOLEAN);
use constant CFGDEF_TYPE_FLOAT                                      => 'float';
    push @EXPORT, qw(CFGDEF_TYPE_FLOAT);
use constant CFGDEF_TYPE_HASH                                       => 'hash';
    push @EXPORT, qw(CFGDEF_TYPE_HASH);
use constant CFGDEF_TYPE_INTEGER                                    => 'integer';
    push @EXPORT, qw(CFGDEF_TYPE_INTEGER);
use constant CFGDEF_TYPE_LIST                                       => 'list';
    push @EXPORT, qw(CFGDEF_TYPE_LIST);
use constant CFGDEF_TYPE_STRING                                     => 'string';
    push @EXPORT, qw(CFGDEF_TYPE_STRING);

# Option config sections
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CFGDEF_SECTION_GLOBAL);
use constant CFGDEF_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CFGDEF_SECTION_STANZA);

####################################################################################################################################
# Option definition data
####################################################################################################################################
my %hConfigDefine =
(
    # Command-line only options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_CONFIG =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGDEF_NEGATE => true,
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_DELTA =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => false,
            }
        }
    },

    &CFGOPT_FORCE =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGDEF_DEFAULT => false,
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_ONLINE,
                    &CFGDEF_DEPEND_LIST => [false],
                },
            },

            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => false,
            },

            &CFGCMD_STANZA_CREATE =>
            {
                &CFGDEF_DEFAULT => false,
            },

            &CFGCMD_STANZA_DELETE =>
            {
                &CFGDEF_DEFAULT => false,
            },

            &CFGCMD_STOP =>
            {
                &CFGDEF_DEFAULT => false
            }
        }
    },

    &CFGOPT_ONLINE =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_NEGATE => true,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_SET =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => 'latest',
            }
        }
    },

    &CFGOPT_STANZA =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_STOP =>
            {
                &CFGDEF_REQUIRED => false
            }
        }
    },

    &CFGOPT_TARGET =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGDEF_DEPEND_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => false,
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGDEF_DEPEND_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE,

                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE,
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE,
                    &CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN,
                ],

                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGDEF_DEPEND_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_REQUIRED => false,
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_TYPE,
                    &CFGDEF_DEPEND_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGDEF_DEFAULT => CFGOPTVAL_BACKUP_TYPE_INCR,
                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_BACKUP_TYPE_FULL,
                    &CFGOPTVAL_BACKUP_TYPE_DIFF,
                    &CFGOPTVAL_BACKUP_TYPE_INCR,
                ]
            },

            &CFGCMD_LOCAL =>
            {
                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_LOCAL_TYPE_DB,
                    &CFGOPTVAL_LOCAL_TYPE_BACKUP,
                ],
            },

            &CFGCMD_REMOTE =>
            {
                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_REMOTE_TYPE_DB,
                    &CFGOPTVAL_REMOTE_TYPE_BACKUP,
                ],
            },

            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => CFGOPTVAL_RESTORE_TYPE_DEFAULT,
                &CFGDEF_ALLOW_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_INFO =>
            {
                &CFGDEF_DEFAULT => CFGOPTVAL_INFO_OUTPUT_TEXT,
                &CFGDEF_ALLOW_LIST =>
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
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
        }
    },

    &CFGOPT_HOST_ID =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_ALLOW_RANGE => [1, CFGDEF_INDEX_PG > CFGDEF_INDEX_REPO ? CFGDEF_INDEX_PG : CFGDEF_INDEX_REPO],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_LOCAL => {},
        },
    },

    &CFGOPT_PROCESS =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_ALLOW_RANGE => [0, 1024],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_LOCAL =>
            {
                &CFGDEF_REQUIRED => true,
            },
            &CFGCMD_REMOTE =>
            {
                &CFGDEF_REQUIRED => false,
            },
        },
    },

    # Command-line only test options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_TEST =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_TEST_DELAY =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_FLOAT,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_DEFAULT => 5,
        &CFGDEF_ALLOW_RANGE => [.1, 60],
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_TEST,
            &CFGDEF_DEPEND_LIST => [true],
        },
        &CFGDEF_COMMAND => CFGOPT_TEST,
    },

    &CFGOPT_TEST_POINT =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_HASH,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_DEPEND => CFGOPT_TEST_DELAY,
        &CFGDEF_COMMAND => CFGOPT_TEST,
    },

    # General options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_TIMEOUT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_FLOAT,
        &CFGDEF_DEFAULT => 60,
        &CFGDEF_ALLOW_RANGE => [WAIT_TIME_MINIMUM, 86400],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
        },
    },

    &CFGOPT_BUFFER_SIZE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => COMMON_IO_BUFFER_MAX,
        &CFGDEF_ALLOW_LIST =>
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
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_DB_TIMEOUT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_FLOAT,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_DB_TIMEOUT,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_DB_TIMEOUT_MIN, CFGDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_COMPRESS =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_COMPRESS_LEVEL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => 6,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_COMPRESS_LEVEL_NETWORK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => 3,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_NEUTRAL_UMASK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_CMD_SSH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => 'ssh',
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_LOCK_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => '/tmp/' . BACKREST_EXE,
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_LOG_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => '/var/log/' . BACKREST_EXE,
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_PERL_OPTION =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_LIST,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_COMMAND =>
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
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_FLOAT,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_DB_TIMEOUT + 30,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_DB_TIMEOUT_MIN, CFGDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    # Repository options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_REPO_CIPHER_PASS =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_SECURE => true,
        &CFGDEF_REQUIRED  => false,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION  => CFGOPT_REPO_CIPHER_TYPE,
            &CFGDEF_DEPEND_LIST => [CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC],
        },
        &CFGDEF_NAME_ALT =>
        {
            'repo-cipher-pass' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_CIPHER_TYPE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => CFGOPTVAL_REPO_CIPHER_TYPE_NONE,
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_REPO_CIPHER_TYPE_NONE,
            &CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC,
        ],
        &CFGDEF_NAME_ALT =>
        {
            'repo-cipher-type' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_HARDLINK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_NAME_ALT =>
        {
            'hardlink' => {},
        },
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        },
    },

    &CFGOPT_REPO_HOST =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'backup-host' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_STANZA_DELETE =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_STANZA_UPGRADE =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_REPO_HOST_CMD =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'backup-cmd' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_HOST
        },
    },

    &CFGOPT_REPO_HOST_CONFIG =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGDEF_NAME_ALT =>
        {
            'backup-config' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_HOST_CMD,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_HOST
        },
    },

    &CFGOPT_REPO_HOST_PORT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_PROTOCOL_PORT_MIN, CFGDEF_DEFAULT_PROTOCOL_PORT_MAX],
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'backup-ssh-port' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_HOST_CMD,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_HOST
        }
    },

    &CFGOPT_REPO_HOST_USER =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => 'pgbackrest',
        &CFGDEF_NAME_ALT =>
        {
            'backup-user' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_HOST_CMD,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_HOST
        }
    },

    &CFGOPT_REPO_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => '/var/lib/' . BACKREST_EXE,
        &CFGDEF_NAME_ALT =>
        {
            'repo-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_BUCKET =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_TYPE,
            &CFGDEF_DEPEND_LIST => [CFGOPTVAL_REPO_TYPE_S3],
        },
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-bucket' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_CA_FILE =>
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_HOST,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-ca-file' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
    },

    &CFGOPT_REPO_S3_CA_PATH =>
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_HOST,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-ca-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
    },

    &CFGOPT_REPO_S3_KEY =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_SECURE => true,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_TYPE,
            &CFGDEF_DEPEND_LIST => [CFGOPTVAL_REPO_TYPE_S3],
        },
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-key' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_KEY_SECRET =>
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_KEY,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-key-secret' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
    },

    &CFGOPT_REPO_S3_ENDPOINT =>
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_BUCKET,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-endpoint' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
    },

    &CFGOPT_REPO_S3_HOST =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED  => false,
        &CFGDEF_DEPEND => CFGOPT_REPO_S3_BUCKET,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-host' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_REGION,
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_BUCKET,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-region' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
    },

    &CFGOPT_REPO_S3_VERIFY_SSL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-verify-ssl' => {&CFGDEF_INDEX => 1},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
        &CFGDEF_DEPEND => CFGOPT_REPO_S3_BUCKET,
    },

    &CFGOPT_REPO_TYPE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => CFGOPTVAL_REPO_TYPE_POSIX,
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_REPO_TYPE_CIFS,
            &CFGOPTVAL_REPO_TYPE_POSIX,
            &CFGOPTVAL_REPO_TYPE_S3,
        ],
        &CFGDEF_NAME_ALT =>
        {
            'repo-type' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_SPOOL_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => '/var/spool/' . BACKREST_EXE,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_ARCHIVE_ASYNC,
            &CFGDEF_DEPEND_LIST => [true],
        },
    },

    &CFGOPT_PROCESS_MAX =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => 1,
        &CFGDEF_ALLOW_RANGE => [1, 96],
        &CFGDEF_COMMAND =>
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
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => lc(WARN),
        &CFGDEF_ALLOW_LIST =>
        [
            lc(OFF),
            lc(ERROR),
            lc(WARN),
            lc(INFO),
            lc(DETAIL),
            lc(DEBUG),
            lc(TRACE),
        ],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_LOG_LEVEL_FILE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => lc(INFO),
        &CFGDEF_ALLOW_LIST => CFGOPT_LOG_LEVEL_CONSOLE,
        &CFGDEF_COMMAND => CFGOPT_LOG_LEVEL_CONSOLE,
    },

    &CFGOPT_LOG_LEVEL_STDERR =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => lc(WARN),
        &CFGDEF_ALLOW_LIST => CFGOPT_LOG_LEVEL_CONSOLE,
        &CFGDEF_COMMAND =>
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
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_LOG_TIMESTAMP =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND => CFGOPT_LOG_LEVEL_CONSOLE,
    },

    # Archive options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_ASYNC =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        }
    },

    &CFGOPT_ARCHIVE_QUEUE_MAX =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [0, 4 * 1024 * 1024 * 1024 * 1024 * 1024], # 0-4PiB
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        },
    },

    # Backup options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_CHECK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_ONLINE,
            &CFGDEF_DEPEND_LIST => [true],
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
        },
    },

    &CFGOPT_ARCHIVE_COPY =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_ARCHIVE_CHECK,
                    &CFGDEF_DEPEND_LIST => [true],
                }
            }
        }
    },

    &CFGOPT_BACKUP_STANDBY =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGOPT_CHECKSUM_PAGE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_MANIFEST_SAVE_THRESHOLD =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => 1 * 1024 * 1024 * 1024,
        &CFGDEF_ALLOW_RANGE => [1, 1024 * 1024 * 1024 * 1024],      # 1-1TB
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_RESUME =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_START_FAST =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_STOP_AUTO =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        }
    },

    # Expire options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_RETENTION_ARCHIVE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_RETENTION_ARCHIVE_TYPE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => CFGOPTVAL_BACKUP_TYPE_FULL,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        },
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_BACKUP_TYPE_FULL,
            &CFGOPTVAL_BACKUP_TYPE_DIFF,
            &CFGOPTVAL_BACKUP_TYPE_INCR,
        ]
    },

    &CFGOPT_RETENTION_DIFF =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_RETENTION_FULL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    # Restore options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_DB_INCLUDE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_LIST,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_LINK_ALL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_LINK_MAP =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_HASH,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_TABLESPACE_MAP_ALL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        }
    },

    &CFGOPT_TABLESPACE_MAP =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_HASH,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_RECOVERY_OPTION =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_HASH,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_RESTORE => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_TYPE,
            &CFGDEF_DEPEND_LIST =>
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
    &CFGOPT_PG_HOST =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_STANZA,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'db-host' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-host' => {&CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_ARCHIVE_PUSH =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_INTERNAL => true,
            },
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_PG_HOST_CMD =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_STANZA,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'db-cmd' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-cmd' => {&CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_PG_HOST
        },
    },

    &CFGOPT_PG_HOST_CONFIG =>
    {
        &CFGDEF_INHERIT => CFGOPT_PG_HOST_CMD,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG,
        &CFGDEF_REQUIRED => true,
        &CFGDEF_NAME_ALT =>
        {
            'db-config' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-config' => {&CFGDEF_RESET => false},
        },
    },

    &CFGOPT_PG_HOST_PORT =>
    {
        &CFGDEF_INHERIT => CFGOPT_PG_HOST_CMD,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_PROTOCOL_PORT_MIN, CFGDEF_DEFAULT_PROTOCOL_PORT_MAX],
        &CFGDEF_NAME_ALT =>
        {
            'db-ssh-port' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-ssh-port' => {&CFGDEF_RESET => false},
        },
    },

    &CFGOPT_PG_HOST_USER =>
    {
        &CFGDEF_INHERIT => CFGOPT_PG_HOST_CMD,
        &CFGDEF_DEFAULT => 'postgres',
        &CFGDEF_NAME_ALT =>
        {
            'db-user' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-user' => {&CFGDEF_RESET => false},
        },
        &CFGDEF_REQUIRED => false,
    },

    &CFGOPT_PG_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_STANZA,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_REQUIRED => true,
        &CFGDEF_NAME_ALT =>
        {
            'db-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-path' => {&CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_ARCHIVE_PUSH =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_REMOTE =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGOPT_PG_PORT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_STANZA,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_DEFAULT => 5432,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_PROTOCOL_PORT_MIN, CFGDEF_DEFAULT_PROTOCOL_PORT_MAX],
        &CFGDEF_NAME_ALT =>
        {
            'db-port' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-port' => {&CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_LOCAL => {},
            &CFGCMD_REMOTE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_PG_PATH
        },
    },

    &CFGOPT_PG_SOCKET_PATH =>
    {
        &CFGDEF_INHERIT => CFGOPT_PG_PORT,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => undef,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'db-socket-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-socket-path' => {&CFGDEF_RESET => false},
        },
    },
);

####################################################################################################################################
# Process define defaults
####################################################################################################################################
foreach my $strKey (sort(keys(%hConfigDefine)))
{
    # If the define is a scalar then copy the entire define from the referenced option
    if (defined($hConfigDefine{$strKey}{&CFGDEF_INHERIT}))
    {
        # Make a copy in case there are overrides that need to be applied after inheriting
        my $hConfigDefineOverride = dclone($hConfigDefine{$strKey});

        # Copy the option being inherited from
        $hConfigDefine{$strKey} = dclone($hConfigDefine{$hConfigDefine{$strKey}{&CFGDEF_INHERIT}});

        # Apply overrides
        foreach my $strOptionDef (sort(keys(%{$hConfigDefineOverride})))
        {
            $hConfigDefine{$strKey}{$strOptionDef} = $hConfigDefineOverride->{$strOptionDef};
        }
    }

    # If the command section is a scalar then copy the section from the referenced option
    if (defined($hConfigDefine{$strKey}{&CFGDEF_COMMAND}) && !ref($hConfigDefine{$strKey}{&CFGDEF_COMMAND}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_COMMAND} =
            dclone($hConfigDefine{$hConfigDefine{$strKey}{&CFGDEF_COMMAND}}{&CFGDEF_COMMAND});
    }

    # If the required section is a scalar then copy the section from the referenced option
    if (defined($hConfigDefine{$strKey}{&CFGDEF_DEPEND}) && !ref($hConfigDefine{$strKey}{&CFGDEF_DEPEND}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_DEPEND} =
            dclone($hConfigDefine{$hConfigDefine{$strKey}{&CFGDEF_DEPEND}}{&CFGDEF_DEPEND});
    }

    # If the allow list is a scalar then copy the list from the referenced option
    if (defined($hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST}) && !ref($hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST} =
            dclone($hConfigDefine{$hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST}}{&CFGDEF_ALLOW_LIST});
    }

    # Default type is string
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_TYPE}))
    {
        &log(ASSERT, "type is required for option '${strKey}'");
    }

    # Default required is true
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_REQUIRED}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_REQUIRED} = true;
    }

    # Default internal is false
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_INTERNAL}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_INTERNAL} = false;
    }

    # Set index total for pg-*
    if (defined($hConfigDefine{$strKey}{&CFGDEF_PREFIX}) &&
        $hConfigDefine{$strKey}{&CFGDEF_PREFIX} eq CFGDEF_PREFIX_PG)
    {
        $hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL} = CFGDEF_INDEX_PG;
    }
    # Else default index total is 1
    else
    {
        $hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL} = 1;
    }

    # All boolean config options can be negated.  Boolean command-line options must be marked for negation individually.
    if ($hConfigDefine{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_BOOLEAN && defined($hConfigDefine{$strKey}{&CFGDEF_SECTION}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_NEGATE} = true;
    }

    # Default for negation is false
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_NEGATE}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_NEGATE} = false;
    }

    # All config options can be reset
    if (defined($hConfigDefine{$strKey}{&CFGDEF_SECTION}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_RESET} = true;
    }
    elsif (!defined($hConfigDefine{$strKey}{&CFGDEF_RESET}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_RESET} = false;
    }

    # By default options are not secure
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_SECURE}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_SECURE} = false;
    }

    # Set all indices to 1 by default - this defines how many copies of any option there can be
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL}))
    {
        $hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL} = 1;
    }

    # All int and float options must have an allow range
    if (($hConfigDefine{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_INTEGER ||
         $hConfigDefine{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_FLOAT) &&
         !(defined($hConfigDefine{$strKey}{&CFGDEF_ALLOW_RANGE}) || defined($hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST})))
    {
        confess &log(ASSERT, "int/float option '${strKey}' must have allow range or list");
    }
}

####################################################################################################################################
# Get configuration definition
####################################################################################################################################
sub cfgDefine
{
    return dclone(\%hConfigDefine);
}

push @EXPORT, qw(cfgDefine);


####################################################################################################################################
# Get list of all commands
####################################################################################################################################
sub cfgDefineCommandList
{
    my $rhCommandMap;

    # Get unique list of commands
    foreach my $strOption (sort(keys(%hConfigDefine)))
    {
        foreach my $strCommand (sort(keys(%{$hConfigDefine{$strOption}{&CFGDEF_COMMAND}})))
        {
            $rhCommandMap->{$strCommand} = true;
        }
    }

    # Add special commands
    $rhCommandMap->{&CFGCMD_HELP} = true;
    $rhCommandMap->{&CFGCMD_VERSION} = true;

    # Return sorted list
    return (sort(keys(%{$rhCommandMap})));
}

push @EXPORT, qw(cfgDefineCommandList);

####################################################################################################################################
# Get list of all option types
####################################################################################################################################
sub cfgDefineOptionTypeList
{
    my $rhOptionTypeMap;

    # Get unique list of types
    foreach my $strOption (sort(keys(%hConfigDefine)))
    {
        my $strOptionType = $hConfigDefine{$strOption}{&CFGDEF_TYPE};

        if (!defined($rhOptionTypeMap->{$strOptionType}))
        {
            $rhOptionTypeMap->{$strOptionType} = true;
        }
    };

    # Return sorted list
    return (sort(keys(%{$rhOptionTypeMap})));
}

push @EXPORT, qw(cfgDefineOptionTypeList);

1;
