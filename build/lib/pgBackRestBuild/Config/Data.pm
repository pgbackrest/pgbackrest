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
# Command constants - basic commands that are allowed in backrest
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
# DB/BACKUP Constants
####################################################################################################################################
use constant DB                                                     => 'db';
    push @EXPORT, qw(DB);
use constant BACKUP                                                 => 'backup';
    push @EXPORT, qw(BACKUP);

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
# REPO Type Constants
####################################################################################################################################
use constant REPO_TYPE_CIFS                                         => 'cifs';
    push @EXPORT, qw(REPO_TYPE_CIFS);
use constant REPO_TYPE_POSIX                                        => 'posix';
    push @EXPORT, qw(REPO_TYPE_POSIX);
use constant REPO_TYPE_S3                                           => 's3';
    push @EXPORT, qw(REPO_TYPE_S3);

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
use constant CFGBLDOPT_RULE_ALT_NAME                                   => 'alt-name';
    push @EXPORT, qw(CFGBLDOPT_RULE_ALT_NAME);
use constant CFGBLDOPT_RULE_ALLOW_LIST                                 => 'allow-list';
    push @EXPORT, qw(CFGBLDOPT_RULE_ALLOW_LIST);
use constant CFGBLDOPT_RULE_ALLOW_RANGE                                => 'allow-range';
    push @EXPORT, qw(CFGBLDOPT_RULE_ALLOW_RANGE);
use constant CFGBLDOPT_RULE_DEFAULT                                    => 'default';
    push @EXPORT, qw(CFGBLDOPT_RULE_DEFAULT);
use constant CFGBLDOPT_RULE_DEPEND                                     => 'depend';
    push @EXPORT, qw(CFGBLDOPT_RULE_DEPEND);
use constant CFGBLDOPT_RULE_DEPEND_OPTION                              => 'depend-option';
    push @EXPORT, qw(CFGBLDOPT_RULE_DEPEND_OPTION);
use constant CFGBLDOPT_RULE_DEPEND_LIST                                => 'depend-list';
    push @EXPORT, qw(CFGBLDOPT_RULE_DEPEND_LIST);
use constant CFGBLDOPT_RULE_DEPEND_VALUE                               => 'depend-value';
    push @EXPORT, qw(CFGBLDOPT_RULE_DEPEND_VALUE);
use constant CFGBLDOPT_RULE_HASH_VALUE                                 => 'hash-value';
    push @EXPORT, qw(CFGBLDOPT_RULE_HASH_VALUE);
use constant CFGBLDOPT_RULE_HINT                                       => 'hint';
    push @EXPORT, qw(CFGBLDOPT_RULE_HINT);
use constant CFGBLDOPT_RULE_INDEX                                      => 'index';
    push @EXPORT, qw(CFGBLDOPT_RULE_INDEX);
use constant CFGBLDOPT_RULE_NEGATE                                     => 'negate';
    push @EXPORT, qw(CFGBLDOPT_RULE_NEGATE);
use constant CFGBLDOPT_RULE_PREFIX                                     => 'prefix';
    push @EXPORT, qw(CFGBLDOPT_RULE_PREFIX);
use constant CFGBLDOPT_RULE_COMMAND                                    => 'command';
    push @EXPORT, qw(CFGBLDOPT_RULE_COMMAND);
use constant CFGBLDOPT_RULE_REQUIRED                                   => 'required';
    push @EXPORT, qw(CFGBLDOPT_RULE_REQUIRED);
use constant CFGBLDOPT_RULE_SECTION                                    => 'section';
    push @EXPORT, qw(CFGBLDOPT_RULE_SECTION);
use constant CFGBLDOPT_RULE_SECURE                                     => 'secure';
    push @EXPORT, qw(CFGBLDOPT_RULE_SECURE);
use constant CFGBLDOPT_RULE_TYPE                                       => 'type';
    push @EXPORT, qw(CFGBLDOPT_RULE_TYPE);

####################################################################################################################################
# Option Types
####################################################################################################################################
use constant CFGBLDOPT_TYPE_BOOLEAN                                    => 'boolean';
    push @EXPORT, qw(CFGBLDOPT_TYPE_BOOLEAN);
use constant CFGBLDOPT_TYPE_FLOAT                                      => 'float';
    push @EXPORT, qw(CFGBLDOPT_TYPE_FLOAT);
use constant CFGBLDOPT_TYPE_HASH                                       => 'hash';
    push @EXPORT, qw(CFGBLDOPT_TYPE_HASH);
use constant CFGBLDOPT_TYPE_INTEGER                                    => 'integer';
    push @EXPORT, qw(CFGBLDOPT_TYPE_INTEGER);
use constant CFGBLDOPT_TYPE_STRING                                     => 'string';
    push @EXPORT, qw(CFGBLDOPT_TYPE_STRING);

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
use constant CFGBLDOPT_CONFIG                                          => 'config';
    push @EXPORT, qw(CFGBLDOPT_CONFIG);
use constant CFGBLDOPT_DELTA                                           => 'delta';
    push @EXPORT, qw(CFGBLDOPT_DELTA);
use constant CFGBLDOPT_FORCE                                           => 'force';
    push @EXPORT, qw(CFGBLDOPT_FORCE);
use constant CFGBLDOPT_ONLINE                                          => 'online';
    push @EXPORT, qw(CFGBLDOPT_ONLINE);
use constant CFGBLDOPT_SET                                             => 'set';
    push @EXPORT, qw(CFGBLDOPT_SET);
