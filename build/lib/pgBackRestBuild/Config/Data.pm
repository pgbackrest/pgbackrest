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
#           &CFGCMD_CHECK =>
#           {
#               &CFGDEF_REQUIRED => true
#           },
#       false - mainly used for overriding the CFGDEF_REQUIRED in the global section.
#
# CFGDEF_DEFAULT:
#   Sets a default for the option for all commands if listed in the global section, or for specific commands if listed in the
#   CFGDEF_COMMAND section. All boolean types require a default.
#
# CFGDEF_NEGATE:
#   The option can be negated with "no" e.g. --no-compress.  This applies to options that are only valid on the command line (i.e.
#   no config section defined) and if not specifically defined, the default is false.  All config file boolean options are
#   automatically negatable.
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

use pgBackRestDoc::Common::Exception;
use pgBackRestDoc::Common::Log;
use pgBackRestDoc::ProjectInfo;

use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Command constants - commands that are allowed in the exe
####################################################################################################################################
use constant CFGCMD_ARCHIVE_GET                                     => 'archive-get';
use constant CFGCMD_ARCHIVE_PUSH                                    => 'archive-push';
use constant CFGCMD_BACKUP                                          => 'backup';
    push @EXPORT, qw(CFGCMD_BACKUP);
use constant CFGCMD_CHECK                                           => 'check';
use constant CFGCMD_EXPIRE                                          => 'expire';
use constant CFGCMD_HELP                                            => 'help';
    push @EXPORT, qw(CFGCMD_HELP);
use constant CFGCMD_INFO                                            => 'info';
    push @EXPORT, qw(CFGCMD_INFO);
use constant CFGCMD_REPO_CREATE                                     => 'repo-create';
use constant CFGCMD_REPO_GET                                        => 'repo-get';
use constant CFGCMD_REPO_LS                                         => 'repo-ls';
use constant CFGCMD_REPO_PUT                                        => 'repo-put';
use constant CFGCMD_REPO_RM                                         => 'repo-rm';
use constant CFGCMD_RESTORE                                         => 'restore';
use constant CFGCMD_STANZA_CREATE                                   => 'stanza-create';
use constant CFGCMD_STANZA_DELETE                                   => 'stanza-delete';
use constant CFGCMD_STANZA_UPGRADE                                  => 'stanza-upgrade';
use constant CFGCMD_START                                           => 'start';
use constant CFGCMD_STOP                                            => 'stop';
use constant CFGCMD_VERSION                                         => 'version';

####################################################################################################################################
# Option constants - options that are allowed for commands
####################################################################################################################################

# Command-line only options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_CONFIG                                          => 'config';
    push @EXPORT, qw(CFGOPT_CONFIG);
use constant CFGOPT_CONFIG_PATH                                     => 'config-path';
use constant CFGOPT_CONFIG_INCLUDE_PATH                             => 'config-include-path';
use constant CFGOPT_DELTA                                           => 'delta';
use constant CFGOPT_DRYRUN                                          => 'dry-run';
use constant CFGOPT_FORCE                                           => 'force';
use constant CFGOPT_ONLINE                                          => 'online';
use constant CFGOPT_SET                                             => 'set';
use constant CFGOPT_STANZA                                          => 'stanza';
    push @EXPORT, qw(CFGOPT_STANZA);
use constant CFGOPT_TARGET                                          => 'target';
use constant CFGOPT_TARGET_EXCLUSIVE                                => 'target-exclusive';
use constant CFGOPT_TARGET_ACTION                                   => 'target-action';
use constant CFGOPT_TARGET_TIMELINE                                 => 'target-timeline';
use constant CFGOPT_TYPE                                            => 'type';
use constant CFGOPT_OUTPUT                                          => 'output';

# Command-line only local/remote options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_PROCESS                                         => 'process';
use constant CFGOPT_HOST_ID                                         => 'host-id';
use constant CFGOPT_REMOTE_TYPE                                     => 'remote-type';

# Command-line only storage options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_CIPHER_PASS                                     => 'cipher-pass';
use constant CFGOPT_FILTER                                          => 'filter';
use constant CFGOPT_IGNORE_MISSING                                  => 'ignore-missing';
use constant CFGOPT_RAW                                             => 'raw';
use constant CFGOPT_RECURSE                                         => 'recurse';
use constant CFGOPT_SORT                                            => 'sort';

