## cfgOptionRuleAllowList

Is there an allow list for this option?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| AllowList | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| AllowList | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgOptionRuleAllowListValue

Get value from allowed list.

### Truth Table:

| Suffix | uiCommandId | uiOptionId | uiValueId | Result |
| ------ | ----------- | ---------- | --------- | ------ |
| AllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `0` | `"full"` |
| AllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `1` | `"diff"` |
| AllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `2` | `"incr"` |
| AllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `0` | `"db"` |
| AllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `1` | `"backup"` |
| AllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `0` | `"db"` |
| AllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `1` | `"backup"` |
| AllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `0` | `"name"` |
| AllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `1` | `"time"` |
| AllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `2` | `"xid"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `0` | `"16384"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `1` | `"32768"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `2` | `"65536"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `3` | `"131072"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `4` | `"262144"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `5` | `"524288"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `6` | `"1048576"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `7` | `"2097152"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `8` | `"4194304"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `9` | `"8388608"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `10` | `"16777216"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `0` | `"off"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `1` | `"error"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `2` | `"warn"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `3` | `"info"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `4` | `"detail"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `5` | `"debug"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `6` | `"trace"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `0` | `"off"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `1` | `"error"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `2` | `"warn"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `3` | `"info"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `4` | `"detail"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `5` | `"debug"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `6` | `"trace"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `0` | `"off"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `1` | `"error"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `2` | `"warn"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `3` | `"info"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `4` | `"detail"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `5` | `"debug"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `6` | `"trace"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `0` | `"text"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `1` | `"json"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `0` | `"cifs"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `1` | `"posix"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `2` | `"s3"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `0` | `"full"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `1` | `"diff"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `2` | `"incr"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"pause"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"promote"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"shutdown"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `3` | `"preserve"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `4` | `"none"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `5` | `"immediate"` |
| AllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `6` | `"default"` |

## cfgOptionRuleAllowListValueTotal

Total number of values allowed.

### Truth Table:

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| AllowListValueTotal | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `3` |
| AllowListValueTotal | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `2` |
| AllowListValueTotal | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `2` |
| AllowListValueTotal | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `7` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `11` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `7` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `7` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `7` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_OUTPUT` | `2` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `3` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `3` |
| AllowListValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |

## cfgOptionRuleAllowRange

Is the option constrained to a range?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| AllowRange | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `true` |
| AllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `true` |

## cfgOptionRuleAllowRangeMax

Maximum value allowed (if the option is constrained to a range).

### Truth Table:

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `86400` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `9` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `9` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `604800` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `96` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `604800` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `999999999` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `999999999` |
| AllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `999999999` |

## cfgOptionRuleAllowRangeMin

Minimum value allowed (if the option is constrained to a range).

