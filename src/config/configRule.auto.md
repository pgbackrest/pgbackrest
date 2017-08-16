## cfgOptionRuleAllowList

Is there an allow list for this option?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgOptionRuleAllowList | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgOptionRuleAllowListValue

Get value from allowed list.

### Truth Table:

| Function | uiCommandId | uiOptionId | uiValueId | Result |
| -------- | ----------- | ---------- | --------- | ------ |
| cfgOptionRuleAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `0` | `"full"` |
| cfgOptionRuleAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `1` | `"diff"` |
| cfgOptionRuleAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `2` | `"incr"` |
| cfgOptionRuleAllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `0` | `"db"` |
| cfgOptionRuleAllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `1` | `"backup"` |
| cfgOptionRuleAllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `0` | `"db"` |
| cfgOptionRuleAllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `1` | `"backup"` |
| cfgOptionRuleAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `0` | `"name"` |
| cfgOptionRuleAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `1` | `"time"` |
| cfgOptionRuleAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `2` | `"xid"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `0` | `"16384"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `1` | `"32768"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `2` | `"65536"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `3` | `"131072"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `4` | `"262144"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `5` | `"524288"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `6` | `"1048576"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `7` | `"2097152"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `8` | `"4194304"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `9` | `"8388608"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `10` | `"16777216"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `0` | `"off"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `1` | `"error"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `2` | `"warn"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `3` | `"info"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `4` | `"detail"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `5` | `"debug"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `6` | `"trace"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `0` | `"off"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `1` | `"error"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `2` | `"warn"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `3` | `"info"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `4` | `"detail"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `5` | `"debug"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `6` | `"trace"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `0` | `"off"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `1` | `"error"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `2` | `"warn"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `3` | `"info"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `4` | `"detail"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `5` | `"debug"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `6` | `"trace"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `0` | `"text"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `1` | `"json"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `0` | `"cifs"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `1` | `"posix"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `2` | `"s3"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `0` | `"full"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `1` | `"diff"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `2` | `"incr"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"pause"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"promote"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"shutdown"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `3` | `"preserve"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `4` | `"none"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `5` | `"immediate"` |
| cfgOptionRuleAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `6` | `"default"` |

## cfgOptionRuleAllowListValueTotal

Total number of values allowed.

### Truth Table:

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleAllowListValueTotal | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `3` |
| cfgOptionRuleAllowListValueTotal | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `2` |
| cfgOptionRuleAllowListValueTotal | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `2` |
| cfgOptionRuleAllowListValueTotal | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `7` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `11` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `7` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `7` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `7` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_OUTPUT` | `2` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `3` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `3` |
| cfgOptionRuleAllowListValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |

## cfgOptionRuleAllowRange

Is the option constrained to a range?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgOptionRuleAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `true` |

## cfgOptionRuleAllowRangeMax

Maximum value allowed (if the option is constrained to a range).

### Truth Table:

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `86400` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `9` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `9` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `604800` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `96` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `604800` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `999999999` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `999999999` |
| cfgOptionRuleAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `999999999` |

## cfgOptionRuleAllowRangeMin

Minimum value allowed (if the option is constrained to a range).