# General options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_TIMEOUT                                 => 'archive-timeout';
use constant CFGOPT_BUFFER_SIZE                                     => 'buffer-size';
use constant CFGOPT_DB_TIMEOUT                                      => 'db-timeout';
use constant CFGOPT_COMPRESS                                        => 'compress';
use constant CFGOPT_COMPRESS_TYPE                                   => 'compress-type';
use constant CFGOPT_COMPRESS_LEVEL                                  => 'compress-level';
use constant CFGOPT_COMPRESS_LEVEL_NETWORK                          => 'compress-level-network';
use constant CFGOPT_IO_TIMEOUT                                      => 'io-timeout';
use constant CFGOPT_NEUTRAL_UMASK                                   => 'neutral-umask';
use constant CFGOPT_PROTOCOL_TIMEOUT                                => 'protocol-timeout';
use constant CFGOPT_PROCESS_MAX                                     => 'process-max';
use constant CFGOPT_SCK_BLOCK                                       => 'sck-block';
use constant CFGOPT_SCK_KEEP_ALIVE                                  => 'sck-keep-alive';
use constant CFGOPT_TCP_KEEP_ALIVE_COUNT                            => 'tcp-keep-alive-count';
use constant CFGOPT_TCP_KEEP_ALIVE_IDLE                             => 'tcp-keep-alive-idle';
use constant CFGOPT_TCP_KEEP_ALIVE_INTERVAL                         => 'tcp-keep-alive-interval';

# Commands
use constant CFGOPT_CMD_SSH                                         => 'cmd-ssh';

# Paths
use constant CFGOPT_LOCK_PATH                                       => 'lock-path';
    push @EXPORT, qw(CFGOPT_LOCK_PATH);
use constant CFGOPT_LOG_PATH                                        => 'log-path';
    push @EXPORT, qw(CFGOPT_LOG_PATH);
use constant CFGOPT_SPOOL_PATH                                      => 'spool-path';
    push @EXPORT, qw(CFGOPT_SPOOL_PATH);

# Logging
use constant CFGOPT_LOG_LEVEL_CONSOLE                               => 'log-level-console';
use constant CFGOPT_LOG_LEVEL_FILE                                  => 'log-level-file';
use constant CFGOPT_LOG_LEVEL_STDERR                                => 'log-level-stderr';
    push @EXPORT, qw(CFGOPT_LOG_LEVEL_STDERR);
use constant CFGOPT_LOG_SUBPROCESS                                  => 'log-subprocess';
use constant CFGOPT_LOG_TIMESTAMP                                   => 'log-timestamp';
    push @EXPORT, qw(CFGOPT_LOG_TIMESTAMP);

# Repository options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many repositories can be configured
use constant CFGDEF_INDEX_REPO                                      => 1;

# Prefix that must be used by all repo options that allow multiple configurations
use constant CFGDEF_PREFIX_REPO                                     => 'repo';

# Repository General
use constant CFGOPT_REPO_CIPHER_TYPE                                => CFGDEF_PREFIX_REPO . '-cipher-type';
use constant CFGOPT_REPO_CIPHER_PASS                                => CFGDEF_PREFIX_REPO . '-cipher-pass';
use constant CFGOPT_REPO_HARDLINK                                   => CFGDEF_PREFIX_REPO . '-hardlink';
use constant CFGOPT_REPO_PATH                                       => CFGDEF_PREFIX_REPO . '-path';
    push @EXPORT, qw(CFGOPT_REPO_PATH);
use constant CFGOPT_REPO_TYPE                                       => CFGDEF_PREFIX_REPO . '-type';

# Repository Retention
use constant CFGOPT_REPO_RETENTION_ARCHIVE                          => CFGDEF_PREFIX_REPO . '-retention-archive';
use constant CFGOPT_REPO_RETENTION_ARCHIVE_TYPE                     => CFGDEF_PREFIX_REPO . '-retention-archive-type';
use constant CFGOPT_REPO_RETENTION_DIFF                             => CFGDEF_PREFIX_REPO . '-retention-diff';
use constant CFGOPT_REPO_RETENTION_FULL                             => CFGDEF_PREFIX_REPO . '-retention-full';

# Repository Host
use constant CFGOPT_REPO_HOST                                       => CFGDEF_PREFIX_REPO . '-host';
use constant CFGOPT_REPO_HOST_CMD                                   => CFGOPT_REPO_HOST . '-cmd';
    push @EXPORT, qw(CFGOPT_REPO_HOST_CMD);
use constant CFGOPT_REPO_HOST_CONFIG                                => CFGOPT_REPO_HOST . '-config';
use constant CFGOPT_REPO_HOST_CONFIG_INCLUDE_PATH                   => CFGOPT_REPO_HOST_CONFIG . '-include-path';
use constant CFGOPT_REPO_HOST_CONFIG_PATH                           => CFGOPT_REPO_HOST_CONFIG . '-path';
use constant CFGOPT_REPO_HOST_PORT                                  => CFGOPT_REPO_HOST . '-port';
use constant CFGOPT_REPO_HOST_USER                                  => CFGOPT_REPO_HOST . '-user';