### Truth Table:

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `0.1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `0` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `0` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `0.1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `0.1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `1` |
| AllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `1` |

## cfgOptionRuleNameAlt

Alternate name for the option primarily used for deprecated names.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| NameAlt | `CFGOPT_DB1_CMD` | `"db-cmd"` |
| NameAlt | `CFGOPT_DB1_CONFIG` | `"db-config"` |
| NameAlt | `CFGOPT_DB1_HOST` | `"db-host"` |
| NameAlt | `CFGOPT_DB1_PATH` | `"db-path"` |
| NameAlt | `CFGOPT_DB1_PORT` | `"db-port"` |
| NameAlt | `CFGOPT_DB1_SOCKET_PATH` | `"db-socket-path"` |
| NameAlt | `CFGOPT_DB1_SSH_PORT` | `"db-ssh-port"` |
| NameAlt | `CFGOPT_DB1_USER` | `"db-user"` |
| NameAlt | `CFGOPT_PROCESS_MAX` | `"thread-max"` |

## cfgOptionRuleDefault

Default value.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| Default | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `"incr"` |
| Default | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `"default"` |
| Default | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `"60"` |
| Default | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `"backrest"` |
| Default | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `"4194304"` |
| Default | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `"ssh"` |
| Default | _\<ANY\>_ | `CFGOPT_COMPRESS` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `"6"` |
| Default | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"3"` |
| Default | _\<ANY\>_ | `CFGOPT_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `"1800"` |
| Default | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB1_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB2_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB2_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB3_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB3_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB4_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB4_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB5_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB5_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB6_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB6_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB7_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB7_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `"/etc/pgbackrest.conf"` |
| Default | _\<ANY\>_ | `CFGOPT_DB8_PORT` | `"5432"` |
| Default | _\<ANY\>_ | `CFGOPT_DB8_USER` | `"postgres"` |
| Default | _\<ANY\>_ | `CFGOPT_DELTA` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_FORCE` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_HARDLINK` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `"/tmp/pgbackrest"` |
| Default | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `"warn"` |
| Default | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `"info"` |
| Default | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `"warn"` |
| Default | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `"/var/log/pgbackrest"` |
| Default | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"1073741824"` |
| Default | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_ONLINE` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_OUTPUT` | `"text"` |
| Default | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `"1830"` |
| Default | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `"/var/lib/pgbackrest"` |
| Default | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `"posix"` |
| Default | _\<ANY\>_ | `CFGOPT_RESUME` | `"1"` |
| Default | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"full"` |
| Default | _\<ANY\>_ | `CFGOPT_SET` | `"latest"` |
| Default | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `"/var/spool/pgbackrest"` |
| Default | _\<ANY\>_ | `CFGOPT_START_FAST` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `"pause"` |
| Default | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_TEST` | `"0"` |
| Default | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `"5"` |

## cfgOptionRuleDepend

Does the option have a dependency on another option?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| Depend | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB1_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB2_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB3_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB4_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB5_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB6_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB7_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_DB8_USER` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| Depend | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `true` |

## cfgOptionRuleDependOption

Name of the option that this option depends in order to be set.

### Truth Table:

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| DependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `CFGOPT_ONLINE` |
| DependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `CFGOPT_ARCHIVE_CHECK` |
| DependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `CFGOPT_BACKUP_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `CFGOPT_BACKUP_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPT_BACKUP_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `CFGOPT_BACKUP_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `CFGOPT_DB1_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `CFGOPT_DB1_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `CFGOPT_DB1_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB1_USER` | `CFGOPT_DB1_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `CFGOPT_DB2_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `CFGOPT_DB2_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `CFGOPT_DB2_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB2_USER` | `CFGOPT_DB2_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `CFGOPT_DB3_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `CFGOPT_DB3_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `CFGOPT_DB3_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB3_USER` | `CFGOPT_DB3_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `CFGOPT_DB4_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `CFGOPT_DB4_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `CFGOPT_DB4_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB4_USER` | `CFGOPT_DB4_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `CFGOPT_DB5_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `CFGOPT_DB5_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `CFGOPT_DB5_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB5_USER` | `CFGOPT_DB5_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `CFGOPT_DB6_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `CFGOPT_DB6_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `CFGOPT_DB6_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB6_USER` | `CFGOPT_DB6_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `CFGOPT_DB7_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `CFGOPT_DB7_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `CFGOPT_DB7_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB7_USER` | `CFGOPT_DB7_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `CFGOPT_DB8_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `CFGOPT_DB8_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `CFGOPT_DB8_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_DB8_USER` | `CFGOPT_DB8_HOST` |
| DependOption | _\<ANY\>_ | `CFGOPT_FORCE` | `CFGOPT_ONLINE` |
| DependOption | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `CFGOPT_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPT_REPO_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `CFGOPT_ARCHIVE_ASYNC` |
| DependOption | _\<ANY\>_ | `CFGOPT_TARGET` | `CFGOPT_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `CFGOPT_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPT_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `CFGOPT_TYPE` |
| DependOption | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `CFGOPT_TEST` |
| DependOption | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `CFGOPT_TEST` |

## cfgOptionRuleDependValue

The depend option must have one of these values before this option is set.

### Truth Table:

