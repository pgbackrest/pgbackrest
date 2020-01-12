####################################################################################################################################
# Automatically generated by Build.pm -- do not modify directly.
####################################################################################################################################
package pgBackRest::LibCAuto;

use strict;
use warnings;

# Configuration option value constants
sub libcAutoConstant
{
    return
    {
        CFGOPTVAL_INFO_OUTPUT_TEXT                                       => 'text',
        CFGOPTVAL_INFO_OUTPUT_JSON                                       => 'json',

        CFGOPTVAL_LS_OUTPUT_TEXT                                         => 'text',
        CFGOPTVAL_LS_OUTPUT_JSON                                         => 'json',

        CFGOPTVAL_REMOTE_TYPE_PG                                         => 'pg',
        CFGOPTVAL_REMOTE_TYPE_REPO                                       => 'repo',

        CFGOPTVAL_REPO_CIPHER_TYPE_NONE                                  => 'none',
        CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC                           => 'aes-256-cbc',

        CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_FULL                       => 'full',
        CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_DIFF                       => 'diff',
        CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_INCR                       => 'incr',

        CFGOPTVAL_REPO_S3_URI_STYLE_HOST                                 => 'host',
        CFGOPTVAL_REPO_S3_URI_STYLE_PATH                                 => 'path',

        CFGOPTVAL_REPO_TYPE_CIFS                                         => 'cifs',
        CFGOPTVAL_REPO_TYPE_POSIX                                        => 'posix',
        CFGOPTVAL_REPO_TYPE_S3                                           => 's3',

        CFGOPTVAL_SORT_NONE                                              => 'none',
        CFGOPTVAL_SORT_ASC                                               => 'asc',
        CFGOPTVAL_SORT_DESC                                              => 'desc',

        CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE                            => 'pause',
        CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE                          => 'promote',
        CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN                         => 'shutdown',

        CFGOPTVAL_BACKUP_TYPE_FULL                                       => 'full',
        CFGOPTVAL_BACKUP_TYPE_DIFF                                       => 'diff',
        CFGOPTVAL_BACKUP_TYPE_INCR                                       => 'incr',

        CFGOPTVAL_RESTORE_TYPE_NAME                                      => 'name',
        CFGOPTVAL_RESTORE_TYPE_TIME                                      => 'time',
        CFGOPTVAL_RESTORE_TYPE_XID                                       => 'xid',
        CFGOPTVAL_RESTORE_TYPE_PRESERVE                                  => 'preserve',
        CFGOPTVAL_RESTORE_TYPE_NONE                                      => 'none',
        CFGOPTVAL_RESTORE_TYPE_IMMEDIATE                                 => 'immediate',
        CFGOPTVAL_RESTORE_TYPE_DEFAULT                                   => 'default',
        CFGOPTVAL_RESTORE_TYPE_STANDBY                                   => 'standby',

        CFGDEF_TYPE_BOOLEAN                                              => 0,
        CFGDEF_TYPE_FLOAT                                                => 1,
        CFGDEF_TYPE_HASH                                                 => 2,
        CFGDEF_TYPE_INTEGER                                              => 3,
        CFGDEF_TYPE_LIST                                                 => 4,
        CFGDEF_TYPE_PATH                                                 => 5,
        CFGDEF_TYPE_SIZE                                                 => 6,
        CFGDEF_TYPE_STRING                                               => 7,

        ENCODE_TYPE_BASE64                                               => 0,

        CIPHER_MODE_ENCRYPT                                              => 0,
        CIPHER_MODE_DECRYPT                                              => 1,
    }
}

