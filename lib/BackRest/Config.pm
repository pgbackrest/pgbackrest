####################################################################################################################################
# CONFIG MODULE
####################################################################################################################################
package BackRest::Config;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);
use Cwd qw(abs_path);
use Exporter qw(import);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Utility;

####################################################################################################################################
# Export functions
####################################################################################################################################
our @EXPORT = qw(configLoad optionGet optionTest optionRuleGet optionRequired optionDefault operationGet operationTest
                 operationSet optionRemoteType optionRemoteTypeTest optionRemote optionRemoteTest remoteDestroy);

####################################################################################################################################
# DB/BACKUP Constants
####################################################################################################################################
use constant
{
    DB              => 'db',
    BACKUP          => 'backup',
    NONE            => 'none'
};

push @EXPORT, qw(DB BACKUP NONE);

####################################################################################################################################
# Operation constants - basic operations that are allowed in backrest
####################################################################################################################################
use constant
{
    OP_ARCHIVE_GET   => 'archive-get',
    OP_ARCHIVE_PUSH  => 'archive-push',
    OP_BACKUP        => 'backup',
    OP_RESTORE       => 'restore',
    OP_EXPIRE        => 'expire'
};

push @EXPORT, qw(OP_ARCHIVE_GET OP_ARCHIVE_PUSH OP_BACKUP OP_RESTORE OP_EXPIRE);

####################################################################################################################################
# BACKUP Type Constants
####################################################################################################################################
use constant
{
    BACKUP_TYPE_FULL          => 'full',
    BACKUP_TYPE_DIFF          => 'diff',
    BACKUP_TYPE_INCR          => 'incr'
};

push @EXPORT, qw(BACKUP_TYPE_FULL BACKUP_TYPE_DIFF BACKUP_TYPE_INCR);

####################################################################################################################################
# RECOVERY Type Constants
####################################################################################################################################
use constant
{
    RECOVERY_TYPE_NAME          => 'name',
    RECOVERY_TYPE_TIME          => 'time',
    RECOVERY_TYPE_XID           => 'xid',
    RECOVERY_TYPE_PRESERVE      => 'preserve',
    RECOVERY_TYPE_NONE          => 'none',
    RECOVERY_TYPE_DEFAULT       => 'default'
};

push @EXPORT, qw(RECOVERY_TYPE_NAME RECOVERY_TYPE_TIME RECOVERY_TYPE_XID RECOVERY_TYPE_PRESERVE RECOVERY_TYPE_NONE
                 RECOVERY_TYPE_DEFAULT);

####################################################################################################################################
# Configuration section constants
####################################################################################################################################
use constant
{
    CONFIG_GLOBAL                           => 'global',

    CONFIG_SECTION_ARCHIVE                  => 'archive',
    CONFIG_SECTION_BACKUP                   => 'backup',
    CONFIG_SECTION_COMMAND                  => 'command',
    CONFIG_SECTION_GENERAL                  => 'general',
    CONFIG_SECTION_LOG                      => 'log',
    CONFIG_SECTION_RESTORE_RECOVERY_SETTING => 'restore:recovery-setting',
    CONFIG_SECTION_RESTORE_TABLESPACE_MAP   => 'restore:tablespace-map',
    CONFIG_SECTION_EXPIRE                   => 'expire',
    CONFIG_SECTION_STANZA                   => 'stanza'
};

push @EXPORT, qw(CONFIG_GLOBAL

                 CONFIG_SECTION_ARCHIVE CONFIG_SECTION_BACKUP CONFIG_SECTION_COMMAND
                 CONFIG_SECTION_GENERAL CONFIG_SECTION_LOG CONFIG_SECTION_RESTORE_RECOVERY_SETTING
                 CONFIG_SECTION_EXPIRE CONFIG_SECTION_STANZA CONFIG_SECTION_RESTORE_TABLESPACE_MAP);