use constant CFGBLDOPT_STANZA                                          => 'stanza';
    push @EXPORT, qw(CFGBLDOPT_STANZA);
use constant CFGBLDOPT_TARGET                                          => 'target';
    push @EXPORT, qw(CFGBLDOPT_TARGET);
use constant CFGBLDOPT_TARGET_EXCLUSIVE                                => 'target-exclusive';
    push @EXPORT, qw(CFGBLDOPT_TARGET_EXCLUSIVE);
use constant CFGBLDOPT_TARGET_ACTION                                   => 'target-action';
    push @EXPORT, qw(CFGBLDOPT_TARGET_ACTION);
use constant CFGBLDOPT_TARGET_TIMELINE                                 => 'target-timeline';
    push @EXPORT, qw(CFGBLDOPT_TARGET_TIMELINE);
use constant CFGBLDOPT_TYPE                                            => 'type';
    push @EXPORT, qw(CFGBLDOPT_TYPE);
use constant CFGBLDOPT_OUTPUT                                          => 'output';
    push @EXPORT, qw(CFGBLDOPT_OUTPUT);

# Command-line only help/version
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_HELP                                            => 'help';
    push @EXPORT, qw(CFGBLDOPT_HELP);
use constant CFGBLDOPT_VERSION                                         => 'version';
    push @EXPORT, qw(CFGBLDOPT_VERSION);

# Command-line only local/remote
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_COMMAND                                          => 'command';
    push @EXPORT, qw(CFGBLDOPT_COMMAND);
use constant CFGBLDOPT_PROCESS                                          => 'process';
    push @EXPORT, qw(CFGBLDOPT_PROCESS);
use constant CFGBLDOPT_HOST_ID                                          => 'host-id';
    push @EXPORT, qw(CFGBLDOPT_HOST_ID);

# Command-line only test
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_TEST                                            => 'test';
    push @EXPORT, qw(CFGBLDOPT_TEST);
use constant CFGBLDOPT_TEST_DELAY                                      => 'test-delay';
    push @EXPORT, qw(CFGBLDOPT_TEST_DELAY);
use constant CFGBLDOPT_TEST_POINT                                      => 'test-point';
    push @EXPORT, qw(CFGBLDOPT_TEST_POINT);

# GENERAL Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_TIMEOUT                                 => 'archive-timeout';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_TIMEOUT);
use constant CFGBLDOPT_BUFFER_SIZE                                     => 'buffer-size';
    push @EXPORT, qw(CFGBLDOPT_BUFFER_SIZE);
use constant CFGBLDOPT_DB_TIMEOUT                                      => 'db-timeout';
    push @EXPORT, qw(CFGBLDOPT_DB_TIMEOUT);
use constant CFGBLDOPT_COMPRESS                                        => 'compress';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS);
use constant CFGBLDOPT_COMPRESS_LEVEL                                  => 'compress-level';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS_LEVEL);
use constant CFGBLDOPT_COMPRESS_LEVEL_NETWORK                          => 'compress-level-network';
    push @EXPORT, qw(CFGBLDOPT_COMPRESS_LEVEL_NETWORK);
use constant CFGBLDOPT_NEUTRAL_UMASK                                   => 'neutral-umask';
    push @EXPORT, qw(CFGBLDOPT_NEUTRAL_UMASK);
use constant CFGBLDOPT_PROTOCOL_TIMEOUT                                => 'protocol-timeout';
    push @EXPORT, qw(CFGBLDOPT_PROTOCOL_TIMEOUT);
use constant CFGBLDOPT_PROCESS_MAX                                     => 'process-max';
    push @EXPORT, qw(CFGBLDOPT_PROCESS_MAX);

# Commands
use constant CFGBLDOPT_CMD_SSH                                         => 'cmd-ssh';
    push @EXPORT, qw(CFGBLDOPT_CMD_SSH);

# Paths
use constant CFGBLDOPT_LOCK_PATH                                       => 'lock-path';
    push @EXPORT, qw(CFGBLDOPT_LOCK_PATH);
use constant CFGBLDOPT_LOG_PATH                                        => 'log-path';
    push @EXPORT, qw(CFGBLDOPT_LOG_PATH);
use constant CFGBLDOPT_SPOOL_PATH                                      => 'spool-path';
    push @EXPORT, qw(CFGBLDOPT_SPOOL_PATH);

# Repository
use constant CFGBLDOPT_REPO_PATH                                       => 'repo-path';
    push @EXPORT, qw(CFGBLDOPT_REPO_PATH);
use constant CFGBLDOPT_REPO_TYPE                                       => 'repo-type';
    push @EXPORT, qw(CFGBLDOPT_REPO_TYPE);

# Repository S3
use constant CFGBLDOPT_REPO_S3_KEY                                     => 'repo-s3-key';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_KEY);
use constant CFGBLDOPT_REPO_S3_KEY_SECRET                              => 'repo-s3-key-secret';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_KEY_SECRET);
use constant CFGBLDOPT_REPO_S3_BUCKET                                  => 'repo-s3-bucket';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_BUCKET);
use constant CFGBLDOPT_REPO_S3_CA_FILE                                 => 'repo-s3-ca-file';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_CA_FILE);
use constant CFGBLDOPT_REPO_S3_CA_PATH                                 => 'repo-s3-ca-path';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_CA_PATH);
use constant CFGBLDOPT_REPO_S3_ENDPOINT                                => 'repo-s3-endpoint';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_ENDPOINT);
use constant CFGBLDOPT_REPO_S3_HOST                                    => 'repo-s3-host';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_HOST);
use constant CFGBLDOPT_REPO_S3_REGION                                  => 'repo-s3-region';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_REGION);
use constant CFGBLDOPT_REPO_S3_VERIFY_SSL                              => 'repo-s3-verify-ssl';
    push @EXPORT, qw(CFGBLDOPT_REPO_S3_VERIFY_SSL);