### Truth Table:

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `0.1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `0` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `0` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `0.1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `0.1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `1` |
| cfgOptionRuleAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `1` |

## cfgOptionRuleDefault

Default value.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleDefault | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `"incr"` |
| cfgOptionRuleDefault | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `"default"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `"60"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `"backrest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `"4194304"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `"ssh"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_COMPRESS` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `"6"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"3"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `"1800"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `"5432"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB1_USER` | `"postgres"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB2_PORT` | `"5432"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DB2_USER` | `"postgres"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_DELTA` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_FORCE` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_HARDLINK` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `"/tmp/pgbackrest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `"warn"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `"info"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `"warn"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `"/var/log/pgbackrest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"1073741824"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_ONLINE` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_OUTPUT` | `"text"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `"1830"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `"/var/lib/pgbackrest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `"posix"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_RESUME` | `"1"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"full"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_SET` | `"latest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `"/var/spool/pgbackrest"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_START_FAST` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `"pause"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_TEST` | `"0"` |
| cfgOptionRuleDefault | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `"5"` |

## cfgOptionRuleDepend

Does the option have a dependency on another option?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleDepend | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| cfgOptionRuleDepend | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `true` |

## cfgOptionRuleDependOption

Name of the option that this option depends in order to be set.

### Truth Table:

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `CFGOPT_ONLINE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `CFGOPT_ARCHIVE_CHECK` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `CFGOPT_BACKUP_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `CFGOPT_BACKUP_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPT_BACKUP_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `CFGOPT_BACKUP_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `CFGOPT_DB1_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `CFGOPT_DB1_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `CFGOPT_DB1_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB1_USER` | `CFGOPT_DB1_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `CFGOPT_DB2_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `CFGOPT_DB2_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `CFGOPT_DB2_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_DB2_USER` | `CFGOPT_DB2_HOST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_FORCE` | `CFGOPT_ONLINE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `CFGOPT_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPT_REPO_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `CFGOPT_ARCHIVE_ASYNC` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TARGET` | `CFGOPT_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `CFGOPT_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPT_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `CFGOPT_TYPE` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `CFGOPT_TEST` |
| cfgOptionRuleDependOption | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `CFGOPT_TEST` |

## cfgOptionRuleDependValue

The depend option must have one of these values before this option is set.

### Truth Table:

| Function | uiCommandId | uiOptionId | uiValueId | Result |
| -------- | ----------- | ---------- | --------- | ------ |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `0` | `"1"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `0` | `"1"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_FORCE` | `0` | `"0"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `0` | `"default"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `1` | `"name"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `2` | `"time"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `3` | `"xid"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `0` | `"s3"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `0` | `"1"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `0` | `"name"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `1` | `"time"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `2` | `"xid"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"name"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"time"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"xid"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `0` | `"time"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `1` | `"xid"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `0` | `"default"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `1` | `"name"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `2` | `"time"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `3` | `"xid"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `0` | `"1"` |
| cfgOptionRuleDependValue | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `0` | `"1"` |

## cfgOptionRuleDependValueTotal

Total depend values for this option.

### Truth Table:

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_USER` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_USER` | `0` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_FORCE` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `4` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET` | `3` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `2` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `4` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `1` |
| cfgOptionRuleDependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `1` |

## cfgOptionRuleHint

Some clue as to what value the user should provide when the option is missing but required.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleHint | _\<ANY\>_ | `CFGOPT_DB1_PATH` | `"does this stanza exist?"` |
| cfgOptionRuleHint | _\<ANY\>_ | `CFGOPT_DB2_PATH` | `"does this stanza exist?"` |

## cfgOptionRuleNameAlt