####################################################################################################################################
# Option constants
####################################################################################################################################
use constant
{
    # Command-line-only options
    OPTION_CONFIG                   => 'config',
    OPTION_DELTA                    => 'delta',
    OPTION_FORCE                    => 'force',
    OPTION_NO_START_STOP            => 'no-start-stop',
    OPTION_SET                      => 'set',
    OPTION_STANZA                   => 'stanza',
    OPTION_TARGET                   => 'target',
    OPTION_TARGET_EXCLUSIVE         => 'target-exclusive',
    OPTION_TARGET_RESUME            => 'target-resume',
    OPTION_TARGET_TIMELINE          => 'target-timeline',
    OPTION_TYPE                     => 'type',

    # Command-line/conf file options
    # GENERAL Section
    OPTION_BUFFER_SIZE              => 'buffer-size',
    OPTION_COMPRESS                 => 'compress',
    OPTION_COMPRESS_LEVEL           => 'compress-level',
    OPTION_COMPRESS_LEVEL_NETWORK   => 'compress-level-network',
    OPTION_REPO_PATH                => 'repo-path',
    OPTION_REPO_REMOTE_PATH         => 'repo-remote-path',
    OPTION_THREAD_MAX               => 'thread-max',
    OPTION_THREAD_TIMEOUT           => 'thread-timeout',

    # ARCHIVE Section
    OPTION_ARCHIVE_MAX_MB           => 'archive-max-mb',
    OPTION_ARCHIVE_ASYNC            => 'archive-async',

    # BACKUP Section
    OPTION_BACKUP_ARCHIVE_CHECK     => 'archive-check',
    OPTION_BACKUP_ARCHIVE_COPY      => 'archive-copy',
    OPTION_HARDLINK                 => 'hardlink',
    OPTION_BACKUP_HOST              => 'backup-host',
    OPTION_BACKUP_USER              => 'backup-user',
    OPTION_START_FAST               => 'start-fast',

    # COMMAND Section
    OPTION_COMMAND_REMOTE           => 'cmd-remote',
    OPTION_COMMAND_PSQL             => 'cmd-psql',
    OPTION_COMMAND_PSQL_OPTION      => 'cmd-psql-option',

    # LOG Section
    OPTION_LOG_LEVEL_CONSOLE        => 'log-level-console',
    OPTION_LOG_LEVEL_FILE           => 'log-level-file',

    # EXPIRE Section
    OPTION_RETENTION_ARCHIVE        => 'retention-archive',
    OPTION_RETENTION_ARCHIVE_TYPE   => 'retention-archive-type',
    OPTION_RETENTION_DIFF           => 'retention-' . BACKUP_TYPE_DIFF,
    OPTION_RETENTION_FULL           => 'retention-' . BACKUP_TYPE_FULL,

    # RESTORE Section
    OPTION_RESTORE_TABLESPACE_MAP   => 'tablespace-map',
    OPTION_RESTORE_RECOVERY_SETTING => 'recovery-setting',

    # STANZA Section
    OPTION_DB_HOST                  => 'db-host',
    OPTION_DB_PATH                  => 'db-path',
    OPTION_DB_USER                  => 'db-user',

    # Command-line-only help/version options
    OPTION_HELP                     => 'help',
    OPTION_VERSION                  => 'version',

    # Command-line-only test options
    OPTION_TEST                     => 'test',
    OPTION_TEST_DELAY               => 'test-delay',
    OPTION_TEST_NO_FORK             => 'no-fork'
};

push @EXPORT, qw(OPTION_CONFIG OPTION_DELTA OPTION_FORCE OPTION_NO_START_STOP OPTION_SET OPTION_STANZA OPTION_TARGET
                 OPTION_TARGET_EXCLUSIVE OPTION_TARGET_RESUME OPTION_TARGET_TIMELINE OPTION_TYPE

                 OPTION_DB_HOST OPTION_BACKUP_HOST OPTION_ARCHIVE_MAX_MB OPTION_BACKUP_ARCHIVE_CHECK OPTION_BACKUP_ARCHIVE_COPY
                 OPTION_ARCHIVE_ASYNC
                 OPTION_BUFFER_SIZE OPTION_COMPRESS OPTION_COMPRESS_LEVEL OPTION_COMPRESS_LEVEL_NETWORK OPTION_HARDLINK
                 OPTION_PATH_ARCHIVE OPTION_REPO_PATH OPTION_REPO_REMOTE_PATH OPTION_DB_PATH OPTION_LOG_LEVEL_CONSOLE
                 OPTION_LOG_LEVEL_FILE
                 OPTION_RESTORE_RECOVERY_SETTING OPTION_RETENTION_ARCHIVE OPTION_RETENTION_ARCHIVE_TYPE OPTION_RETENTION_FULL
                 OPTION_RETENTION_DIFF OPTION_START_FAST OPTION_THREAD_MAX OPTION_THREAD_TIMEOUT
                 OPTION_DB_USER OPTION_BACKUP_USER OPTION_COMMAND_PSQL OPTION_COMMAND_PSQL_OPTION OPTION_COMMAND_REMOTE
                 OPTION_RESTORE_TABLESPACE_MAP

                 OPTION_TEST OPTION_TEST_DELAY OPTION_TEST_NO_FORK);