| Suffix | uiCommandId | uiOptionId | uiValueId | Result |
| ------ | ----------- | ---------- | --------- | ------ |
| DependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `0` | `"1"` |
| DependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `0` | `"1"` |
| DependValue | _\<ANY\>_ | `CFGOPT_FORCE` | `0` | `"0"` |
| DependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `0` | `"default"` |
| DependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `1` | `"name"` |
| DependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `2` | `"time"` |
| DependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `3` | `"xid"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `0` | `"s3"` |
| DependValue | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `0` | `"1"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `0` | `"name"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `1` | `"time"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `2` | `"xid"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"name"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"time"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"xid"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `0` | `"time"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `1` | `"xid"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `0` | `"default"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `1` | `"name"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `2` | `"time"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `3` | `"xid"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `0` | `"1"` |
| DependValue | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `0` | `"1"` |

## cfgOptionRuleDependValueTotal

Total depend values for this option.

### Truth Table:

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_USER` | `0` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_FORCE` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `4` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET` | `3` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `2` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `4` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `1` |
| DependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `1` |

## cfgOptionRuleValueHash

Is the option a true hash or just a list of keys?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| ValueHash | `CFGOPT_LINK_MAP` | `true` |
| ValueHash | `CFGOPT_RECOVERY_OPTION` | `true` |
| ValueHash | `CFGOPT_TABLESPACE_MAP` | `true` |
| ValueHash | `CFGOPT_TEST_POINT` | `true` |

## cfgOptionRuleHint

Some clue as to what value the user should provide when the option is missing but required.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| Hint | _\<ANY\>_ | `CFGOPT_DB1_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB2_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB3_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB4_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB5_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB6_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB7_PATH` | `"does this stanza exist?"` |
| Hint | _\<ANY\>_ | `CFGOPT_DB8_PATH` | `"does this stanza exist?"` |

## cfgOptionIndexTotal

Total index values allowed.

### Truth Table:

Functions that return `1` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| IndexTotal | `CFGOPT_DB1_CMD` | `8` |
| IndexTotal | `CFGOPT_DB1_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB1_HOST` | `8` |
| IndexTotal | `CFGOPT_DB1_PATH` | `8` |
| IndexTotal | `CFGOPT_DB1_PORT` | `8` |
| IndexTotal | `CFGOPT_DB1_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB1_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB1_USER` | `8` |
| IndexTotal | `CFGOPT_DB2_CMD` | `8` |
| IndexTotal | `CFGOPT_DB2_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB2_HOST` | `8` |
| IndexTotal | `CFGOPT_DB2_PATH` | `8` |
| IndexTotal | `CFGOPT_DB2_PORT` | `8` |
| IndexTotal | `CFGOPT_DB2_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB2_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB2_USER` | `8` |
| IndexTotal | `CFGOPT_DB3_CMD` | `8` |
| IndexTotal | `CFGOPT_DB3_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB3_HOST` | `8` |
| IndexTotal | `CFGOPT_DB3_PATH` | `8` |
| IndexTotal | `CFGOPT_DB3_PORT` | `8` |
| IndexTotal | `CFGOPT_DB3_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB3_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB3_USER` | `8` |
| IndexTotal | `CFGOPT_DB4_CMD` | `8` |
| IndexTotal | `CFGOPT_DB4_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB4_HOST` | `8` |
| IndexTotal | `CFGOPT_DB4_PATH` | `8` |
| IndexTotal | `CFGOPT_DB4_PORT` | `8` |
| IndexTotal | `CFGOPT_DB4_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB4_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB4_USER` | `8` |
| IndexTotal | `CFGOPT_DB5_CMD` | `8` |
| IndexTotal | `CFGOPT_DB5_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB5_HOST` | `8` |
| IndexTotal | `CFGOPT_DB5_PATH` | `8` |
| IndexTotal | `CFGOPT_DB5_PORT` | `8` |
| IndexTotal | `CFGOPT_DB5_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB5_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB5_USER` | `8` |
| IndexTotal | `CFGOPT_DB6_CMD` | `8` |
| IndexTotal | `CFGOPT_DB6_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB6_HOST` | `8` |
| IndexTotal | `CFGOPT_DB6_PATH` | `8` |
| IndexTotal | `CFGOPT_DB6_PORT` | `8` |
| IndexTotal | `CFGOPT_DB6_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB6_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB6_USER` | `8` |
| IndexTotal | `CFGOPT_DB7_CMD` | `8` |
| IndexTotal | `CFGOPT_DB7_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB7_HOST` | `8` |
| IndexTotal | `CFGOPT_DB7_PATH` | `8` |
| IndexTotal | `CFGOPT_DB7_PORT` | `8` |
| IndexTotal | `CFGOPT_DB7_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB7_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB7_USER` | `8` |
| IndexTotal | `CFGOPT_DB8_CMD` | `8` |
| IndexTotal | `CFGOPT_DB8_CONFIG` | `8` |
| IndexTotal | `CFGOPT_DB8_HOST` | `8` |
| IndexTotal | `CFGOPT_DB8_PATH` | `8` |
| IndexTotal | `CFGOPT_DB8_PORT` | `8` |
| IndexTotal | `CFGOPT_DB8_SOCKET_PATH` | `8` |
| IndexTotal | `CFGOPT_DB8_SSH_PORT` | `8` |
| IndexTotal | `CFGOPT_DB8_USER` | `8` |