Alternate name for the option primarily used for deprecated names.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_CMD` | `"db-cmd"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_CONFIG` | `"db-config"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_HOST` | `"db-host"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_PATH` | `"db-path"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_PORT` | `"db-port"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_SOCKET_PATH` | `"db-socket-path"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_SSH_PORT` | `"db-ssh-port"` |
| cfgOptionRuleNameAlt | `CFGOPT_DB1_USER` | `"db-user"` |
| cfgOptionRuleNameAlt | `CFGOPT_PROCESS_MAX` | `"thread-max"` |

## cfgOptionRuleNegate

Can the boolean option be negated?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleNegate | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgOptionRuleNegate | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgOptionRuleNegate | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgOptionRuleNegate | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleNegate | `CFGOPT_CHECKSUM_PAGE` | `true` |
| cfgOptionRuleNegate | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleNegate | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleNegate | `CFGOPT_HARDLINK` | `true` |
| cfgOptionRuleNegate | `CFGOPT_LINK_ALL` | `true` |
| cfgOptionRuleNegate | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleNegate | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleNegate | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleNegate | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleNegate | `CFGOPT_RESUME` | `true` |
| cfgOptionRuleNegate | `CFGOPT_START_FAST` | `true` |
| cfgOptionRuleNegate | `CFGOPT_STOP_AUTO` | `true` |

## cfgOptionRulePrefix

Prefix when the option has an index > 1 (e.g. "db").

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRulePrefix | `CFGOPT_DB1_CMD` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_CONFIG` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_HOST` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_PATH` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_PORT` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_SOCKET_PATH` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_SSH_PORT` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB1_USER` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_CMD` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_CONFIG` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_HOST` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_PATH` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_PORT` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_SOCKET_PATH` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_SSH_PORT` | `"db"` |
| cfgOptionRulePrefix | `CFGOPT_DB2_USER` | `"db"` |

## cfgOptionRuleRequired

Is the option required?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleRequired | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleRequired | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleRequired | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| cfgOptionRuleRequired | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleRequired | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleRequired | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleRequired | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_COMMAND` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_DELTA` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_HARDLINK` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_HOST_ID` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_RESUME` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_SET` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_START_FAST` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TEST` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| cfgOptionRuleRequired | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgOptionRuleSection

Section that the option belongs in, NULL means command-line only.

### Truth Table:

Functions that return `NULL` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_ASYNC` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_CHECK` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_COPY` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_MAX_MB` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_QUEUE_MAX` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_ARCHIVE_TIMEOUT` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_CMD` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_CONFIG` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_HOST` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_SSH_PORT` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_STANDBY` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BACKUP_USER` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_BUFFER_SIZE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_CHECKSUM_PAGE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_CMD_SSH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_COMPRESS` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_COMPRESS_LEVEL` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB_INCLUDE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB_TIMEOUT` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB1_CMD` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB1_CONFIG` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB1_HOST` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB1_PATH` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB1_PORT` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB1_SOCKET_PATH` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB1_SSH_PORT` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB1_USER` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_CMD` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB2_CONFIG` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_DB2_HOST` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_PATH` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_PORT` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_SOCKET_PATH` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_SSH_PORT` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_DB2_USER` | `"stanza"` |
| cfgOptionRuleSection | `CFGOPT_HARDLINK` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LINK_ALL` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LINK_MAP` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOCK_PATH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOG_LEVEL_CONSOLE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOG_LEVEL_FILE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOG_LEVEL_STDERR` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOG_PATH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_LOG_TIMESTAMP` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_NEUTRAL_UMASK` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_PROCESS_MAX` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_PROTOCOL_TIMEOUT` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RECOVERY_OPTION` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_PATH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_BUCKET` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_CA_FILE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_CA_PATH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_ENDPOINT` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_HOST` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_KEY` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_KEY_SECRET` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_REGION` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_S3_VERIFY_SSL` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_REPO_TYPE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RESUME` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RETENTION_ARCHIVE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RETENTION_DIFF` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_RETENTION_FULL` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_SPOOL_PATH` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_START_FAST` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_STOP_AUTO` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_TABLESPACE_MAP` | `"global"` |
| cfgOptionRuleSection | `CFGOPT_TABLESPACE_MAP_ALL` | `"global"` |

## cfgOptionRuleSecure

Secure options can never be passed on the commmand-line.

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleSecure | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleSecure | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |

## cfgOptionRuleType

Secure options can never be passed on the commmand-line.