####################################################################################################################################
# Option Defaults
####################################################################################################################################
use constant
{
    OPTION_DEFAULT_BUFFER_SIZE                  => 4194304,
    OPTION_DEFAULT_BUFFER_SIZE_MIN              => 16384,
    OPTION_DEFAULT_BUFFER_SIZE_MAX              => 8388608,

    OPTION_DEFAULT_COMPRESS                     => true,
    OPTION_DEFAULT_COMPRESS_LEVEL               => 6,
    OPTION_DEFAULT_COMPRESS_LEVEL_MIN           => 0,
    OPTION_DEFAULT_COMPRESS_LEVEL_MAX           => 9,
    OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK       => 3,
    OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN   => 0,
    OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX   => 9,

    OPTION_DEFAULT_CONFIG                       => '/etc/pg_backrest.conf',
    OPTION_DEFAULT_LOG_LEVEL_CONSOLE            => lc(WARN),
    OPTION_DEFAULT_LOG_LEVEL_FILE               => lc(INFO),
    OPTION_DEFAULT_THREAD_MAX                   => 1,

    OPTION_DEFAULT_ARCHIVE_ASYNC                => false,

    OPTION_DEFAULT_COMMAND_PSQL                 => '/usr/bin/psql -X',
    OPTION_DEFAULT_COMMAND_REMOTE               => dirname(abs_path($0)) . '/pg_backrest_remote.pl',

    OPTION_DEFAULT_BACKUP_ARCHIVE_CHECK         => true,
    OPTION_DEFAULT_BACKUP_ARCHIVE_COPY          => false,
    OPTION_DEFAULT_BACKUP_FORCE                 => false,
    OPTION_DEFAULT_BACKUP_HARDLINK              => false,
    OPTION_DEFAULT_BACKUP_NO_START_STOP         => false,
    OPTION_DEFAULT_BACKUP_START_FAST            => false,
    OPTION_DEFAULT_BACKUP_TYPE                  => BACKUP_TYPE_INCR,

    OPTION_DEFAULT_REPO_PATH                    => '/var/lib/backup',

    OPTION_DEFAULT_RESTORE_DELTA                => false,
    OPTION_DEFAULT_RESTORE_FORCE                => false,
    OPTION_DEFAULT_RESTORE_SET                  => 'latest',
    OPTION_DEFAULT_RESTORE_TYPE                 => RECOVERY_TYPE_DEFAULT,
    OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE     => false,
    OPTION_DEFAULT_RESTORE_TARGET_RESUME        => false,

    OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE       => BACKUP_TYPE_FULL,
    OPTION_DEFAULT_RETENTION_MIN                => 1,
    OPTION_DEFAULT_RETENTION_MAX                => 999999999,

    OPTION_DEFAULT_TEST                         => false,
    OPTION_DEFAULT_TEST_DELAY                   => 5,
    OPTION_DEFAULT_TEST_NO_FORK                 => false
};

push @EXPORT, qw(OPTION_DEFAULT_BUFFER_SIZE OPTION_DEFAULT_COMPRESS OPTION_DEFAULT_CONFIG OPTION_LEVEL_CONSOLE OPTION_LEVEL_FILE
                 OPTION_DEFAULT_THREAD_MAX

                 OPTION_DEFAULT_COMPRESS OPTION_DEFAULT_COMPRESS_LEVEL OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK
                 OPTION_DEFAULT_COMMAND_REMOTE

                 OPTION_DEFAULT_BACKUP_FORCE OPTION_DEFAULT_BACKUP_NO_START_STOP OPTION_DEFAULT_BACKUP_TYPE

                 OPTION_DEFAULT_RESTORE_DELTA OPTION_DEFAULT_RESTORE_FORCE OPTION_DEFAULT_RESTORE_SET OPTION_DEFAULT_RESTORE_TYPE
                 OPTION_DEFAULT_RESTORE_TARGET_EXCLUSIVE OPTION_DEFAULT_RESTORE_TARGET_RESUME

                 OPTION_DEFAULT_TEST OPTION_DEFAULT_TEST_DELAY OPTION_DEFAULT_TEST_NO_FORK);

####################################################################################################################################
# Option Rules
####################################################################################################################################
use constant
{
    OPTION_RULE_ALLOW_LIST       => 'allow-list',
    OPTION_RULE_ALLOW_RANGE      => 'allow-range',
    OPTION_RULE_DEFAULT          => 'default',
    OPTION_RULE_DEPEND           => 'depend',
    OPTION_RULE_DEPEND_OPTION    => 'depend-option',
    OPTION_RULE_DEPEND_LIST      => 'depend-list',
    OPTION_RULE_DEPEND_VALUE     => 'depend-value',
    OPTION_RULE_NEGATE           => 'negate',
    OPTION_RULE_OPERATION        => 'operation',
    OPTION_RULE_REQUIRED         => 'required',
    OPTION_RULE_SECTION          => 'section',
    OPTION_RULE_SECTION_INHERIT  => 'section-inherit',
    OPTION_RULE_TYPE             => 'type'
};