## cfgOptionRuleNegate

Can the boolean option be negated?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| Negate | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| Negate | `CFGOPT_ARCHIVE_CHECK` | `true` |
| Negate | `CFGOPT_ARCHIVE_COPY` | `true` |
| Negate | `CFGOPT_BACKUP_STANDBY` | `true` |
| Negate | `CFGOPT_CHECKSUM_PAGE` | `true` |
| Negate | `CFGOPT_COMPRESS` | `true` |
| Negate | `CFGOPT_CONFIG` | `true` |
| Negate | `CFGOPT_HARDLINK` | `true` |
| Negate | `CFGOPT_LINK_ALL` | `true` |
| Negate | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Negate | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Negate | `CFGOPT_ONLINE` | `true` |
| Negate | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Negate | `CFGOPT_RESUME` | `true` |
| Negate | `CFGOPT_START_FAST` | `true` |
| Negate | `CFGOPT_STOP_AUTO` | `true` |

## cfgOptionRulePrefix

Prefix when the option has an index > 1 (e.g. "db").

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| Prefix | `CFGOPT_DB1_CMD` | `"db"` |
| Prefix | `CFGOPT_DB1_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB1_HOST` | `"db"` |
| Prefix | `CFGOPT_DB1_PATH` | `"db"` |
| Prefix | `CFGOPT_DB1_PORT` | `"db"` |
| Prefix | `CFGOPT_DB1_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB1_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB1_USER` | `"db"` |
| Prefix | `CFGOPT_DB2_CMD` | `"db"` |
| Prefix | `CFGOPT_DB2_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB2_HOST` | `"db"` |
| Prefix | `CFGOPT_DB2_PATH` | `"db"` |
| Prefix | `CFGOPT_DB2_PORT` | `"db"` |
| Prefix | `CFGOPT_DB2_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB2_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB2_USER` | `"db"` |
| Prefix | `CFGOPT_DB3_CMD` | `"db"` |
| Prefix | `CFGOPT_DB3_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB3_HOST` | `"db"` |
| Prefix | `CFGOPT_DB3_PATH` | `"db"` |
| Prefix | `CFGOPT_DB3_PORT` | `"db"` |
| Prefix | `CFGOPT_DB3_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB3_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB3_USER` | `"db"` |
| Prefix | `CFGOPT_DB4_CMD` | `"db"` |
| Prefix | `CFGOPT_DB4_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB4_HOST` | `"db"` |
| Prefix | `CFGOPT_DB4_PATH` | `"db"` |
| Prefix | `CFGOPT_DB4_PORT` | `"db"` |
| Prefix | `CFGOPT_DB4_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB4_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB4_USER` | `"db"` |
| Prefix | `CFGOPT_DB5_CMD` | `"db"` |
| Prefix | `CFGOPT_DB5_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB5_HOST` | `"db"` |
| Prefix | `CFGOPT_DB5_PATH` | `"db"` |
| Prefix | `CFGOPT_DB5_PORT` | `"db"` |
| Prefix | `CFGOPT_DB5_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB5_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB5_USER` | `"db"` |
| Prefix | `CFGOPT_DB6_CMD` | `"db"` |
| Prefix | `CFGOPT_DB6_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB6_HOST` | `"db"` |
| Prefix | `CFGOPT_DB6_PATH` | `"db"` |
| Prefix | `CFGOPT_DB6_PORT` | `"db"` |
| Prefix | `CFGOPT_DB6_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB6_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB6_USER` | `"db"` |
| Prefix | `CFGOPT_DB7_CMD` | `"db"` |
| Prefix | `CFGOPT_DB7_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB7_HOST` | `"db"` |
| Prefix | `CFGOPT_DB7_PATH` | `"db"` |
| Prefix | `CFGOPT_DB7_PORT` | `"db"` |
| Prefix | `CFGOPT_DB7_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB7_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB7_USER` | `"db"` |
| Prefix | `CFGOPT_DB8_CMD` | `"db"` |
| Prefix | `CFGOPT_DB8_CONFIG` | `"db"` |
| Prefix | `CFGOPT_DB8_HOST` | `"db"` |
| Prefix | `CFGOPT_DB8_PATH` | `"db"` |
| Prefix | `CFGOPT_DB8_PORT` | `"db"` |
| Prefix | `CFGOPT_DB8_SOCKET_PATH` | `"db"` |
| Prefix | `CFGOPT_DB8_SSH_PORT` | `"db"` |
| Prefix | `CFGOPT_DB8_USER` | `"db"` |