# Repository S3
use constant CFGDEF_REPO_S3                                         => CFGDEF_PREFIX_REPO . '-s3';
use constant CFGOPT_REPO_S3_KEY                                     => CFGDEF_REPO_S3 . '-key';
use constant CFGOPT_REPO_S3_KEY_SECRET                              => CFGDEF_REPO_S3 . '-key-secret';
use constant CFGOPT_REPO_S3_BUCKET                                  => CFGDEF_REPO_S3 . '-bucket';
use constant CFGOPT_REPO_S3_CA_FILE                                 => CFGDEF_REPO_S3 . '-ca-file';
use constant CFGOPT_REPO_S3_CA_PATH                                 => CFGDEF_REPO_S3 . '-ca-path';
use constant CFGOPT_REPO_S3_ENDPOINT                                => CFGDEF_REPO_S3 . '-endpoint';
use constant CFGOPT_REPO_S3_HOST                                    => CFGDEF_REPO_S3 . '-host';
use constant CFGOPT_REPO_S3_PORT                                    => CFGDEF_REPO_S3 . '-port';
use constant CFGOPT_REPO_S3_REGION                                  => CFGDEF_REPO_S3 . '-region';
use constant CFGOPT_REPO_S3_TOKEN                                   => CFGDEF_REPO_S3 . '-token';
use constant CFGOPT_REPO_S3_URI_STYLE                               => CFGDEF_REPO_S3 . '-uri-style';
use constant CFGOPT_REPO_S3_VERIFY_TLS                              => CFGDEF_REPO_S3 . '-verify-tls';

# Archive options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_ASYNC                                   => 'archive-async';
use constant CFGOPT_ARCHIVE_GET_QUEUE_MAX                           => 'archive-get-queue-max';
use constant CFGOPT_ARCHIVE_PUSH_QUEUE_MAX                          => 'archive-push-queue-max';

# Backup options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_ARCHIVE_CHECK                                   => 'archive-check';
use constant CFGOPT_ARCHIVE_COPY                                    => 'archive-copy';
use constant CFGOPT_BACKUP_STANDBY                                  => 'backup-standby';
use constant CFGOPT_CHECKSUM_PAGE                                   => 'checksum-page';
use constant CFGOPT_EXCLUDE                                         => 'exclude';
use constant CFGOPT_MANIFEST_SAVE_THRESHOLD                         => 'manifest-save-threshold';
use constant CFGOPT_RESUME                                          => 'resume';
use constant CFGOPT_START_FAST                                      => 'start-fast';
use constant CFGOPT_STOP_AUTO                                       => 'stop-auto';

# Restore options
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPT_DB_INCLUDE                                      => 'db-include';
use constant CFGOPT_LINK_ALL                                        => 'link-all';
use constant CFGOPT_LINK_MAP                                        => 'link-map';
use constant CFGOPT_TABLESPACE_MAP_ALL                              => 'tablespace-map-all';
use constant CFGOPT_TABLESPACE_MAP                                  => 'tablespace-map';
use constant CFGOPT_RECOVERY_OPTION                                 => 'recovery-option';

# Stanza options
#-----------------------------------------------------------------------------------------------------------------------------------
# Determines how many databases can be configured
use constant CFGDEF_INDEX_PG                                        => 8;
    push @EXPORT, qw(CFGDEF_INDEX_PG);

# Prefix that must be used by all db options that allow multiple configurations
use constant CFGDEF_PREFIX_PG                                       => 'pg';
    push @EXPORT, qw(CFGDEF_PREFIX_PG);

use constant CFGOPT_PG_HOST                                         => CFGDEF_PREFIX_PG . '-host';
use constant CFGOPT_PG_HOST_CMD                                     => CFGOPT_PG_HOST . '-cmd';
    push @EXPORT, qw(CFGOPT_PG_HOST_CMD);
use constant CFGOPT_PG_HOST_CONFIG                                  => CFGOPT_PG_HOST . '-config';
use constant CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH                     => CFGOPT_PG_HOST_CONFIG . '-include-path';
use constant CFGOPT_PG_HOST_CONFIG_PATH                             => CFGOPT_PG_HOST_CONFIG . '-path';
use constant CFGOPT_PG_HOST_PORT                                    => CFGOPT_PG_HOST . '-port';
use constant CFGOPT_PG_HOST_USER                                    => CFGOPT_PG_HOST . '-user';

use constant CFGOPT_PG_PATH                                         => CFGDEF_PREFIX_PG . '-path';
use constant CFGOPT_PG_PORT                                         => CFGDEF_PREFIX_PG . '-port';
use constant CFGOPT_PG_SOCKET_PATH                                  => CFGDEF_PREFIX_PG . '-socket-path';
use constant CFGOPT_PG_USER                                         => CFGDEF_PREFIX_PG . '-user';

####################################################################################################################################
# Option values - for options that have a specific list of allowed values
####################################################################################################################################

# Storage types
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_STORAGE_TYPE_PG                              => 'pg';
use constant CFGOPTVAL_STORAGE_TYPE_REPO                            => 'repo';

# Backup type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_BACKUP_TYPE_FULL                             => 'full';
use constant CFGOPTVAL_BACKUP_TYPE_DIFF                             => 'diff';
use constant CFGOPTVAL_BACKUP_TYPE_INCR                             => 'incr';

# Repo type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_REPO_TYPE_CIFS                               => 'cifs';
use constant CFGOPTVAL_REPO_TYPE_POSIX                              => 'posix';
use constant CFGOPTVAL_REPO_TYPE_S3                                 => 's3';