push @EXPORT, qw(OPTION_RULE_ALLOW_LIST OPTION_RULE_ALLOW_RANGE OPTION_RULE_DEFAULT OPTION_RULE_DEPEND OPTION_RULE_DEPEND_OPTION
                 OPTION_RULE_DEPEND_LIST OPTION_RULE_DEPEND_VALUE OPTION_RULE_NEGATE OPTION_RULE_OPERATION OPTION_RULE_REQUIRED
                 OPTION_RULE_SECTION OPTION_RULE_SECTION_INHERIT OPTION_RULE_TYPE);

####################################################################################################################################
# Option Types
####################################################################################################################################
use constant
{
    OPTION_TYPE_BOOLEAN      => 'boolean',
    OPTION_TYPE_FLOAT        => 'float',
    OPTION_TYPE_HASH         => 'hash',
    OPTION_TYPE_INTEGER      => 'integer',
    OPTION_TYPE_STRING       => 'string'
};

push @EXPORT, qw(OPTION_TYPE_BOOLEAN OPTION_TYPE_FLOAT OPTION_TYPE_INTEGER OPTION_TYPE_STRING);

####################################################################################################################################
# Option Rule Hash
####################################################################################################################################
my %oOptionRule =
(
    # Command-line-only option rule
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_CONFIG =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_CONFIG,
        &OPTION_RULE_NEGATE => true
    },

    &OPTION_DELTA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_DELTA,
            }
        }
    },

    &OPTION_FORCE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_FORCE,
            },

            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_FORCE,
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION  => OPTION_NO_START_STOP,
                    &OPTION_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &OPTION_NO_START_STOP =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_NO_START_STOP
            }
        }
    },

    &OPTION_SET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TYPE,
            }
        }
    },

    &OPTION_STANZA =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING
    },

    &OPTION_TARGET =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
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
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
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

    &OPTION_TARGET_RESUME =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TARGET_RESUME,
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
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE =>
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
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &BACKUP_TYPE_FULL => true,
                    &BACKUP_TYPE_DIFF => true,
                    &BACKUP_TYPE_INCR => true,
                }
            },

            &OP_RESTORE =>
            {
                &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RESTORE_TYPE,
                &OPTION_RULE_ALLOW_LIST =>
                {
                    &RECOVERY_TYPE_NAME     => true,
                    &RECOVERY_TYPE_TIME     => true,
                    &RECOVERY_TYPE_XID      => true,
                    &RECOVERY_TYPE_PRESERVE => true,
                    &RECOVERY_TYPE_NONE     => true,
                    &RECOVERY_TYPE_DEFAULT  => true
                }
            }
        }
    },

    # Command-line/conf option rules
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_COMMAND_REMOTE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMMAND_REMOTE,
        &OPTION_RULE_SECTION => CONFIG_SECTION_COMMAND,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_COMMAND_PSQL =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMMAND_PSQL,
        &OPTION_RULE_SECTION => CONFIG_SECTION_COMMAND,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        }
    },

    &OPTION_COMMAND_PSQL_OPTION =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_SECTION => CONFIG_SECTION_COMMAND,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        },
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_COMMAND_PSQL
        }
    },

    &OPTION_ARCHIVE_ASYNC =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_ARCHIVE_ASYNC,
        &OPTION_RULE_SECTION => CONFIG_SECTION_ARCHIVE,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_PUSH => true
        }
    },

    &OPTION_DB_HOST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        }
    },

    &OPTION_DB_USER =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        },
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_DB_HOST
        }
    },

    &OPTION_BACKUP_HOST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_BACKUP,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_RESTORE => true
        },
    },

    &OPTION_BACKUP_USER =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_SECTION => CONFIG_SECTION_BACKUP,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_RESTORE => true
        },
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_BACKUP_HOST
        }
    },

    &OPTION_REPO_PATH =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_REPO_PATH,
        &OPTION_RULE_SECTION => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_BACKUP => true,
            &OP_RESTORE => true,
            &OP_EXPIRE => true
        },
    },

    &OPTION_REPO_REMOTE_PATH =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_RESTORE => true
        },
    },

    &OPTION_DB_PATH =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_SECTION => CONFIG_SECTION_STANZA,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &OP_ARCHIVE_PUSH =>
            {
                &OPTION_RULE_REQUIRED => false
            },
            &OP_BACKUP => true
        },
    },

    &OPTION_BUFFER_SIZE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BUFFER_SIZE,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_BUFFER_SIZE_MIN, OPTION_DEFAULT_BUFFER_SIZE_MAX]
    },

    &OPTION_ARCHIVE_MAX_MB =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_ARCHIVE,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_PUSH => true
        }
    },

    &OPTION_BACKUP_ARCHIVE_CHECK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_ARCHIVE_CHECK,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        }
    },

    &OPTION_BACKUP_ARCHIVE_COPY =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_ARCHIVE_COPY,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP =>
            {
                &OPTION_RULE_DEPEND =>
                {
                    &OPTION_RULE_DEPEND_OPTION  => OPTION_BACKUP_ARCHIVE_CHECK,
                    &OPTION_RULE_DEPEND_VALUE   => true
                }
            }
        }
    },

    &OPTION_COMPRESS =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_COMPRESS_LEVEL =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS_LEVEL,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_COMPRESS_LEVEL_MIN, OPTION_DEFAULT_COMPRESS_LEVEL_MAX],
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_COMPRESS_LEVEL_NETWORK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MIN, OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK_MAX],
        &OPTION_RULE_OPERATION =>
        {
            &OP_ARCHIVE_GET => true,
            &OP_ARCHIVE_PUSH => true,
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_HARDLINK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_HARDLINK,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        }
    },

    &OPTION_LOG_LEVEL_CONSOLE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOG_LEVEL_CONSOLE,
        &OPTION_RULE_SECTION => CONFIG_SECTION_LOG,
        &OPTION_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        }
    },

    &OPTION_LOG_LEVEL_FILE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_LOG_LEVEL_FILE,
        &OPTION_RULE_SECTION => CONFIG_SECTION_LOG,
        &OPTION_RULE_ALLOW_LIST =>
        {
            lc(OFF)    => true,
            lc(ERROR)  => true,
            lc(WARN)   => true,
            lc(INFO)   => true,
            lc(DEBUG)  => true,
            lc(TRACE)  => true
        }
    },

    &OPTION_RESTORE_TABLESPACE_MAP =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_RESTORE_TABLESPACE_MAP,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE => 1
        },
    },

    &OPTION_RESTORE_RECOVERY_SETTING =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_HASH,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_RESTORE_RECOVERY_SETTING,
        &OPTION_RULE_OPERATION =>
        {
            &OP_RESTORE => 1
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

    &OPTION_RETENTION_ARCHIVE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_EXPIRE,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_EXPIRE => true
        }
    },

    &OPTION_RETENTION_ARCHIVE_TYPE =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_STRING,
        &OPTION_RULE_REQUIRED => true,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_RETENTION_ARCHIVE_TYPE,
        &OPTION_RULE_SECTION => CONFIG_SECTION_EXPIRE,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_EXPIRE => true
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
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_EXPIRE,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_EXPIRE => true
        }
    },

    &OPTION_RETENTION_FULL =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => CONFIG_SECTION_EXPIRE,
        &OPTION_RULE_ALLOW_RANGE => [OPTION_DEFAULT_RETENTION_MIN, OPTION_DEFAULT_RETENTION_MAX],
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_EXPIRE => true
        }
    },

    &OPTION_START_FAST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_BACKUP_START_FAST,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true
        }
    },

    &OPTION_THREAD_MAX =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_THREAD_MAX,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    &OPTION_THREAD_TIMEOUT =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_INTEGER,
        &OPTION_RULE_REQUIRED => false,
        &OPTION_RULE_SECTION => true,
        &OPTION_RULE_SECTION_INHERIT => CONFIG_SECTION_GENERAL,
        &OPTION_RULE_OPERATION =>
        {
            &OP_BACKUP => true,
            &OP_RESTORE => true
        }
    },

    # Command-line-only test option rules
    #-------------------------------------------------------------------------------------------------------------------------------
    &OPTION_TEST =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST
    },

    &OPTION_TEST_DELAY =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_FLOAT,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_DELAY,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST,
            &OPTION_RULE_DEPEND_VALUE => true
        }
    },

    &OPTION_TEST_NO_FORK =>
    {
        &OPTION_RULE_TYPE => OPTION_TYPE_BOOLEAN,
        &OPTION_RULE_DEFAULT => OPTION_DEFAULT_TEST_NO_FORK,
        &OPTION_RULE_DEPEND =>
        {
            &OPTION_RULE_DEPEND_OPTION => OPTION_TEST
        }
    }
);