## cfgOptionRuleRequired

Is the option required?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| Required | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| Required | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| Required | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| Required | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| Required | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| Required | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| Required | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| Required | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_COMMAND` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_COMPRESS` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_CONFIG` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_DELTA` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_FORCE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_HARDLINK` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_HOST_ID` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_ONLINE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_RESUME` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_SET` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_START_FAST` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TEST` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| Required | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgOptionRuleSection

Section that the option belongs in, NULL means command-line only.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| Section | `CFGOPT_ARCHIVE_ASYNC` | `"global"` |
| Section | `CFGOPT_ARCHIVE_CHECK` | `"global"` |
| Section | `CFGOPT_ARCHIVE_COPY` | `"global"` |
| Section | `CFGOPT_ARCHIVE_MAX_MB` | `"global"` |
| Section | `CFGOPT_ARCHIVE_QUEUE_MAX` | `"global"` |
| Section | `CFGOPT_ARCHIVE_TIMEOUT` | `"global"` |
| Section | `CFGOPT_BACKUP_CMD` | `"global"` |
| Section | `CFGOPT_BACKUP_CONFIG` | `"global"` |
| Section | `CFGOPT_BACKUP_HOST` | `"global"` |
| Section | `CFGOPT_BACKUP_SSH_PORT` | `"global"` |
| Section | `CFGOPT_BACKUP_STANDBY` | `"global"` |
| Section | `CFGOPT_BACKUP_USER` | `"global"` |
| Section | `CFGOPT_BUFFER_SIZE` | `"global"` |
| Section | `CFGOPT_CHECKSUM_PAGE` | `"global"` |
| Section | `CFGOPT_CMD_SSH` | `"global"` |
| Section | `CFGOPT_COMPRESS` | `"global"` |
| Section | `CFGOPT_COMPRESS_LEVEL` | `"global"` |
| Section | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"global"` |
| Section | `CFGOPT_DB_INCLUDE` | `"global"` |
| Section | `CFGOPT_DB_TIMEOUT` | `"global"` |
| Section | `CFGOPT_DB1_CMD` | `"global"` |
| Section | `CFGOPT_DB1_CONFIG` | `"global"` |
| Section | `CFGOPT_DB1_HOST` | `"stanza"` |
| Section | `CFGOPT_DB1_PATH` | `"stanza"` |
| Section | `CFGOPT_DB1_PORT` | `"stanza"` |
| Section | `CFGOPT_DB1_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB1_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB1_USER` | `"stanza"` |
| Section | `CFGOPT_DB2_CMD` | `"global"` |
| Section | `CFGOPT_DB2_CONFIG` | `"global"` |
| Section | `CFGOPT_DB2_HOST` | `"stanza"` |
| Section | `CFGOPT_DB2_PATH` | `"stanza"` |
| Section | `CFGOPT_DB2_PORT` | `"stanza"` |
| Section | `CFGOPT_DB2_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB2_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB2_USER` | `"stanza"` |
| Section | `CFGOPT_DB3_CMD` | `"global"` |
| Section | `CFGOPT_DB3_CONFIG` | `"global"` |
| Section | `CFGOPT_DB3_HOST` | `"stanza"` |
| Section | `CFGOPT_DB3_PATH` | `"stanza"` |
| Section | `CFGOPT_DB3_PORT` | `"stanza"` |
| Section | `CFGOPT_DB3_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB3_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB3_USER` | `"stanza"` |
| Section | `CFGOPT_DB4_CMD` | `"global"` |
| Section | `CFGOPT_DB4_CONFIG` | `"global"` |
| Section | `CFGOPT_DB4_HOST` | `"stanza"` |
| Section | `CFGOPT_DB4_PATH` | `"stanza"` |
| Section | `CFGOPT_DB4_PORT` | `"stanza"` |
| Section | `CFGOPT_DB4_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB4_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB4_USER` | `"stanza"` |
| Section | `CFGOPT_DB5_CMD` | `"global"` |
| Section | `CFGOPT_DB5_CONFIG` | `"global"` |
| Section | `CFGOPT_DB5_HOST` | `"stanza"` |
| Section | `CFGOPT_DB5_PATH` | `"stanza"` |
| Section | `CFGOPT_DB5_PORT` | `"stanza"` |
| Section | `CFGOPT_DB5_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB5_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB5_USER` | `"stanza"` |
| Section | `CFGOPT_DB6_CMD` | `"global"` |
| Section | `CFGOPT_DB6_CONFIG` | `"global"` |
| Section | `CFGOPT_DB6_HOST` | `"stanza"` |
| Section | `CFGOPT_DB6_PATH` | `"stanza"` |
| Section | `CFGOPT_DB6_PORT` | `"stanza"` |
| Section | `CFGOPT_DB6_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB6_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB6_USER` | `"stanza"` |
| Section | `CFGOPT_DB7_CMD` | `"global"` |
| Section | `CFGOPT_DB7_CONFIG` | `"global"` |
| Section | `CFGOPT_DB7_HOST` | `"stanza"` |
| Section | `CFGOPT_DB7_PATH` | `"stanza"` |
| Section | `CFGOPT_DB7_PORT` | `"stanza"` |
| Section | `CFGOPT_DB7_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB7_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB7_USER` | `"stanza"` |
| Section | `CFGOPT_DB8_CMD` | `"global"` |
| Section | `CFGOPT_DB8_CONFIG` | `"global"` |
| Section | `CFGOPT_DB8_HOST` | `"stanza"` |
| Section | `CFGOPT_DB8_PATH` | `"stanza"` |
| Section | `CFGOPT_DB8_PORT` | `"stanza"` |
| Section | `CFGOPT_DB8_SOCKET_PATH` | `"stanza"` |
| Section | `CFGOPT_DB8_SSH_PORT` | `"stanza"` |
| Section | `CFGOPT_DB8_USER` | `"stanza"` |
| Section | `CFGOPT_HARDLINK` | `"global"` |
| Section | `CFGOPT_LINK_ALL` | `"global"` |
| Section | `CFGOPT_LINK_MAP` | `"global"` |
| Section | `CFGOPT_LOCK_PATH` | `"global"` |
| Section | `CFGOPT_LOG_LEVEL_CONSOLE` | `"global"` |
| Section | `CFGOPT_LOG_LEVEL_FILE` | `"global"` |
| Section | `CFGOPT_LOG_LEVEL_STDERR` | `"global"` |
| Section | `CFGOPT_LOG_PATH` | `"global"` |
| Section | `CFGOPT_LOG_TIMESTAMP` | `"global"` |
| Section | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"global"` |
| Section | `CFGOPT_NEUTRAL_UMASK` | `"global"` |
| Section | `CFGOPT_PROCESS_MAX` | `"global"` |
| Section | `CFGOPT_PROTOCOL_TIMEOUT` | `"global"` |
| Section | `CFGOPT_RECOVERY_OPTION` | `"global"` |
| Section | `CFGOPT_REPO_PATH` | `"global"` |
| Section | `CFGOPT_REPO_S3_BUCKET` | `"global"` |
| Section | `CFGOPT_REPO_S3_CA_FILE` | `"global"` |
| Section | `CFGOPT_REPO_S3_CA_PATH` | `"global"` |
| Section | `CFGOPT_REPO_S3_ENDPOINT` | `"global"` |
| Section | `CFGOPT_REPO_S3_HOST` | `"global"` |
| Section | `CFGOPT_REPO_S3_KEY` | `"global"` |
| Section | `CFGOPT_REPO_S3_KEY_SECRET` | `"global"` |
| Section | `CFGOPT_REPO_S3_REGION` | `"global"` |
| Section | `CFGOPT_REPO_S3_VERIFY_SSL` | `"global"` |
| Section | `CFGOPT_REPO_TYPE` | `"global"` |
| Section | `CFGOPT_RESUME` | `"global"` |
| Section | `CFGOPT_RETENTION_ARCHIVE` | `"global"` |
| Section | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"global"` |
| Section | `CFGOPT_RETENTION_DIFF` | `"global"` |
| Section | `CFGOPT_RETENTION_FULL` | `"global"` |
| Section | `CFGOPT_SPOOL_PATH` | `"global"` |
| Section | `CFGOPT_START_FAST` | `"global"` |
| Section | `CFGOPT_STOP_AUTO` | `"global"` |
| Section | `CFGOPT_TABLESPACE_MAP` | `"global"` |
| Section | `CFGOPT_TABLESPACE_MAP_ALL` | `"global"` |

## cfgOptionRuleSecure

Secure options can never be passed on the commmand-line.

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| Secure | `CFGOPT_REPO_S3_KEY` | `true` |
| Secure | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |

## cfgOptionRuleType

Secure options can never be passed on the commmand-line.

### Truth Table:

| Suffix | uiOptionId | Result |
| ------ | ---------- | ------ |
| Type | `CFGOPT_ARCHIVE_ASYNC` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_ARCHIVE_CHECK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_ARCHIVE_COPY` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_ARCHIVE_MAX_MB` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_ARCHIVE_QUEUE_MAX` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_ARCHIVE_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| Type | `CFGOPT_BACKUP_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_BACKUP_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_BACKUP_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_BACKUP_STANDBY` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_BACKUP_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_BUFFER_SIZE` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_CHECKSUM_PAGE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_CMD_SSH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_COMMAND` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_COMPRESS` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_COMPRESS_LEVEL` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB_INCLUDE` | `CFGOPTRULE_TYPE_HASH` |
| Type | `CFGOPT_DB_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| Type | `CFGOPT_DB1_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB1_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB1_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB1_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB1_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB1_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB1_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB1_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB2_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB2_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB2_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB3_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB3_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB3_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB4_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB4_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB4_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB5_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB5_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB5_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB6_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB6_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB6_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB7_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB7_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB7_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_CMD` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB8_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DB8_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_DB8_USER` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_DELTA` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_FORCE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_HARDLINK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_HOST_ID` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_LINK_ALL` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_LINK_MAP` | `CFGOPTRULE_TYPE_HASH` |
| Type | `CFGOPT_LOCK_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_LOG_LEVEL_CONSOLE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_LOG_LEVEL_FILE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_LOG_LEVEL_STDERR` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_LOG_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_LOG_TIMESTAMP` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_NEUTRAL_UMASK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_ONLINE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_OUTPUT` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_PROCESS` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_PROCESS_MAX` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_PROTOCOL_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| Type | `CFGOPT_RECOVERY_OPTION` | `CFGOPTRULE_TYPE_HASH` |
| Type | `CFGOPT_REPO_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_BUCKET` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_HOST` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_KEY` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_REGION` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_REPO_TYPE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_RESUME` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_RETENTION_ARCHIVE` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_RETENTION_DIFF` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_RETENTION_FULL` | `CFGOPTRULE_TYPE_INTEGER` |
| Type | `CFGOPT_SET` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_SPOOL_PATH` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_STANZA` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_START_FAST` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_STOP_AUTO` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_TABLESPACE_MAP` | `CFGOPTRULE_TYPE_HASH` |
| Type | `CFGOPT_TABLESPACE_MAP_ALL` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_TARGET` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_TARGET_ACTION` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_TARGET_TIMELINE` | `CFGOPTRULE_TYPE_STRING` |
| Type | `CFGOPT_TEST` | `CFGOPTRULE_TYPE_BOOLEAN` |
| Type | `CFGOPT_TEST_DELAY` | `CFGOPTRULE_TYPE_FLOAT` |
| Type | `CFGOPT_TEST_POINT` | `CFGOPTRULE_TYPE_HASH` |
| Type | `CFGOPT_TYPE` | `CFGOPTRULE_TYPE_STRING` |