### Truth Table:

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_ASYNC` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_CHECK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_COPY` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_MAX_MB` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_QUEUE_MAX` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_ARCHIVE_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| cfgOptionRuleType | `CFGOPT_BACKUP_CMD` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_BACKUP_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_BACKUP_HOST` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_BACKUP_STANDBY` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_BACKUP_USER` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_BUFFER_SIZE` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_CHECKSUM_PAGE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_CMD_SSH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_COMMAND` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_COMPRESS` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_COMPRESS_LEVEL` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB_INCLUDE` | `CFGOPTRULE_TYPE_HASH` |
| cfgOptionRuleType | `CFGOPT_DB_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| cfgOptionRuleType | `CFGOPT_DB1_CMD` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB1_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB1_HOST` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB1_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB1_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_DB1_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB1_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_DB1_USER` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_CMD` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_CONFIG` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_HOST` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_DB2_SOCKET_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DB2_SSH_PORT` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_DB2_USER` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_DELTA` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_FORCE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_HARDLINK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_HOST_ID` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_LINK_ALL` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_LINK_MAP` | `CFGOPTRULE_TYPE_HASH` |
| cfgOptionRuleType | `CFGOPT_LOCK_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_LOG_LEVEL_CONSOLE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_LOG_LEVEL_FILE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_LOG_LEVEL_STDERR` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_LOG_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_LOG_TIMESTAMP` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_NEUTRAL_UMASK` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_ONLINE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_OUTPUT` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_PROCESS` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_PROCESS_MAX` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_PROTOCOL_TIMEOUT` | `CFGOPTRULE_TYPE_FLOAT` |
| cfgOptionRuleType | `CFGOPT_RECOVERY_OPTION` | `CFGOPTRULE_TYPE_HASH` |
| cfgOptionRuleType | `CFGOPT_REPO_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_BUCKET` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_HOST` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_KEY` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_REGION` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_REPO_TYPE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_RESUME` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_RETENTION_ARCHIVE` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_RETENTION_DIFF` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_RETENTION_FULL` | `CFGOPTRULE_TYPE_INTEGER` |
| cfgOptionRuleType | `CFGOPT_SET` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_SPOOL_PATH` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_STANZA` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_START_FAST` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_STOP_AUTO` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_TABLESPACE_MAP` | `CFGOPTRULE_TYPE_HASH` |
| cfgOptionRuleType | `CFGOPT_TABLESPACE_MAP_ALL` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_TARGET` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_TARGET_ACTION` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_TARGET_TIMELINE` | `CFGOPTRULE_TYPE_STRING` |
| cfgOptionRuleType | `CFGOPT_TEST` | `CFGOPTRULE_TYPE_BOOLEAN` |
| cfgOptionRuleType | `CFGOPT_TEST_DELAY` | `CFGOPTRULE_TYPE_FLOAT` |
| cfgOptionRuleType | `CFGOPT_TEST_POINT` | `CFGOPTRULE_TYPE_HASH` |
| cfgOptionRuleType | `CFGOPT_TYPE` | `CFGOPTRULE_TYPE_STRING` |

## cfgOptionRuleValid

Is the option valid for this command?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiCommandId | uiOptionId | Result |
| -------- | ----------- | ---------- | ------ |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_MAX_MB` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_QUEUE_MAX` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_SPOOL_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_DELAY` | `true` |
| cfgOptionRuleValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_POINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_CHECKSUM_PAGE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_HARDLINK` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_RESUME` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_FULL` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_START_FAST` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_STOP_AUTO` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_TEST` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_TEST_DELAY` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_TEST_POINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_FULL` | `true` |
| cfgOptionRuleValid | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_OUTPUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_INFO` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_COMMAND` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_HOST_ID` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_COMMAND` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_PROCESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_DB_INCLUDE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_DELTA` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LINK_ALL` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LINK_MAP` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_RECOVERY_OPTION` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_SET` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP_ALL` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_ACTION` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_TIMELINE` | `true` |
| cfgOptionRuleValid | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_ONLINE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_START` | `CFGOPT_STANZA` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_USER` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_CMD_SSH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB1_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB1_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB2_CMD` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB2_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_FORCE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOCK_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOG_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_REPO_TYPE` | `true` |
| cfgOptionRuleValid | `CFGCMD_STOP` | `CFGOPT_STANZA` | `true` |

## cfgOptionRuleValueHash

Is the option a true hash or just a list of keys?

### Truth Table:

Functions that return `false` are not listed here for brevity.

| Function | uiOptionId | Result |
| -------- | ---------- | ------ |
| cfgOptionRuleValueHash | `CFGOPT_LINK_MAP` | `true` |
| cfgOptionRuleValueHash | `CFGOPT_RECOVERY_OPTION` | `true` |
| cfgOptionRuleValueHash | `CFGOPT_TABLESPACE_MAP` | `true` |
| cfgOptionRuleValueHash | `CFGOPT_TEST_POINT` | `true` |