# Repo encryption type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_REPO_CIPHER_TYPE_NONE                        => 'none';
use constant CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC                 => 'aes-256-cbc';

# Repo S3 URI style
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_REPO_S3_URI_STYLE_HOST                       => 'host';
use constant CFGOPTVAL_REPO_S3_URI_STYLE_PATH                       => 'path';

# Info output
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_OUTPUT_TEXT                                  => 'text';
use constant CFGOPTVAL_OUTPUT_JSON                                  => 'json';

# Restore type
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_RESTORE_TYPE_NAME                            => 'name';
use constant CFGOPTVAL_RESTORE_TYPE_TIME                            => 'time';
use constant CFGOPTVAL_RESTORE_TYPE_XID                             => 'xid';
use constant CFGOPTVAL_RESTORE_TYPE_PRESERVE                        => 'preserve';
use constant CFGOPTVAL_RESTORE_TYPE_NONE                            => 'none';
use constant CFGOPTVAL_RESTORE_TYPE_IMMEDIATE                       => 'immediate';
use constant CFGOPTVAL_RESTORE_TYPE_DEFAULT                         => 'default';
use constant CFGOPTVAL_RESTORE_TYPE_STANDBY                         => 'standby';

# Restore target action
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE                  => 'pause';
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE                => 'promote';
use constant CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN               => 'shutdown';

####################################################################################################################################
# Option defaults - only defined here when the default is used in more than one place
####################################################################################################################################
use constant CFGDEF_DEFAULT_BUFFER_SIZE_MIN                         => 16384;

use constant CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN                      => 0;
use constant CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX                      => 9;

use constant CFGDEF_DEFAULT_CONFIG_PATH                             => '/etc/' . PROJECT_EXE;
use constant CFGDEF_DEFAULT_CONFIG                                  => CFGDEF_DEFAULT_CONFIG_PATH . '/' . PROJECT_CONF;
use constant CFGDEF_DEFAULT_CONFIG_INCLUDE_PATH                     => CFGDEF_DEFAULT_CONFIG_PATH . '/conf.d';

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

# Command defines
#-----------------------------------------------------------------------------------------------------------------------------------
# Does this command log to a file?  This is the default behavior, but it can be overridden in code by calling logFileInit().  The
# default is true.
use constant CFGDEF_LOG_FILE                                        => 'log-file';
    push @EXPORT, qw(CFGDEF_LOG_FILE);

# Defines the log level to use for default messages that are output for every command.  For example, the log message that lists all
# the options passed is usually output at the info level, but that might not be appropriate for some commands, such as info.  Allow
# the log level to be lowered so these common messages will not be emitted where they might be distracting.
use constant CFGDEF_LOG_LEVEL_DEFAULT                               => 'log-level-default';
    push @EXPORT, qw(CFGDEF_LOG_LEVEL_DEFAULT);

# Does the command require a lock?  This doesn't mean a lock can't be taken explicitly later, just controls whether a lock will be
# acquired as soon at the command starts.
use constant CFGDEF_LOCK_REQUIRED                                   => 'lock-required';
    push @EXPORT, qw(CFGDEF_LOCK_REQUIRED);

# Does the command require a lock on the remote?  The lock will only be acquired for process id 0.
use constant CFGDEF_LOCK_REMOTE_REQUIRED                            => 'lock-remote-required';
    push @EXPORT, qw(CFGDEF_LOCK_REMOTE_REQUIRED);

# What type of lock is required?
use constant CFGDEF_LOCK_TYPE                                       => 'lock-type';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE);

use constant CFGDEF_LOCK_TYPE_ARCHIVE                               => 'archive';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE_ARCHIVE);
use constant CFGDEF_LOCK_TYPE_BACKUP                                => 'backup';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE_BACKUP);
use constant CFGDEF_LOCK_TYPE_ALL                                   => 'all';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE_ALL);
use constant CFGDEF_LOCK_TYPE_NONE                                  => 'none';
    push @EXPORT, qw(CFGDEF_LOCK_TYPE_NONE);

# Does the command allow parameters?  If not then the config parser will automatically error out if parameters are detected.  If so,
# then the command is responsible for ensuring that the parameters are valid.
use constant CFGDEF_PARAMETER_ALLOWED                               => 'parameter-allowed';
    push @EXPORT, qw(CFGDEF_PARAMETER_ALLOWED);

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
use constant CFGDEF_TYPE_PATH                                       => 'path';
    push @EXPORT, qw(CFGDEF_TYPE_PATH);
use constant CFGDEF_TYPE_STRING                                     => 'string';
    push @EXPORT, qw(CFGDEF_TYPE_STRING);
use constant CFGDEF_TYPE_SIZE                                       => 'size';
    push @EXPORT, qw(CFGDEF_TYPE_SIZE);

# Option config sections
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGDEF_SECTION_GLOBAL                                  => 'global';
    push @EXPORT, qw(CFGDEF_SECTION_GLOBAL);