# Log level
use constant CFGBLDOPT_LOG_LEVEL_CONSOLE                               => 'log-level-console';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_CONSOLE);
use constant CFGBLDOPT_LOG_LEVEL_FILE                                  => 'log-level-file';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_FILE);
use constant CFGBLDOPT_LOG_LEVEL_STDERR                                => 'log-level-stderr';
    push @EXPORT, qw(CFGBLDOPT_LOG_LEVEL_STDERR);
use constant CFGBLDOPT_LOG_TIMESTAMP                                   => 'log-timestamp';
    push @EXPORT, qw(CFGBLDOPT_LOG_TIMESTAMP);

# ARCHIVE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_ASYNC                                   => 'archive-async';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_ASYNC);
# Deprecated and to be removed
use constant CFGBLDOPT_ARCHIVE_MAX_MB                                  => 'archive-max-mb';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_MAX_MB);
use constant CFGBLDOPT_ARCHIVE_QUEUE_MAX                               => 'archive-queue-max';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_QUEUE_MAX);

# BACKUP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_ARCHIVE_CHECK                                => 'archive-check';
    push @EXPORT, qw(CFGBLDOPT_ARCHIVE_CHECK);
use constant CFGBLDOPT_BACKUP_ARCHIVE_COPY                             => 'archive-copy';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_ARCHIVE_COPY);
use constant CFGBLDOPT_BACKUP_CMD                                      => 'backup-cmd';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_CMD);
use constant CFGBLDOPT_BACKUP_CONFIG                                   => 'backup-config';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_CONFIG);
use constant CFGBLDOPT_BACKUP_HOST                                     => 'backup-host';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_HOST);
use constant CFGBLDOPT_BACKUP_STANDBY                                  => 'backup-standby';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_STANDBY);
use constant CFGBLDOPT_BACKUP_USER                                     => 'backup-user';
    push @EXPORT, qw(CFGBLDOPT_BACKUP_USER);
use constant CFGBLDOPT_CHECKSUM_PAGE                                   => 'checksum-page';
    push @EXPORT, qw(CFGBLDOPT_CHECKSUM_PAGE);
use constant CFGBLDOPT_HARDLINK                                        => 'hardlink';
    push @EXPORT, qw(CFGBLDOPT_HARDLINK);
use constant CFGBLDOPT_MANIFEST_SAVE_THRESHOLD                         => 'manifest-save-threshold';
    push @EXPORT, qw(CFGBLDOPT_MANIFEST_SAVE_THRESHOLD);
use constant CFGBLDOPT_RESUME                                          => 'resume';
    push @EXPORT, qw(CFGBLDOPT_RESUME);
use constant CFGBLDOPT_START_FAST                                      => 'start-fast';
    push @EXPORT, qw(CFGBLDOPT_START_FAST);
use constant CFGBLDOPT_STOP_AUTO                                       => 'stop-auto';
    push @EXPORT, qw(CFGBLDOPT_STOP_AUTO);

# EXPIRE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_RETENTION_ARCHIVE                               => 'retention-archive';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_ARCHIVE);
use constant CFGBLDOPT_RETENTION_ARCHIVE_TYPE                          => 'retention-archive-type';
    push @EXPORT, qw(CFGBLDOPT_RETENTION_ARCHIVE_TYPE);
use constant CFGBLDOPT_RETENTION_DIFF                                  => 'retention-' . BACKUP_TYPE_DIFF;
    push @EXPORT, qw(CFGBLDOPT_RETENTION_DIFF);
use constant CFGBLDOPT_RETENTION_FULL                                  => 'retention-' . BACKUP_TYPE_FULL;
    push @EXPORT, qw(CFGBLDOPT_RETENTION_FULL);

# RESTORE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_DB_INCLUDE                                      => 'db-include';
    push @EXPORT, qw(CFGBLDOPT_DB_INCLUDE);
use constant CFGBLDOPT_LINK_ALL                                        => 'link-all';
    push @EXPORT, qw(CFGBLDOPT_LINK_ALL);
use constant CFGBLDOPT_LINK_MAP                                        => 'link-map';
    push @EXPORT, qw(CFGBLDOPT_LINK_MAP);
use constant CFGBLDOPT_TABLESPACE_MAP_ALL                              => 'tablespace-map-all';
    push @EXPORT, qw(CFGBLDOPT_TABLESPACE_MAP_ALL);
use constant CFGBLDOPT_TABLESPACE_MAP                                  => 'tablespace-map';
    push @EXPORT, qw(CFGBLDOPT_TABLESPACE_MAP);
use constant CFGBLDOPT_RECOVERY_OPTION                         => 'recovery-option';
    push @EXPORT, qw(CFGBLDOPT_RECOVERY_OPTION);

# STANZA Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPT_PREFIX_DB                                       => 'db';
    push @EXPORT, qw(CFGBLDOPT_PREFIX_DB);

use constant CFGBLDOPT_DB_CMD                                          => CFGBLDOPT_PREFIX_DB . '-cmd';
    push @EXPORT, qw(CFGBLDOPT_DB_CMD);
use constant CFGBLDOPT_DB_CONFIG                                       => CFGBLDOPT_PREFIX_DB . '-config';
    push @EXPORT, qw(CFGBLDOPT_DB_CONFIG);
