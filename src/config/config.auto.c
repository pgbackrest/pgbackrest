/***********************************************************************************************************************************
Command and Option Configuration

Automatically generated by 'make build-config' -- do not modify directly.
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Command data
***********************************************************************************************************************************/
static const ConfigCommandData configCommandData[CFG_COMMAND_TOTAL] = CONFIG_COMMAND_LIST
(
    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_ARCHIVE_GET)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeArchive)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_ARCHIVE_PUSH)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(true)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeArchive)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_BACKUP)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(true)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(true)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeBackup)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_CHECK)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_EXPIRE)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(true)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeBackup)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_HELP)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_INFO)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_REPO_CREATE)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_REPO_GET)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_REPO_LS)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_REPO_PUT)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_REPO_RM)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_RESTORE)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_STANZA_CREATE)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(true)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeAll)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_STANZA_DELETE)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(true)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeAll)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_STANZA_UPGRADE)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(true)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeAll)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_START)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_STOP)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_VERIFY)

        CONFIG_COMMAND_LOG_FILE(true)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelInfo)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )

    CONFIG_COMMAND
    (
        CONFIG_COMMAND_NAME(CFGCMD_VERSION)

        CONFIG_COMMAND_LOG_FILE(false)
        CONFIG_COMMAND_LOG_LEVEL_DEFAULT(logLevelDebug)
        CONFIG_COMMAND_LOCK_REQUIRED(false)
        CONFIG_COMMAND_LOCK_REMOTE_REQUIRED(false)
        CONFIG_COMMAND_LOCK_TYPE(lockTypeNone)
    )
)