use constant CFGDEF_SECTION_STANZA                                  => 'stanza';
    push @EXPORT, qw(CFGDEF_SECTION_STANZA);

####################################################################################################################################
# Command definition data
####################################################################################################################################
my $rhCommandDefine =
{
    &CFGCMD_ARCHIVE_GET =>
    {
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_ARCHIVE,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_ARCHIVE_PUSH =>
    {
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOCK_REMOTE_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_ARCHIVE,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_BACKUP =>
    {
        &CFGDEF_LOCK_REQUIRED => true,
        &CFGDEF_LOCK_REMOTE_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_BACKUP,
    },

    &CFGCMD_CHECK =>
    {
        &CFGDEF_LOG_FILE => false,
    },

    &CFGCMD_EXPIRE =>
    {
        &CFGDEF_LOCK_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_BACKUP,
    },

    &CFGCMD_HELP =>
    {
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_INFO =>
    {
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
    },

    &CFGCMD_REPO_CREATE =>
    {
        &CFGDEF_INTERNAL => true,
        &CFGDEF_LOG_FILE => false,
    },

    &CFGCMD_REPO_GET =>
    {
        &CFGDEF_INTERNAL => true,
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_REPO_LS =>
    {
        &CFGDEF_INTERNAL => true,
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_REPO_PUT =>
    {
        &CFGDEF_INTERNAL => true,
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_REPO_RM =>
    {
        &CFGDEF_INTERNAL => true,
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
        &CFGDEF_PARAMETER_ALLOWED => true,
    },

    &CFGCMD_RESTORE =>
    {
    },

    &CFGCMD_STANZA_CREATE =>
    {
        &CFGDEF_LOCK_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_ALL,
    },

    &CFGCMD_STANZA_DELETE =>
    {
        &CFGDEF_LOCK_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_ALL,
    },

    &CFGCMD_STANZA_UPGRADE =>
    {
        &CFGDEF_LOCK_REQUIRED => true,
        &CFGDEF_LOCK_TYPE => CFGDEF_LOCK_TYPE_ALL,
    },

    &CFGCMD_START =>
    {
    },

    &CFGCMD_STOP =>
    {
    },

    &CFGCMD_VERSION =>
    {
        &CFGDEF_LOG_FILE => false,
        &CFGDEF_LOG_LEVEL_DEFAULT => DEBUG,
    },
};

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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_CONFIG_INCLUDE_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_CONFIG,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_INCLUDE_PATH,
        &CFGDEF_NEGATE => false,
    },

    &CFGOPT_CONFIG_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_CONFIG,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_PATH,
        &CFGDEF_NEGATE => false,
    },

    &CFGOPT_DRYRUN =>
    {
       &CFGDEF_TYPE    => CFGDEF_TYPE_BOOLEAN,
       &CFGDEF_DEFAULT => false,
       &CFGDEF_COMMAND =>
       {
         &CFGCMD_EXPIRE => {},
      },
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
                &CFGDEF_INTERNAL => true,
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
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_SET =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_INFO =>
            {
                &CFGDEF_REQUIRED => false,
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_STANZA,
                },
            },
            &CFGCMD_RESTORE =>
            {
                &CFGDEF_DEFAULT => 'latest',
            },
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
            &CFGCMD_REPO_CREATE =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_REPO_GET =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_REPO_LS =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_REPO_PUT =>
            {
                &CFGDEF_REQUIRED => false
            },
            &CFGCMD_REPO_RM =>
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
                        &CFGOPTVAL_RESTORE_TYPE_IMMEDIATE,
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
                        &CFGOPTVAL_RESTORE_TYPE_STANDBY,
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
                    &CFGOPTVAL_RESTORE_TYPE_STANDBY,
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
                &CFGDEF_DEFAULT => CFGOPTVAL_OUTPUT_TEXT,
                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_OUTPUT_TEXT,
                    &CFGOPTVAL_OUTPUT_JSON,
                ]
            },

            &CFGCMD_REPO_LS =>
            {
                &CFGDEF_DEFAULT => CFGOPTVAL_OUTPUT_TEXT,
                &CFGDEF_ALLOW_LIST =>
                [
                    &CFGOPTVAL_OUTPUT_TEXT,
                    &CFGOPTVAL_OUTPUT_JSON,
                ]
            }
        }
    },

    # Command-line only local/remote options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_HOST_ID =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_ALLOW_RANGE => [1, CFGDEF_INDEX_PG > CFGDEF_INDEX_REPO ? CFGDEF_INDEX_PG : CFGDEF_INDEX_REPO],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_RESTORE => {},
        },
    },

    &CFGOPT_PROCESS =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_ALLOW_RANGE => [0, 1024],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGOPT_REMOTE_TYPE =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_STORAGE_TYPE_PG,
            &CFGOPTVAL_STORAGE_TYPE_REPO,
        ],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
    },

    # Command-line only storage options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_CIPHER_PASS =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_PUT => {},
        }
    },

    &CFGOPT_FILTER =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_LS => {},
        }
    },

    &CFGOPT_IGNORE_MISSING =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_GET => {},
        }
    },

    &CFGOPT_RAW =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_PUT => {},
        }
    },

    &CFGOPT_RECURSE =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_RM => {},
        }
    },

    &CFGOPT_SORT =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => 'asc',
        &CFGDEF_ALLOW_LIST =>
        [
            'none',
            'asc',
            'desc',
        ],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_REPO_LS => {},
        }
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
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
        },
    },

    &CFGOPT_BUFFER_SIZE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_SIZE,
        &CFGDEF_DEFAULT => 4194304,
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_SCK_BLOCK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_INTERNAL => true,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND => CFGOPT_BUFFER_SIZE,
    },

    &CFGOPT_SCK_KEEP_ALIVE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND => CFGOPT_BUFFER_SIZE,
    },

    &CFGOPT_TCP_KEEP_ALIVE_COUNT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND => CFGOPT_BUFFER_SIZE,
        &CFGDEF_ALLOW_RANGE => [1, 32],
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_SCK_KEEP_ALIVE,
            &CFGDEF_DEPEND_LIST => [true],
        },
    },

    &CFGOPT_TCP_KEEP_ALIVE_IDLE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND => CFGOPT_BUFFER_SIZE,
        &CFGDEF_ALLOW_RANGE => [1, 3600],
        &CFGDEF_DEPEND => CFGOPT_TCP_KEEP_ALIVE_COUNT,
    },

    &CFGOPT_TCP_KEEP_ALIVE_INTERVAL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND => CFGOPT_BUFFER_SIZE,
        &CFGDEF_ALLOW_RANGE => [1, 900],
        &CFGDEF_DEPEND => CFGOPT_TCP_KEEP_ALIVE_COUNT,
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_DELTA =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_RESTORE => {},
        },
    },

    # Option is deprecated and should not be referenced outside of cfgLoadUpdateOption().
    &CFGOPT_COMPRESS =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
        }
    },

    &CFGOPT_COMPRESS_TYPE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_DEFAULT => 'gz',
        &CFGDEF_ALLOW_LIST =>
        [
            'none',
            'gz',
            'lz4',
        ],
        &CFGDEF_COMMAND => CFGOPT_COMPRESS,
    },

    &CFGOPT_COMPRESS_LEVEL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGDEF_COMMAND => CFGOPT_COMPRESS,
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
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
    },

    &CFGOPT_IO_TIMEOUT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_FLOAT,
        &CFGDEF_DEFAULT => 60,
        &CFGDEF_ALLOW_RANGE => [.1, 3600],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGOPT_LOCK_PATH =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_DEFAULT => '/tmp/' . PROJECT_EXE,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_DEFAULT => '/var/log/' . PROJECT_EXE,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
        &CFGDEF_REQUIRED => true,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_CIPHER_TYPE,
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
            'hardlink' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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

    &CFGOPT_REPO_HOST_CONFIG_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_REPO_HOST_CONFIG,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_PATH,
    },

    &CFGOPT_REPO_HOST_CONFIG_INCLUDE_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_REPO_HOST_CONFIG,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_INCLUDE_PATH,
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
        &CFGDEF_DEFAULT => PROJECT_EXE,
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => '/var/lib/' . PROJECT_EXE,
        &CFGDEF_NAME_ALT =>
        {
            'repo-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_RETENTION_ARCHIVE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_NAME_ALT =>
        {
            'retention-archive' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_REPO_RETENTION_ARCHIVE_TYPE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => CFGOPTVAL_BACKUP_TYPE_FULL,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        },
        &CFGDEF_NAME_ALT =>
        {
            'retention-archive-type' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_BACKUP_TYPE_FULL,
            &CFGOPTVAL_BACKUP_TYPE_DIFF,
            &CFGOPTVAL_BACKUP_TYPE_INCR,
        ]
    },

    &CFGOPT_REPO_RETENTION_DIFF =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_NAME_ALT =>
        {
            'retention-diff' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
    },

    &CFGOPT_REPO_RETENTION_FULL =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_ALLOW_RANGE => [CFGDEF_DEFAULT_RETENTION_MIN, CFGDEF_DEFAULT_RETENTION_MAX],
        &CFGDEF_NAME_ALT =>
        {
            'retention-full' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_EXPIRE => {},
        }
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
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
        &CFGDEF_REQUIRED => true,
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_REPO_TYPE,
            &CFGDEF_DEPEND_LIST => [CFGOPTVAL_REPO_TYPE_S3],
        },
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-key' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        },
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
        &CFGDEF_REQUIRED => false,
        &CFGDEF_DEPEND => CFGOPT_REPO_S3_BUCKET,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-host' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
        },
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_PORT =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => 443,
        &CFGDEF_ALLOW_RANGE => [1, 65535],
        &CFGDEF_DEPEND => CFGOPT_REPO_S3_BUCKET,
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

    &CFGOPT_REPO_S3_TOKEN =>
    {
        &CFGDEF_INHERIT => CFGOPT_REPO_S3_KEY,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
    },

    &CFGOPT_REPO_S3_URI_STYLE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => CFGOPTVAL_REPO_S3_URI_STYLE_HOST,
        &CFGDEF_ALLOW_LIST =>
        [
            &CFGOPTVAL_REPO_S3_URI_STYLE_HOST,
            &CFGOPTVAL_REPO_S3_URI_STYLE_PATH,
        ],
        &CFGDEF_COMMAND => CFGOPT_REPO_TYPE,
        &CFGDEF_DEPEND => CFGOPT_REPO_S3_BUCKET,
    },

    &CFGOPT_REPO_S3_VERIFY_TLS =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_REPO,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_REPO,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_NAME_ALT =>
        {
            'repo-s3-verify-ssl' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'repo?-s3-verify-ssl' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_DEFAULT => '/var/spool/' . PROJECT_EXE,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET =>
            {
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_ARCHIVE_ASYNC,
                    &CFGDEF_DEPEND_LIST => [true],
                },
            },
            &CFGCMD_ARCHIVE_PUSH =>
            {
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_ARCHIVE_ASYNC,
                    &CFGDEF_DEPEND_LIST => [true],
                },
            },
        },
    },

    &CFGOPT_PROCESS_MAX =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_INTEGER,
        &CFGDEF_DEFAULT => 1,
        &CFGDEF_ALLOW_RANGE => [1, 999],
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
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
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
            &CFGCMD_RESTORE => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
            &CFGCMD_START => {},
            &CFGCMD_STOP => {},
        }
    },

    &CFGOPT_LOG_SUBPROCESS =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_EXPIRE => {},
            &CFGCMD_INFO => {},
            &CFGCMD_REPO_CREATE => {},
            &CFGCMD_REPO_GET => {},
            &CFGCMD_REPO_LS => {},
            &CFGCMD_REPO_PUT => {},
            &CFGCMD_REPO_RM => {},
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
            &CFGCMD_ARCHIVE_GET => {},
            &CFGCMD_ARCHIVE_PUSH => {},
        }
    },

    &CFGOPT_ARCHIVE_PUSH_QUEUE_MAX =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_SIZE,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'archive-queue-max' => {},
        },
        &CFGDEF_ALLOW_RANGE => [0, 4 * 1024 * 1024 * 1024 * 1024 * 1024], # 0-4PB
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_PUSH => {},
        },
    },

    &CFGOPT_ARCHIVE_GET_QUEUE_MAX =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_SIZE,
        &CFGDEF_DEFAULT => 128 * 1024 * 1024, # 128MB
        &CFGDEF_ALLOW_RANGE => [0, 4 * 1024 * 1024 * 1024 * 1024 * 1024], # 0-4PB
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_ARCHIVE_GET => {},
        },
    },

    # Backup options
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGOPT_ARCHIVE_CHECK =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_BOOLEAN,
        &CFGDEF_DEFAULT => true,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP =>
            {
                &CFGDEF_DEPEND =>
                {
                    &CFGDEF_DEPEND_OPTION => CFGOPT_ONLINE,
                    &CFGDEF_DEPEND_LIST => [true],
                },
            },
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

    &CFGOPT_EXCLUDE =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_LIST,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
        },
    },

    &CFGOPT_MANIFEST_SAVE_THRESHOLD =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_GLOBAL,
        &CFGDEF_TYPE => CFGDEF_TYPE_SIZE,
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
                &CFGOPTVAL_RESTORE_TYPE_IMMEDIATE,
                &CFGOPTVAL_RESTORE_TYPE_NAME,
                &CFGOPTVAL_RESTORE_TYPE_TIME,
                &CFGOPTVAL_RESTORE_TYPE_STANDBY,
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
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_PG,
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
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_PG,
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

    &CFGOPT_PG_HOST_CONFIG_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_PG_HOST_CMD,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_PATH,
    },

    &CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH =>
    {
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_INHERIT => CFGOPT_PG_HOST_CMD,
        &CFGDEF_DEFAULT => CFGDEF_DEFAULT_CONFIG_INCLUDE_PATH,
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_PG,
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
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_PG,
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
        &CFGDEF_TYPE => CFGDEF_TYPE_PATH,
        &CFGDEF_DEFAULT => undef,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_NAME_ALT =>
        {
            'db-socket-path' => {&CFGDEF_INDEX => 1, &CFGDEF_RESET => false},
            'db?-socket-path' => {&CFGDEF_RESET => false},
        },
    },

    &CFGOPT_PG_USER =>
    {
        &CFGDEF_SECTION => CFGDEF_SECTION_STANZA,
        &CFGDEF_TYPE => CFGDEF_TYPE_STRING,
        &CFGDEF_PREFIX => CFGDEF_PREFIX_PG,
        &CFGDEF_INDEX_TOTAL => CFGDEF_INDEX_PG,
        &CFGDEF_REQUIRED => false,
        &CFGDEF_COMMAND =>
        {
            &CFGCMD_BACKUP => {},
            &CFGCMD_CHECK => {},
            &CFGCMD_STANZA_CREATE => {},
            &CFGCMD_STANZA_DELETE => {},
            &CFGCMD_STANZA_UPGRADE => {},
        },
        &CFGDEF_DEPEND =>
        {
            &CFGDEF_DEPEND_OPTION => CFGOPT_PG_PATH
        },
    },
);