use constant CFGBLDOPT_DB_HOST                                         => CFGBLDOPT_PREFIX_DB . '-host';
    push @EXPORT, qw(CFGBLDOPT_DB_HOST);
use constant CFGBLDOPT_DB_PATH                                         => CFGBLDOPT_PREFIX_DB . '-path';
    push @EXPORT, qw(CFGBLDOPT_DB_PATH);
use constant CFGBLDOPT_DB_PORT                                         => CFGBLDOPT_PREFIX_DB . '-port';
    push @EXPORT, qw(CFGBLDOPT_DB_PORT);
use constant CFGBLDOPT_DB_SOCKET_PATH                                  => CFGBLDOPT_PREFIX_DB . '-socket-path';
    push @EXPORT, qw(CFGBLDOPT_DB_SOCKET_PATH);
use constant CFGBLDOPT_DB_USER                                         => CFGBLDOPT_PREFIX_DB . '-user';
    push @EXPORT, qw(CFGBLDOPT_DB_USER);

####################################################################################################################################
# Option Index Values
####################################################################################################################################
use constant CFGBLDOPTDEF_INDEX_DB                                  => 2;

####################################################################################################################################
# Option Defaults
####################################################################################################################################

# Command-line only
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_TYPE                             => BACKUP_TYPE_INCR;
use constant CFGBLDOPTDEF_DEFAULT_INFO_OUTPUT                             => INFO_OUTPUT_TEXT;
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_DELTA                           => false;
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_FORCE                           => false;
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_SET                             => 'latest';
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_TARGET_EXCLUSIVE                => false;
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_TARGET_ACTION                   => RECOVERY_ACTION_PAUSE;
use constant CFGBLDOPTDEF_DEFAULT_RESTORE_TYPE                            => RECOVERY_TYPE_DEFAULT;
use constant CFGBLDOPTDEF_DEFAULT_STANZA_CREATE_FORCE                     => false;

# Command-line only test
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_TEST                                    => false;
use constant CFGBLDOPTDEF_DEFAULT_TEST_DELAY                              => 5;

# GENERAL Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT                         => 60;
use constant CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT_MIN                     => WAIT_TIME_MINIMUM;
use constant CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT_MAX                     => 86400;

use constant CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE                             => COMMON_IO_BUFFER_MAX;
use constant CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN                         => 16384;

use constant CFGBLDOPTDEF_DEFAULT_COMPRESS                                => true;

use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL                          => 6;
use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_MIN                      => 0;
use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_MAX                      => 9;

use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK                  => 3;
use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN              => 0;
use constant CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX              => 9;

use constant CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT                              => 1800;
use constant CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MIN                          => WAIT_TIME_MINIMUM;
use constant CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MAX                          => 86400 * 7;

use constant CFGBLDOPTDEF_DEFAULT_REPO_SYNC                               => true;

use constant CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT                        => CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT + 30;
use constant CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT_MIN                    => CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MIN;
use constant CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT_MAX                    => CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MAX;

use constant CFGBLDOPTDEF_DEFAULT_CMD_SSH                                 => 'ssh';
use constant CFGBLDOPTDEF_DEFAULT_CONFIG                                  => '/etc/' . BACKREST_CONF;
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_CONFIG);
use constant CFGBLDOPTDEF_DEFAULT_LOCK_PATH                               => '/tmp/' . BACKREST_EXE;
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_LOCK_PATH);
use constant CFGBLDOPTDEF_DEFAULT_LOG_PATH                                => '/var/log/' . BACKREST_EXE;
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_LOG_PATH);
use constant CFGBLDOPTDEF_DEFAULT_NEUTRAL_UMASK                           => true;
use constant CFGBLDOPTDEF_DEFAULT_REPO_LINK                               => true;
use constant CFGBLDOPTDEF_DEFAULT_REPO_PATH                               => '/var/lib/' . BACKREST_EXE;
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_REPO_PATH);
use constant CFGBLDOPTDEF_DEFAULT_REPO_S3_VERIFY_SSL                      => true;
use constant CFGBLDOPTDEF_DEFAULT_REPO_TYPE                               => REPO_TYPE_POSIX;
use constant CFGBLDOPTDEF_DEFAULT_SPOOL_PATH                              => '/var/spool/' . BACKREST_EXE;
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_SPOOL_PATH);
use constant CFGBLDOPTDEF_DEFAULT_PROCESS_MAX                              => 1;
use constant CFGBLDOPTDEF_DEFAULT_PROCESS_MAX_MIN                          => 1;
use constant CFGBLDOPTDEF_DEFAULT_PROCESS_MAX_MAX                          => 96;

# LOG Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_CONSOLE                       => lc(WARN);
use constant CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_FILE                          => lc(INFO);
use constant CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_STDERR                        => lc(WARN);
    push @EXPORT, qw(CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_STDERR);
use constant CFGBLDOPTDEF_DEFAULT_LOG_TIMESTAMP                           => true;

# ARCHIVE SECTION
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_ARCHIVE_ASYNC                           => false;

# BACKUP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_ARCHIVE_CHECK                    => true;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_ARCHIVE_COPY                     => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_FORCE                            => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_HARDLINK                         => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_ONLINE                           => true;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_MANIFEST_SAVE_THRESHOLD          => 1073741824;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_RESUME                           => true;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_STANDBY                          => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_STOP_AUTO                        => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_START_FAST                       => false;
use constant CFGBLDOPTDEF_DEFAULT_BACKUP_USER                             => 'backrest';