####################################################################################################################################
# Global variables
####################################################################################################################################
my %oOption;            # Option hash
my $strOperation;       # Operation (backup, archive-get, ...)
my $strRemoteType;      # Remote type (DB, BACKUP, NONE)
my $oRemote;            # Global remote object that is created on first request (NOT THREADSAFE!)

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
    use Getopt::Long qw(GetOptions);
    my %oOptionTest;

    if (!GetOptions(\%oOptionTest, %oOptionAllow))
    {
        print "\n";
        print 'pg_backrest ' . version_get() . "\n";
        print "\n";
        use Pod::Usage;
        pod2usage(2);
    };

    # Display version and exit if requested
    if (defined($oOptionTest{&OPTION_VERSION}) || defined($oOptionTest{&OPTION_HELP}))
    {
        print 'pg_backrest ' . version_get() . "\n";

        if (!defined($oOptionTest{&OPTION_HELP}))
        {
            exit 0;
        }
    }

    # Display help and exit if requested
    if (defined($oOptionTest{&OPTION_HELP}))
    {
        print "\n";
        pod2usage();
        exit 0;
    }

    # Validate and store options
    optionValid(\%oOptionTest);

    # Replace command psql options if set
    if (optionTest(OPTION_COMMAND_PSQL) && optionTest(OPTION_COMMAND_PSQL_OPTION))
    {
        $oOption{&OPTION_COMMAND_PSQL} =~ s/\%option\%/$oOption{&OPTION_COMMAND_PSQL_OPTION}/g;
    }

    # Set repo-remote-path to repo-path if it is not set
    if (optionTest(OPTION_REPO_PATH) && !optionTest(OPTION_REPO_REMOTE_PATH))
    {
        $oOption{&OPTION_REPO_REMOTE_PATH} = optionGet(OPTION_REPO_PATH);
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
}