####################################################################################################################################
# Process command define defaults
####################################################################################################################################
foreach my $strCommand (sort(keys(%{$rhCommandDefine})))
{
    # Commands are external by default
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_INTERNAL}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_INTERNAL} = false;
    }

    # Log files are created by default
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOG_FILE}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOG_FILE} = true;
    }

    # Default log level is INFO
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOG_LEVEL_DEFAULT}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOG_LEVEL_DEFAULT} = INFO;
    }

    # Default lock required is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED} = false;
    }

    # Default lock remote required is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REMOTE_REQUIRED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REMOTE_REQUIRED} = false;
    }

    # Lock type must be set if a lock is required
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE}))
    {
        # Is a lock type required?
        if ($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED})
        {
            confess &log(ERROR, "lock type is required for command '${strCommand}'");
        }

        $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE} = CFGDEF_LOCK_TYPE_NONE;
    }
    else
    {
        if ($rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_REQUIRED} &&
            $rhCommandDefine->{$strCommand}{&CFGDEF_LOCK_TYPE} eq CFGDEF_LOCK_TYPE_NONE)
        {
            confess &log(ERROR, "lock type is required for command '${strCommand}' and cannot be 'none'");
        }
    }

    # Default parameter allowed is false
    if (!defined($rhCommandDefine->{$strCommand}{&CFGDEF_PARAMETER_ALLOWED}))
    {
        $rhCommandDefine->{$strCommand}{&CFGDEF_PARAMETER_ALLOWED} = false;
    }
}