# START/STOP Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_STOP_FORCE                              => false;

# EXPIRE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_RETENTION_ARCHIVE_TYPE                  => BACKUP_TYPE_FULL;
use constant CFGBLDOPTDEF_DEFAULT_RETENTION_MIN                           => 1;
use constant CFGBLDOPTDEF_DEFAULT_RETENTION_MAX                           => 999999999;

# RESTORE Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_LINK_ALL                                => false;

# STANZA Section
#-----------------------------------------------------------------------------------------------------------------------------------
use constant CFGBLDOPTDEF_DEFAULT_DB_PORT                                 => 5432;
use constant CFGBLDOPTDEF_DEFAULT_DB_USER                                 => 'postgres';

####################################################################################################################################
# Option Rule Hash
#
# Contains the rules for options: which commands the option can/cannot be specified, for which commands it is required, default
# settings, types, ranges, whether the option is negatable, whether it has dependencies, etc. The initial section is the global
# section meaning the rules defined there apply to all commands listed for the option.
#
# CFGBLDOPT_RULE_COMMAND:
#     List of commands the option can be used with this option.  An empty hash signifies that the command does no deviate from the
#     option defaults.  Otherwise, overrides can be specified.
#
# NOTE: If the option (A) has a dependency on another option (B) then the CFGBLDCMD_ must also be specified in the other option
#         (B), else it will still error on the option (A).
#
# CFGBLDOPT_RULE_REQUIRED:
#   In global section:
#       true - if the option does not have a default, then setting CFGBLDOPT_RULE_REQUIRED in the global section means all commands
#              listed in CFGBLDOPT_RULE_COMMAND require the user to set it.
#       false - no commands listed require it as an option but it can be set. This can be overridden for individual commands by
#               setting CFGBLDOPT_RULE_REQUIRED in the CFGBLDOPT_RULE_COMMAND section.
#   In CFGBLDOPT_RULE_COMMAND section:
#       true - the option must be set somehow for the command, either by default (CFGBLDOPT_RULE_DEFAULT) or by the user.
# 	        &CFGBLDCMD_CHECK =>
#             {
#                 &CFGBLDOPT_RULE_REQUIRED => true
#             },
#       false - mainly used for overriding the CFGBLDOPT_RULE_REQUIRED in the global section.
#
# CFGBLDOPT_RULE_DEFAULT:
#   Sets a default for the option for all commands if listed in the global section, or for specific commands if listed in the
#   CFGBLDOPT_RULE_COMMAND section.
#
# CFGBLDOPT_RULE_NEGATE:
#   The option can be negated with "no" e.g. --no-compress.  This applies tp options that are only valid on the command line (i.e.
#   no config section defined).  All config options are automatically negatable.
#
# CFGBLDOPT_RULE_DEPEND:
#   Specify the dependencies this option has on another option. All commands listed for this option must also be listed in the
#   dependent option(s).
#   CFGBLDOPT_RULE_DEPEND_LIST further defines the allowable settings for the depended option.
#
# CFGBLDOPT_RULE_ALLOW_LIST:
#   Lists the allowable settings for the option.
####################################################################################################################################
my %hOptionRule =
(
    # Command-line only
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_CONFIG =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_CONFIG,
        &CFGBLDOPT_RULE_NEGATE => true,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_DELTA,
            }
        }
    },

    &CFGBLDOPT_FORCE =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_FORCE,
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_ONLINE,
                    &CFGBLDOPT_RULE_DEPEND_VALUE   => false
                }
            },

            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_FORCE,
            },

            &CFGBLDCMD_STANZA_CREATE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_STANZA_CREATE_FORCE,
            },

            &CFGBLDCMD_STOP =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_STOP_FORCE
            }
        }
    },

    &CFGBLDOPT_ONLINE =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_NEGATE => true,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_ONLINE,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_SET =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_SET,
            }
        }
    },

    &CFGBLDOPT_STANZA =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_ARCHIVE_PUSH =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_CHECK =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_EXPIRE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_INFO =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_STANZA_CREATE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_STANZA_UPGRADE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true
            },
            &CFGBLDCMD_START =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_STOP =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            }
        }
    },

    &CFGBLDOPT_TARGET =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDOPT_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_EXCLUSIVE =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_TARGET_EXCLUSIVE,
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDOPT_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_ACTION =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_TARGET_ACTION,
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDOPT_RULE_DEPEND_LIST =>
                    {
                        &RECOVERY_TYPE_NAME => true,
                        &RECOVERY_TYPE_TIME => true,
                        &RECOVERY_TYPE_XID  => true
                    }
                }
            }
        }
    },

    &CFGBLDOPT_TARGET_TIMELINE =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false,
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
                    &CFGBLDOPT_RULE_DEPEND_LIST =>
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

    &CFGBLDOPT_TYPE =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_TYPE,
                &CFGBLDOPT_RULE_ALLOW_LIST =>
                {
                    &BACKUP_TYPE_FULL => true,
                    &BACKUP_TYPE_DIFF => true,
                    &BACKUP_TYPE_INCR => true,
                }
            },

            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDOPT_RULE_ALLOW_LIST =>
                {
                    &DB                      => true,
                    &BACKUP                  => true,
                },
            },

            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDOPT_RULE_ALLOW_LIST =>
                {
                    &DB                      => true,
                    &BACKUP                  => true,
                },
            },

            &CFGBLDCMD_RESTORE =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RESTORE_TYPE,
                &CFGBLDOPT_RULE_ALLOW_LIST =>
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

    &CFGBLDOPT_OUTPUT =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_INFO =>
            {
                &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_INFO_OUTPUT,
                &CFGBLDOPT_RULE_ALLOW_LIST =>
                {
                    &INFO_OUTPUT_TEXT => true,
                    &INFO_OUTPUT_JSON => true
                }
            }
        }
    },

    # Command-line only local/remote
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_COMMAND =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_REMOTE => {},
        }
    },

    &CFGBLDOPT_HOST_ID =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL => {},
        },
    },

    &CFGBLDOPT_PROCESS =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDOPT_RULE_REQUIRED => true,
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false,
            },
        },
    },

    # Command-line only test
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_TEST =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_TEST,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_TEST_DELAY =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_FLOAT,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_TEST_DELAY,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TEST,
            &CFGBLDOPT_RULE_DEPEND_VALUE => true
        },
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_TEST_POINT =>
    {
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_HASH,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TEST,
            &CFGBLDOPT_RULE_DEPEND_VALUE => true
        },
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
        }
    },

    # GENERAL Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_TIMEOUT =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_FLOAT,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT_MIN, CFGBLDOPTDEF_DEFAULT_ARCHIVE_TIMEOUT_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
        },
    },

    &CFGBLDOPT_BUFFER_SIZE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE,
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN         => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 2     => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 4     => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 8     => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 16    => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 32    => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 64    => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 128   => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 256   => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 512   => true,
            &CFGBLDOPTDEF_DEFAULT_BUFFER_SIZE_MIN * 1024  => true,
        },
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_FLOAT,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MIN, CFGBLDOPTDEF_DEFAULT_DB_TIMEOUT_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_COMPRESS,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_MIN, CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN, CFGBLDOPTDEF_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_NEUTRAL_UMASK,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_CMD_SSH,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOCK_PATH,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOG_PATH,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_FLOAT,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT_MIN, CFGBLDOPTDEF_DEFAULT_PROTOCOL_TIMEOUT_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_REPO_PATH,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_REPO_TYPE,
            &CFGBLDOPT_RULE_DEPEND_VALUE   => REPO_TYPE_S3,
        },
        &CFGBLDOPT_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_CA_FILE => &CFGBLDOPT_REPO_S3_HOST,
    &CFGBLDOPT_REPO_S3_CA_PATH => &CFGBLDOPT_REPO_S3_HOST,

    &CFGBLDOPT_REPO_S3_KEY =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_SECURE => true,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_REPO_TYPE,
            &CFGBLDOPT_RULE_DEPEND_VALUE   => REPO_TYPE_S3,
        },
        &CFGBLDOPT_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_KEY_SECRET => CFGBLDOPT_REPO_S3_KEY,

    &CFGBLDOPT_REPO_S3_ENDPOINT => CFGBLDOPT_REPO_S3_BUCKET,

    &CFGBLDOPT_REPO_S3_HOST =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_REQUIRED  => false,
        &CFGBLDOPT_RULE_DEPEND => CFGBLDOPT_REPO_S3_BUCKET,
        &CFGBLDOPT_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
    },

    &CFGBLDOPT_REPO_S3_REGION => CFGBLDOPT_REPO_S3_BUCKET,

    &CFGBLDOPT_REPO_S3_VERIFY_SSL =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_REPO_S3_VERIFY_SSL,
        &CFGBLDOPT_RULE_COMMAND => CFGBLDOPT_REPO_TYPE,
        &CFGBLDOPT_RULE_DEPEND => CFGBLDOPT_REPO_S3_BUCKET,
    },

    &CFGBLDOPT_REPO_TYPE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_REPO_TYPE,
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            &REPO_TYPE_CIFS     => true,
            &REPO_TYPE_POSIX    => true,
            &REPO_TYPE_S3       => true,
        },
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_SPOOL_PATH,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        },
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_ARCHIVE_ASYNC,
            &CFGBLDOPT_RULE_DEPEND_VALUE   => true
        },
    },

    &CFGBLDOPT_PROCESS_MAX =>
    {
        &CFGBLDOPT_RULE_ALT_NAME => 'thread-max',
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_PROCESS_MAX,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_PROCESS_MAX_MIN, CFGBLDOPTDEF_DEFAULT_PROCESS_MAX_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_RESTORE => {},
        }
    },

    # LOG Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_LOG_LEVEL_CONSOLE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_CONSOLE,
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DETAIL) => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        },
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_FILE,
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DEBUG)  => true,
            lc(DETAIL) => true,
            lc(TRACE)  => true
        },
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOG_LEVEL_STDERR,
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DETAIL) => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        },
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LOG_TIMESTAMP,
        &CFGBLDOPT_RULE_COMMAND =>
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

    # ARCHIVE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_ASYNC =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_ARCHIVE_ASYNC,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        }
    },

    # Deprecated and to be removed
    &CFGBLDOPT_ARCHIVE_MAX_MB =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        }
    },

    &CFGBLDOPT_ARCHIVE_QUEUE_MAX =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_PUSH => {},
        },
    },

    # BACKUP Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_ARCHIVE_CHECK =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_ARCHIVE_CHECK,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_ONLINE,
            &CFGBLDOPT_RULE_DEPEND_VALUE   => true,
        },
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
        },
    },

    &CFGBLDOPT_BACKUP_ARCHIVE_COPY =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_ARCHIVE_COPY,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP =>
            {
                &CFGBLDOPT_RULE_DEPEND =>
                {
                    &CFGBLDOPT_RULE_DEPEND_OPTION  => CFGBLDOPT_ARCHIVE_CHECK,
                    &CFGBLDOPT_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &CFGBLDOPT_BACKUP_CMD =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        },
    },

    &CFGBLDOPT_BACKUP_CONFIG =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_CONFIG,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        },
    },

    &CFGBLDOPT_BACKUP_HOST =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
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

    &CFGBLDOPT_BACKUP_STANDBY =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_STANDBY,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGBLDOPT_BACKUP_USER =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_USER,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_BACKUP_HOST
        }
    },

    &CFGBLDOPT_CHECKSUM_PAGE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_HARDLINK =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_HARDLINK,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_MANIFEST_SAVE_THRESHOLD =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_MANIFEST_SAVE_THRESHOLD,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_RESUME =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_RESUME,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_START_FAST =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_START_FAST,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    &CFGBLDOPT_STOP_AUTO =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_BACKUP_STOP_AUTO,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
        }
    },

    # EXPIRE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_RETENTION_ARCHIVE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_RETENTION_MIN, CFGBLDOPTDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    &CFGBLDOPT_RETENTION_ARCHIVE_TYPE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_RETENTION_ARCHIVE_TYPE,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        },
        &CFGBLDOPT_RULE_ALLOW_LIST =>
        {
            &BACKUP_TYPE_FULL => 1,
            &BACKUP_TYPE_DIFF => 1,
            &BACKUP_TYPE_INCR => 1
        }
    },

    &CFGBLDOPT_RETENTION_DIFF =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_RETENTION_MIN, CFGBLDOPTDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    &CFGBLDOPT_RETENTION_FULL =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_ALLOW_RANGE => [CFGBLDOPTDEF_DEFAULT_RETENTION_MIN, CFGBLDOPTDEF_DEFAULT_RETENTION_MAX],
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_EXPIRE => {},
        }
    },

    # RESTORE Section
    #-------------------------------------------------------------------------------------------------------------------------------
    &CFGBLDOPT_DB_INCLUDE =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_HASH,
        &CFGBLDOPT_RULE_HASH_VALUE => false,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_LINK_ALL =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_BOOLEAN,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_LINK_ALL,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        }
    },

    &CFGBLDOPT_LINK_MAP =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_HASH,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_TABLESPACE_MAP_ALL =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        }
    },

    &CFGBLDOPT_TABLESPACE_MAP =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_HASH,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
    },

    &CFGBLDOPT_RECOVERY_OPTION =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_HASH,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_RESTORE => {},
        },
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_TYPE,
            &CFGBLDOPT_RULE_DEPEND_LIST =>
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
    &CFGBLDOPT_DB_CMD =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
        },
    },

    &CFGBLDOPT_DB_CONFIG =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_GLOBAL,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_CONFIG,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
        },
    },

    &CFGBLDOPT_DB_HOST =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_STANZA,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_STANZA,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_REQUIRED => true,
        &CFGBLDOPT_RULE_HINT => "does this stanza exist?",
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_ARCHIVE_GET =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_ARCHIVE_PUSH =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_LOCAL =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_REMOTE =>
            {
                &CFGBLDOPT_RULE_REQUIRED => false
            },
            &CFGBLDCMD_RESTORE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
    },

    &CFGBLDOPT_DB_PORT =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_STANZA,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_INTEGER,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_DB_PORT,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_REMOTE => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        }
    },

    &CFGBLDOPT_DB_SOCKET_PATH =>
    {
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_STANZA,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_COMMAND =>
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
        &CFGBLDOPT_RULE_SECTION => CONFIG_SECTION_STANZA,
        &CFGBLDOPT_RULE_PREFIX => CFGBLDOPT_PREFIX_DB,
        &CFGBLDOPT_RULE_TYPE => CFGBLDOPT_TYPE_STRING,
        &CFGBLDOPT_RULE_DEFAULT => CFGBLDOPTDEF_DEFAULT_DB_USER,
        &CFGBLDOPT_RULE_COMMAND =>
        {
            &CFGBLDCMD_BACKUP => {},
            &CFGBLDCMD_CHECK => {},
            &CFGBLDCMD_LOCAL => {},
            &CFGBLDCMD_STANZA_CREATE => {},
            &CFGBLDCMD_STANZA_UPGRADE => {},
        },
        &CFGBLDOPT_RULE_REQUIRED => false,
        &CFGBLDOPT_RULE_DEPEND =>
        {
            &CFGBLDOPT_RULE_DEPEND_OPTION => CFGBLDOPT_DB_HOST
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

    # Default type is string
    if (!defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_TYPE}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_TYPE} = CFGBLDOPT_TYPE_STRING;
    }

    # Hash types by default have hash values (rather than just a boolean list)
    if (!defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_HASH_VALUE}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_HASH_VALUE} =
            $hOptionRule{$strKey}{&CFGBLDOPT_RULE_TYPE} eq CFGBLDOPT_TYPE_HASH ? true : false;
    }

    # If the command section is a scalar then copy the section from the referenced option
    if (defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_COMMAND}) && !ref($hOptionRule{$strKey}{&CFGBLDOPT_RULE_COMMAND}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_COMMAND} =
            dclone($hOptionRule{$hOptionRule{$strKey}{&CFGBLDOPT_RULE_COMMAND}}{&CFGBLDOPT_RULE_COMMAND});
    }

    # If the required section is a scalar then copy the section from the referenced option
    if (defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_DEPEND}) && !ref($hOptionRule{$strKey}{&CFGBLDOPT_RULE_DEPEND}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_DEPEND} =
            dclone($hOptionRule{$hOptionRule{$strKey}{&CFGBLDOPT_RULE_DEPEND}}{&CFGBLDOPT_RULE_DEPEND});
    }

    # All config options can be negated
    if ($hOptionRule{$strKey}{&CFGBLDOPT_RULE_TYPE} eq CFGBLDOPT_TYPE_BOOLEAN && defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_SECTION}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_NEGATE} = true;
    }

    # By default options are not secure
    if (!defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_SECURE}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_SECURE} = false;
    }

    # Set all indices to 1 by default - this defines how many copies of any option there can be
    if (!defined($hOptionRule{$strKey}{&CFGBLDOPT_RULE_INDEX}))
    {
        $hOptionRule{$strKey}{&CFGBLDOPT_RULE_INDEX} = 1;
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
        return defined($hOptionRule{$strOption}{&CFGBLDOPT_RULE_COMMAND}) &&
               defined($hOptionRule{$strOption}{&CFGBLDOPT_RULE_COMMAND}{$strCommand}) &&
               ref($hOptionRule{$strOption}{&CFGBLDOPT_RULE_COMMAND}{$strCommand}) eq 'HASH' ?
               $hOptionRule{$strOption}{&CFGBLDOPT_RULE_COMMAND}{$strCommand} : undef;
    }

    return;
}

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
    my $strDefault = defined($oCommandRule) ? $$oCommandRule{&CFGBLDOPT_RULE_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $hOptionRule{$strOption}{&CFGBLDOPT_RULE_DEFAULT};
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
    if (defined($oCommandRule) && defined($$oCommandRule{&CFGBLDOPT_RULE_ALLOW_RANGE}))
    {
        return $$oCommandRule{&CFGBLDOPT_RULE_ALLOW_RANGE}[0], $$oCommandRule{&CFGBLDOPT_RULE_ALLOW_RANGE}[1];
    }

    # If defined return, else try to grab the global default
    return $hOptionRule{$strOption}{&CFGBLDOPT_RULE_ALLOW_RANGE}[0], $hOptionRule{$strOption}{&CFGBLDOPT_RULE_ALLOW_RANGE}[1];
}