# Export function and constants
sub libcAutoExportTag
{
    return
    {
        checksum =>
        [
            'pageChecksum',
        ],

        config =>
        [
            'CFGOPTVAL_INFO_OUTPUT_TEXT',
            'CFGOPTVAL_INFO_OUTPUT_JSON',
            'CFGOPTVAL_LS_OUTPUT_TEXT',
            'CFGOPTVAL_LS_OUTPUT_JSON',
            'CFGOPTVAL_REMOTE_TYPE_PG',
            'CFGOPTVAL_REMOTE_TYPE_REPO',
            'CFGOPTVAL_REPO_CIPHER_TYPE_NONE',
            'CFGOPTVAL_REPO_CIPHER_TYPE_AES_256_CBC',
            'CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_FULL',
            'CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_DIFF',
            'CFGOPTVAL_REPO_RETENTION_ARCHIVE_TYPE_INCR',
            'CFGOPTVAL_REPO_S3_URI_STYLE_HOST',
            'CFGOPTVAL_REPO_S3_URI_STYLE_PATH',
            'CFGOPTVAL_REPO_TYPE_CIFS',
            'CFGOPTVAL_REPO_TYPE_POSIX',
            'CFGOPTVAL_REPO_TYPE_S3',
            'CFGOPTVAL_SORT_NONE',
            'CFGOPTVAL_SORT_ASC',
            'CFGOPTVAL_SORT_DESC',
            'CFGOPTVAL_RESTORE_TARGET_ACTION_PAUSE',
            'CFGOPTVAL_RESTORE_TARGET_ACTION_PROMOTE',
            'CFGOPTVAL_RESTORE_TARGET_ACTION_SHUTDOWN',
            'CFGOPTVAL_BACKUP_TYPE_FULL',
            'CFGOPTVAL_BACKUP_TYPE_DIFF',
            'CFGOPTVAL_BACKUP_TYPE_INCR',
            'CFGOPTVAL_RESTORE_TYPE_NAME',
            'CFGOPTVAL_RESTORE_TYPE_TIME',
            'CFGOPTVAL_RESTORE_TYPE_XID',
            'CFGOPTVAL_RESTORE_TYPE_PRESERVE',
            'CFGOPTVAL_RESTORE_TYPE_NONE',
            'CFGOPTVAL_RESTORE_TYPE_IMMEDIATE',
            'CFGOPTVAL_RESTORE_TYPE_DEFAULT',
            'CFGOPTVAL_RESTORE_TYPE_STANDBY',
            'CFGCMD_ARCHIVE_GET',
            'CFGCMD_ARCHIVE_PUSH',
            'CFGCMD_BACKUP',
            'CFGCMD_CHECK',
            'CFGCMD_EXPIRE',
            'CFGCMD_HELP',
            'CFGCMD_INFO',
            'CFGCMD_LS',
            'CFGCMD_RESTORE',
            'CFGCMD_STANZA_CREATE',
            'CFGCMD_STANZA_DELETE',
            'CFGCMD_STANZA_UPGRADE',
            'CFGCMD_START',
            'CFGCMD_STOP',
            'CFGCMD_VERSION',
            'CFGOPT_ARCHIVE_ASYNC',
            'CFGOPT_ARCHIVE_CHECK',
            'CFGOPT_ARCHIVE_COPY',
            'CFGOPT_ARCHIVE_GET_QUEUE_MAX',
            'CFGOPT_ARCHIVE_PUSH_QUEUE_MAX',
            'CFGOPT_ARCHIVE_TIMEOUT',
            'CFGOPT_BACKUP_STANDBY',
            'CFGOPT_BUFFER_SIZE',
            'CFGOPT_CHECKSUM_PAGE',
            'CFGOPT_CMD_SSH',
            'CFGOPT_COMPRESS',
            'CFGOPT_COMPRESS_LEVEL',
            'CFGOPT_COMPRESS_LEVEL_NETWORK',
            'CFGOPT_CONFIG',
            'CFGOPT_CONFIG_INCLUDE_PATH',
            'CFGOPT_CONFIG_PATH',
            'CFGOPT_DB_INCLUDE',
            'CFGOPT_DB_TIMEOUT',
            'CFGOPT_DELTA',
            'CFGOPT_EXCLUDE',
            'CFGOPT_FILTER',
            'CFGOPT_FORCE',
            'CFGOPT_HOST_ID',
            'CFGOPT_LINK_ALL',
            'CFGOPT_LINK_MAP',
            'CFGOPT_LOCK_PATH',
            'CFGOPT_LOG_LEVEL_CONSOLE',
            'CFGOPT_LOG_LEVEL_FILE',
            'CFGOPT_LOG_LEVEL_STDERR',
            'CFGOPT_LOG_PATH',
            'CFGOPT_LOG_SUBPROCESS',
            'CFGOPT_LOG_TIMESTAMP',
            'CFGOPT_MANIFEST_SAVE_THRESHOLD',
            'CFGOPT_NEUTRAL_UMASK',
            'CFGOPT_ONLINE',
            'CFGOPT_OUTPUT',
            'CFGOPT_PG_HOST',
            'CFGOPT_PG_HOST2',
            'CFGOPT_PG_HOST3',
            'CFGOPT_PG_HOST4',
            'CFGOPT_PG_HOST5',
            'CFGOPT_PG_HOST6',
            'CFGOPT_PG_HOST7',
            'CFGOPT_PG_HOST8',
            'CFGOPT_PG_HOST_CMD',
            'CFGOPT_PG_HOST_CMD2',
            'CFGOPT_PG_HOST_CMD3',
            'CFGOPT_PG_HOST_CMD4',
            'CFGOPT_PG_HOST_CMD5',
            'CFGOPT_PG_HOST_CMD6',
            'CFGOPT_PG_HOST_CMD7',
            'CFGOPT_PG_HOST_CMD8',
            'CFGOPT_PG_HOST_CONFIG',
            'CFGOPT_PG_HOST_CONFIG2',
            'CFGOPT_PG_HOST_CONFIG3',
            'CFGOPT_PG_HOST_CONFIG4',
            'CFGOPT_PG_HOST_CONFIG5',
            'CFGOPT_PG_HOST_CONFIG6',
            'CFGOPT_PG_HOST_CONFIG7',
            'CFGOPT_PG_HOST_CONFIG8',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH2',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH3',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH4',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH5',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH6',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH7',
            'CFGOPT_PG_HOST_CONFIG_INCLUDE_PATH8',
            'CFGOPT_PG_HOST_CONFIG_PATH',
            'CFGOPT_PG_HOST_CONFIG_PATH2',
            'CFGOPT_PG_HOST_CONFIG_PATH3',
            'CFGOPT_PG_HOST_CONFIG_PATH4',
            'CFGOPT_PG_HOST_CONFIG_PATH5',
            'CFGOPT_PG_HOST_CONFIG_PATH6',
            'CFGOPT_PG_HOST_CONFIG_PATH7',
            'CFGOPT_PG_HOST_CONFIG_PATH8',
            'CFGOPT_PG_HOST_PORT',
            'CFGOPT_PG_HOST_PORT2',
            'CFGOPT_PG_HOST_PORT3',
            'CFGOPT_PG_HOST_PORT4',
            'CFGOPT_PG_HOST_PORT5',
            'CFGOPT_PG_HOST_PORT6',
            'CFGOPT_PG_HOST_PORT7',
            'CFGOPT_PG_HOST_PORT8',
            'CFGOPT_PG_HOST_USER',
            'CFGOPT_PG_HOST_USER2',
            'CFGOPT_PG_HOST_USER3',
            'CFGOPT_PG_HOST_USER4',
            'CFGOPT_PG_HOST_USER5',
            'CFGOPT_PG_HOST_USER6',
            'CFGOPT_PG_HOST_USER7',
            'CFGOPT_PG_HOST_USER8',
            'CFGOPT_PG_PATH',
            'CFGOPT_PG_PATH2',
            'CFGOPT_PG_PATH3',
            'CFGOPT_PG_PATH4',
            'CFGOPT_PG_PATH5',
            'CFGOPT_PG_PATH6',
            'CFGOPT_PG_PATH7',
            'CFGOPT_PG_PATH8',
            'CFGOPT_PG_PORT',
            'CFGOPT_PG_PORT2',
            'CFGOPT_PG_PORT3',
            'CFGOPT_PG_PORT4',
            'CFGOPT_PG_PORT5',
            'CFGOPT_PG_PORT6',
            'CFGOPT_PG_PORT7',
            'CFGOPT_PG_PORT8',
            'CFGOPT_PG_SOCKET_PATH',
            'CFGOPT_PG_SOCKET_PATH2',
            'CFGOPT_PG_SOCKET_PATH3',
            'CFGOPT_PG_SOCKET_PATH4',
            'CFGOPT_PG_SOCKET_PATH5',
            'CFGOPT_PG_SOCKET_PATH6',
            'CFGOPT_PG_SOCKET_PATH7',
            'CFGOPT_PG_SOCKET_PATH8',
            'CFGOPT_PG_USER',
            'CFGOPT_PG_USER2',
            'CFGOPT_PG_USER3',
            'CFGOPT_PG_USER4',
            'CFGOPT_PG_USER5',
            'CFGOPT_PG_USER6',
            'CFGOPT_PG_USER7',
            'CFGOPT_PG_USER8',
            'CFGOPT_PROCESS',
            'CFGOPT_PROCESS_MAX',
            'CFGOPT_PROTOCOL_TIMEOUT',
            'CFGOPT_RECOVERY_OPTION',
            'CFGOPT_RECURSE',
            'CFGOPT_REMOTE_TYPE',
            'CFGOPT_REPO_CIPHER_PASS',
            'CFGOPT_REPO_CIPHER_TYPE',
            'CFGOPT_REPO_HARDLINK',
            'CFGOPT_REPO_HOST',
            'CFGOPT_REPO_HOST_CMD',
            'CFGOPT_REPO_HOST_CONFIG',
            'CFGOPT_REPO_HOST_CONFIG_INCLUDE_PATH',
            'CFGOPT_REPO_HOST_CONFIG_PATH',
            'CFGOPT_REPO_HOST_PORT',
            'CFGOPT_REPO_HOST_USER',
            'CFGOPT_REPO_PATH',
            'CFGOPT_REPO_RETENTION_ARCHIVE',
            'CFGOPT_REPO_RETENTION_ARCHIVE_TYPE',
            'CFGOPT_REPO_RETENTION_DIFF',
            'CFGOPT_REPO_RETENTION_FULL',
            'CFGOPT_REPO_S3_BUCKET',
            'CFGOPT_REPO_S3_CA_FILE',
            'CFGOPT_REPO_S3_CA_PATH',
            'CFGOPT_REPO_S3_ENDPOINT',
            'CFGOPT_REPO_S3_HOST',
            'CFGOPT_REPO_S3_KEY',
            'CFGOPT_REPO_S3_KEY_SECRET',
            'CFGOPT_REPO_S3_PORT',
            'CFGOPT_REPO_S3_REGION',
            'CFGOPT_REPO_S3_TOKEN',
            'CFGOPT_REPO_S3_URI_STYLE',
            'CFGOPT_REPO_S3_VERIFY_TLS',
            'CFGOPT_REPO_TYPE',
            'CFGOPT_RESUME',
            'CFGOPT_SET',
            'CFGOPT_SORT',
            'CFGOPT_SPOOL_PATH',
            'CFGOPT_STANZA',
            'CFGOPT_START_FAST',
            'CFGOPT_STOP_AUTO',
            'CFGOPT_TABLESPACE_MAP',
            'CFGOPT_TABLESPACE_MAP_ALL',
            'CFGOPT_TARGET',
            'CFGOPT_TARGET_ACTION',
            'CFGOPT_TARGET_EXCLUSIVE',
            'CFGOPT_TARGET_TIMELINE',
            'CFGOPT_TYPE',
            'cfgCommandName',
            'cfgOptionIndexTotal',
            'cfgOptionName',
        ],

        configDefine =>
        [
            'CFGDEF_TYPE_BOOLEAN',
            'CFGDEF_TYPE_FLOAT',
            'CFGDEF_TYPE_HASH',
            'CFGDEF_TYPE_INTEGER',
            'CFGDEF_TYPE_LIST',
            'CFGDEF_TYPE_PATH',
            'CFGDEF_TYPE_SIZE',
            'CFGDEF_TYPE_STRING',
            'cfgCommandId',
            'cfgDefOptionDefault',
            'cfgDefOptionPrefix',
            'cfgDefOptionSecure',
            'cfgDefOptionType',
            'cfgDefOptionValid',
            'cfgOptionId',
            'cfgOptionTotal',
        ],

        crypto =>
        [
            'cryptoHashOne',
        ],

        debug =>
        [
            'libcUvSize',
        ],

        storage =>
        [
            'storageRepoFree',
        ],

        test =>
        [
            'cfgParseTest',
        ],
    }
}

1;