####################################################################################################################################
# optionValid
#
# Make sure the command-line options are valid based on the operation.
####################################################################################################################################
sub optionValid
{
    my $oOptionTest = shift;

    # Check that the operation is present and valid
    $strOperation = $ARGV[0];

    if (!defined($strOperation))
    {
        confess &log(ERROR, "operation must be specified", ERROR_OPERATION_REQUIRED);
    }

    if ($strOperation ne OP_ARCHIVE_GET &&
        $strOperation ne OP_ARCHIVE_PUSH &&
        $strOperation ne OP_BACKUP &&
        $strOperation ne OP_RESTORE &&
        $strOperation ne OP_EXPIRE)
    {
        confess &log(ERROR, "invalid operation ${strOperation}");
    }

    # Set the operation section - because of the various archive commands this is not always the operation
    my $strOperationSection;

    if (operationTest(OP_ARCHIVE_GET) || operationTest(OP_ARCHIVE_PUSH))
    {
        $strOperationSection = CONFIG_SECTION_ARCHIVE;
    }
    else
    {
        $strOperationSection = $strOperation;
    }

    # Hash to store contents of the config file.  The file will be loaded one the config dependency is resolved unless all options
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

            # If the operation has rules store them for later evaluation
            my $oOperationRule = optionOperationRule($strOption, $strOperation);

            # Check dependency for the operation then for the option
            my $bDependResolved = true;
            my $oDepend = defined($oOperationRule) ? $$oOperationRule{&OPTION_RULE_DEPEND} :
                                                     $oOptionRule{$strOption}{&OPTION_RULE_DEPEND};
            my $strDependOption;
            my $strDependValue;
            my $strDependType;

            if (defined($oDepend))
            {
                # Check if the depend option has a value
                $strDependOption = $$oDepend{&OPTION_RULE_DEPEND_OPTION};
                $strDependValue = $oOption{$strDependOption};

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

            # If the option value is undefined and not negated, see if it can be loaded from pg_backrest.conf
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
                if ($bConfigExists && defined($oOption{&OPTION_CONFIG}))
                {
                    # Attempt to load the config file if it has not been loaded
                    if (!defined($oConfig))
                    {
                        my $strConfigFile = $oOption{&OPTION_CONFIG};
                        $bConfigExists = -e $strConfigFile;

                        if ($bConfigExists)
                        {
                            if (!-f $strConfigFile)
                            {
                                confess &log(ERROR, "'${strConfigFile}' is not a file", ERROR_FILE_INVALID);
                            }

                            $oConfig = ini_load($strConfigFile);
                        }
                    }

                    # Get the section that the value should be in
                    my $strSection = defined($oOptionRule{$strOption}{&OPTION_RULE_SECTION}) ?
                                         ($oOptionRule{$strOption}{&OPTION_RULE_SECTION} eq '1' ?
                                             $strOperationSection : $oOptionRule{$strOption}{&OPTION_RULE_SECTION}) : undef;

                    # Only look in the stanza section when $strSection = true
                    if ($strSection eq CONFIG_SECTION_STANZA)
                    {
                        $strValue = $$oConfig{optionGet(OPTION_STANZA)}{$strOption};
                    }
                    # Else do a full search
                    else
                    {
                        # First check in the stanza section
                        $strValue = $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_HASH ?
                                    $$oConfig{optionGet(OPTION_STANZA) . ":${strSection}"} :
                                    $$oConfig{optionGet(OPTION_STANZA) . ":${strSection}"}{$strOption};

                        # Else check for an inherited stanza section
                        if (!defined($strValue))
                        {
                            my $strInheritedSection = undef;

                            $strInheritedSection = $oOptionRule{$strOption}{&OPTION_RULE_SECTION_INHERIT};

                            if (defined($strInheritedSection))
                            {
                                $strValue = $$oConfig{optionGet(OPTION_STANZA) . ":${strInheritedSection}"}{$strOption};
                            }

                            # Else check the global section
                            if (!defined($strValue))
                            {
                                $strValue = $oOptionRule{$strOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_HASH ?
                                            $$oConfig{&CONFIG_GLOBAL . ":${strSection}"} :
                                            $$oConfig{&CONFIG_GLOBAL . ":${strSection}"}{$strOption};

                                # Else check the global inherited section
                                if (!defined($strValue) && defined($strInheritedSection))
                                {
                                    $strValue = $$oConfig{&CONFIG_GLOBAL . ":${strInheritedSection}"}{$strOption};
                                }
                            }
                        }
                    }

                    # Fix up data types
                    if (defined($strValue))
                    {
                        if ($strValue eq '')
                        {
                            $strValue = undef;
                        }
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
                    }
                }
            }

            if (defined($oDepend) && !$bDependResolved && defined($strValue))
            {
                my $strError = "option '${strOption}' not valid without option '${strDependOption}'";

                if ($strDependType eq 'source')
                {
                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                # If a depend value exists, make sure the option value matches
                if ($strDependType eq 'value')
                {
                    if ($oOptionRule{$strDependOption}{&OPTION_RULE_TYPE} eq OPTION_TYPE_BOOLEAN)
                    {
                        if (!$$oDepend{&OPTION_RULE_DEPEND_VALUE})
                        {
                            confess &log(ASSERT, "no error has been created for unused case where depend value = false");
                        }
                    }
                    else
                    {
                        $strError .= " = '$$oDepend{&OPTION_RULE_DEPEND_VALUE}'";
                    }

                    confess &log(ERROR, $strError, ERROR_OPTION_INVALID);
                }

                # If a depend list exists, make sure the value is in the list
                if ($strDependType eq 'list')
                {
                    my @oyValue;

                    foreach my $strValue (sort(keys($$oDepend{&OPTION_RULE_DEPEND_LIST})))
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

                # Process an allow list for the operation then for the option
                my $oAllow = defined($oOperationRule) ? $$oOperationRule{&OPTION_RULE_ALLOW_LIST} :
                                                        $oOptionRule{$strOption}{&OPTION_RULE_ALLOW_LIST};

                if (defined($oAllow) && !defined($$oAllow{$strValue}))
                {
                    confess &log(ERROR, "'${strValue}' is not valid for '${strOption}' option", ERROR_OPTION_INVALID_VALUE);
                }

                # Process an allow range for the operation then for the option
                $oAllow = defined($oOperationRule) ? $$oOperationRule{&OPTION_RULE_ALLOW_RANGE} :
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
                        # Check for = and make sure there is a least one character on each side
                        my $iEqualPos = index($strItem, '=');

                        if ($iEqualPos < 1 || length($strItem) <= $iEqualPos + 1)
                        {
                            confess &log(ERROR, "'${strItem}' not valid key/value for '${strOption}' option",
                                                ERROR_OPTION_INVALID_PAIR);
                        }

                        # Check that the key has not already been set
                        my $strKey = substr($strItem, 0, $iEqualPos);

                        if (defined($oOption{$strOption}{$strKey}))
                        {
                            confess &log(ERROR, "'${$strItem}' already defined for '${strOption}' option",
                                                ERROR_OPTION_DUPLICATE_KEY);
                        }

                        $oOption{$strOption}{$strKey} = substr($strItem, $iEqualPos + 1);
                    }
                }
                else
                {
                    $oOption{$strOption} = $strValue;
                }
            }
            # Else try to set a default
            elsif ($bDependResolved &&
                   (!defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) ||
                    defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation})))
            {
                # Check for default in operation then option
                my $strDefault = optionDefault($strOption, $strOperation);

                # If default is defined
                if (defined($strDefault))
                {
                    # Only set default if dependency is resolved
                    $oOption{$strOption} = $strDefault if !$bNegate;
                }
                # Else check required
                elsif (optionRequired($strOption, $strOperation))
                {
                    confess &log(ERROR, "${strOperation} operation requires option: ${strOption}", ERROR_OPTION_REQUIRED);
                }
            }

            $oOptionResolved{$strOption} = true;
        }
    }
}