####################################################################################################################################
# Process option define defaults
####################################################################################################################################
foreach my $strKey (sort(keys(%hConfigDefine)))
{
    # Error if prefix and index total are not both defined
    if ((defined($hConfigDefine{$strKey}{&CFGDEF_PREFIX}) && !defined($hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL})) ||
        (!defined($hConfigDefine{$strKey}{&CFGDEF_PREFIX}) && defined($hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL})))
    {
        confess &log(ASSERT, "CFGDEF_PREFIX and CFGDEF_INDEX_TOTAL must both be defined (or neither) for option '${strKey}'");
    }

    # If the define is a scalar then copy the entire define from the referenced option
    if (defined($hConfigDefine{$strKey}{&CFGDEF_INHERIT}))
    {
        # Make a copy in case there are overrides that need to be applied after inheriting
        my $hConfigDefineOverride = dclone($hConfigDefine{$strKey});

        # Copy the option being inherited from
        $hConfigDefine{$strKey} = dclone($hConfigDefine{$hConfigDefine{$strKey}{&CFGDEF_INHERIT}});

        # No need to copy the inheritance key
        delete($hConfigDefine{$strKey}{&CFGDEF_INHERIT});

        # It makes no sense to inherit alt names - they must be specified for each option
        delete($hConfigDefine{$strKey}{&CFGDEF_NAME_ALT});

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

    # Set index total for any option where it has not been explicitly defined
    if (!defined($hConfigDefine{$strKey}{&CFGDEF_INDEX_TOTAL}))
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
         $hConfigDefine{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_FLOAT ||
         $hConfigDefine{$strKey}{&CFGDEF_TYPE} eq CFGDEF_TYPE_SIZE) &&
         !(defined($hConfigDefine{$strKey}{&CFGDEF_ALLOW_RANGE}) || defined($hConfigDefine{$strKey}{&CFGDEF_ALLOW_LIST})))
    {
        confess &log(ASSERT, "int/float option '${strKey}' must have allow range or list");
    }

    # Ensure all commands are valid
    foreach my $strCommand (sort(keys(%{$hConfigDefine{$strKey}{&CFGDEF_COMMAND}})))
    {
        if (!defined($rhCommandDefine->{$strCommand}))
        {
            confess &log(ASSERT, "invalid command '${strCommand}'");
        }
    }
}

####################################################################################################################################
# Get option definition
####################################################################################################################################
sub cfgDefine
{
    return dclone(\%hConfigDefine);
}

push @EXPORT, qw(cfgDefine);

####################################################################################################################################
# Get command definition
####################################################################################################################################
sub cfgDefineCommand
{
    return dclone($rhCommandDefine);
}

push @EXPORT, qw(cfgDefineCommand);

####################################################################################################################################
# Get list of all commands
####################################################################################################################################
sub cfgDefineCommandList
{
    # Return sorted list
    return (sort(keys(%{$rhCommandDefine})));
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