## cfgOptionRuleValid

Is the option valid for this command?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Suffix | uiCommandId | uiOptionId | Result |
| ------ | ----------- | ---------- | ------ |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_MAX_MB` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_QUEUE_MAX` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROCESS_MAX` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_SPOOL_PATH` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_DELAY` | `true` |
| Valid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_POINT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_COPY` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_BACKUP_STANDBY` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_CHECKSUM_PAGE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB1_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB2_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB3_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB4_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB5_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB6_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB7_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_DB8_USER` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_HARDLINK` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_ONLINE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_PROCESS_MAX` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_RESUME` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_DIFF` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_FULL` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_START_FAST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_STOP_AUTO` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_TEST` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_TEST_DELAY` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_TEST_POINT` | `true` |
| Valid | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_STANDBY` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB1_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB2_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB3_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB4_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB5_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB6_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB7_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_DB8_USER` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_ONLINE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_DIFF` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_FULL` | `true` |
| Valid | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_OUTPUT` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_INFO` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_COMMAND` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB1_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB2_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB3_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB4_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB5_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB6_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB7_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_DB8_USER` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_HOST_ID` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_COMMAND` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB3_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB4_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB5_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB6_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB7_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB8_PORT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_PROCESS` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB_INCLUDE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_DELTA` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_FORCE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LINK_ALL` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LINK_MAP` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_PROCESS_MAX` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_RECOVERY_OPTION` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_SET` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP_ALL` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TARGET` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_ACTION` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_TIMELINE` | `true` |
| Valid | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_USER` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_FORCE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_ONLINE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BUFFER_SIZE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB_TIMEOUT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_USER` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_ONLINE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_START` | `CFGOPT_STANZA` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_BACKUP_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_BACKUP_USER` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_CMD_SSH` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB1_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB1_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB1_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB2_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB2_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB2_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB3_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB3_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB3_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB3_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB4_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB4_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB4_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB4_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB5_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB5_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB5_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB5_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB6_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB6_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB6_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB6_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB7_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB7_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB7_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB7_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB8_CMD` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB8_CONFIG` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB8_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_DB8_SSH_PORT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_FORCE` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOCK_PATH` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOG_PATH` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_PATH` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_HOST` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_REGION` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_REPO_TYPE` | `true` |
| Valid | `CFGCMD_STOP` | `CFGOPT_STANZA` | `true` |