push @EXPORT, qw(optionRange);

####################################################################################################################################
# optionType
#
# Get the option type.
####################################################################################################################################
sub optionType
{
    my $strOption = shift;

    return $hOptionRule{$strOption}{&CFGBLDOPT_RULE_TYPE};
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
# optionRuleHelpGet - get option rules without indexed options
####################################################################################################################################
sub optionRuleHelpGet
{
    return dclone(\%hOptionRule);
}

push @EXPORT, qw(optionRuleHelpGet);

####################################################################################################################################
# optionRuleGet - get option rules
####################################################################################################################################
sub optionRuleGet
{
    my $rhOptionRuleIndex = dclone(\%hOptionRule);

    foreach my $strKey (sort(keys(%{$rhOptionRuleIndex})))
    {
        # Build options for all possible db configurations
        if (defined($rhOptionRuleIndex->{$strKey}{&CFGBLDOPT_RULE_PREFIX}) &&
            $rhOptionRuleIndex->{$strKey}{&CFGBLDOPT_RULE_PREFIX} eq CFGBLDOPT_PREFIX_DB)
        {
            my $strPrefix = $rhOptionRuleIndex->{$strKey}{&CFGBLDOPT_RULE_PREFIX};
            $rhOptionRuleIndex->{$strKey}{&CFGBLDOPT_RULE_INDEX} = CFGBLDOPTDEF_INDEX_DB;

            for (my $iIndex = 1; $iIndex <= CFGBLDOPTDEF_INDEX_DB; $iIndex++)
            {
                my $strKeyNew = "${strPrefix}${iIndex}" . substr($strKey, length($strPrefix));

                $rhOptionRuleIndex->{$strKeyNew} = dclone($rhOptionRuleIndex->{$strKey});

                # Create the alternate name for option index 1
                if ($iIndex == 1)
                {
                    $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_ALT_NAME} = $strKey;
                }
                else
                {
                    $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_REQUIRED} = false;
                }

                if (defined($rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_DEPEND}) &&
                    defined($rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_DEPEND}{&CFGBLDOPT_RULE_DEPEND_OPTION}))
                {
                    $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_DEPEND}{&CFGBLDOPT_RULE_DEPEND_OPTION} =
                        "${strPrefix}${iIndex}" .
                        substr(
                            $rhOptionRuleIndex->{$strKeyNew}{&CFGBLDOPT_RULE_DEPEND}{&CFGBLDOPT_RULE_DEPEND_OPTION},
                            length($strPrefix));
                }
            }

            delete($rhOptionRuleIndex->{$strKey});
        }
    }

    return $rhOptionRuleIndex;
}

push @EXPORT, qw(optionRuleGet);

####################################################################################################################################
# commandHashGet - get the hash that contains all valid commands
####################################################################################################################################
sub commandHashGet
{
    my $rhCommand;

    # Get commands from the rule hash
    foreach my $strOption (sort(keys(%hOptionRule)))
    {
        foreach my $strCommand (sort(keys(%{$hOptionRule{$strOption}{&CFGBLDOPT_RULE_COMMAND}})))
        {
            $rhCommand->{$strCommand} = true;
        }
    }

    # Add special commands
    $rhCommand->{&CFGBLDCMD_HELP} = true;
    $rhCommand->{&CFGBLDCMD_VERSION} = true;

    return $rhCommand;
}

push @EXPORT, qw(commandHashGet);

1;