####################################################################################################################################
# optionOperationRule
#
# Returns the option rules based on the operation.
####################################################################################################################################
sub optionOperationRule
{
    my $strOption = shift;
    my $strOperation = shift;

    if (defined($strOperation))
    {
        return defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}) &&
               defined($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) &&
               ref($oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation}) eq 'HASH' ?
               $oOptionRule{$strOption}{&OPTION_RULE_OPERATION}{$strOperation} : undef;
    }

    return undef;
}

####################################################################################################################################
# optionRequired
#
# Is the option required for this operation?
####################################################################################################################################
sub optionRequired
{
    my $strOption = shift;
    my $strOperation = shift;

    # Get the operation rule
    my $oOperationRule = optionOperationRule($strOption, $strOperation);

    # Check for required in operation then option
    my $bRequired = defined($oOperationRule) ? $$oOperationRule{&OPTION_RULE_REQUIRED} :
                                               $oOptionRule{$strOption}{&OPTION_RULE_REQUIRED};

    # Return required
    return !defined($bRequired) || $bRequired;
}


####################################################################################################################################
# optionDefault
#
# Does the option have a default for this operation?
####################################################################################################################################
sub optionDefault
{
    my $strOption = shift;
    my $strOperation = shift;

    # Get the operation rule
    my $oOperationRule = optionOperationRule($strOption, $strOperation);

    # Check for default in operation
    my $strDefault = defined($oOperationRule) ? $$oOperationRule{&OPTION_RULE_DEFAULT} : undef;

    # If defined return, else try to grab the global default
    return defined($strDefault) ? $strDefault : $oOptionRule{$strOption}{&OPTION_RULE_DEFAULT};
}

####################################################################################################################################
# operationGet
#
# Get the current operation.
####################################################################################################################################
sub operationGet
{
    return $strOperation;
}

####################################################################################################################################
# operationTest
#
# Test the current operation.
####################################################################################################################################
sub operationTest
{
    my $strOperationTest = shift;

    return $strOperationTest eq $strOperation;
}

####################################################################################################################################
# operationSet
#
# Set current operation (usually for triggering follow-on operations).
####################################################################################################################################
sub operationSet
{
    my $strValue = shift;

    $strOperation = $strValue;
}

####################################################################################################################################
# optionGet
#
# Get option value.
####################################################################################################################################
sub optionGet
{
    my $strOption = shift;
    my $bRequired = shift;

    if (!defined($oOption{$strOption}) && (!defined($bRequired) || $bRequired))
    {
        confess &log(ASSERT, "option ${strOption} is required");
    }

    return $oOption{$strOption};
}

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

    return defined($oOption{$strOption});
}

####################################################################################################################################
# optionRemoteType
#
# Returns the remote type.
####################################################################################################################################
sub optionRemoteType
{
    return $strRemoteType;
}

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

####################################################################################################################################
# optionRemote
#
# Get the remote object or create it if does not exist.  Shared remotes are used because they create an SSH connection to the remote
# host and the number of these connections should be minimized.  A remote can be shared without a single thread - for new threads
# clone() should be called on the shared remote.
####################################################################################################################################
sub optionRemote
{
    my $bForceLocal = shift;
    my $bStore = shift;

    # If force local or remote = NONE then create a local remote and return it
    if ((defined($bForceLocal) && $bForceLocal) || optionRemoteTypeTest(NONE))
    {
        return new BackRest::Remote
        (
            undef, undef, undef, undef, undef,
            optionGet(OPTION_BUFFER_SIZE),
            operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL : optionGet(OPTION_COMPRESS_LEVEL),
            operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK : optionGet(OPTION_COMPRESS_LEVEL_NETWORK)
        );
    }

    # Return the remote if is already defined
    if (defined($oRemote))
    {
        return $oRemote;
    }

    # Return the remote when required
    my $oRemoteTemp = new BackRest::Remote
    (
        optionRemoteTypeTest(DB) ? optionGet(OPTION_DB_HOST) : optionGet(OPTION_BACKUP_HOST),
        optionRemoteTypeTest(DB) ? optionGet(OPTION_DB_USER) : optionGet(OPTION_BACKUP_USER),
        optionGet(OPTION_COMMAND_REMOTE),
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_REMOTE_PATH),
        optionGet(OPTION_BUFFER_SIZE),
        operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL : optionGet(OPTION_COMPRESS_LEVEL),
        operationTest(OP_EXPIRE) ? OPTION_DEFAULT_COMPRESS_LEVEL_NETWORK : optionGet(OPTION_COMPRESS_LEVEL_NETWORK)
    );

    if ($bStore)
    {
        $oRemote = $oRemoteTemp;
    }

    return $oRemoteTemp;
}

####################################################################################################################################
# remoteDestroy
#
# Undefined the remote if it is stored locally.
####################################################################################################################################
sub remoteDestroy
{
    if (defined($oRemote))
    {
        undef($oRemote);
    }
}

####################################################################################################################################
# optionRemoteTest
#
# Test if the remote DB or BACKUP.
####################################################################################################################################
sub optionRemoteTest
{
    return $strRemoteType ne NONE ? true : false;
}

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

1;
