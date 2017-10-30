# Command and Option Configuration Rules

## cfgRuleOptionAllowList

Is there an allow list for this option?

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `false` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgRuleOptionAllowList | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgRuleOptionAllowListValue

Get value from allowed list.

### Truth Table:

This function is valid when `cfgRuleOptionAllowList()` = `true`.

| Function | commandId | optionId | valueId | Result |
| -------- | --------- | -------- | ------- | ------ |
| cfgRuleOptionAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `0` | `"full"` |
| cfgRuleOptionAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `1` | `"diff"` |
| cfgRuleOptionAllowListValue | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `2` | `"incr"` |
| cfgRuleOptionAllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `0` | `"db"` |
| cfgRuleOptionAllowListValue | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `1` | `"backup"` |
| cfgRuleOptionAllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `0` | `"db"` |
| cfgRuleOptionAllowListValue | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `1` | `"backup"` |
| cfgRuleOptionAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `0` | `"name"` |
| cfgRuleOptionAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `1` | `"time"` |
| cfgRuleOptionAllowListValue | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `2` | `"xid"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `0` | `"16384"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `1` | `"32768"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `2` | `"65536"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `3` | `"131072"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `4` | `"262144"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `5` | `"524288"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `6` | `"1048576"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `7` | `"2097152"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `8` | `"4194304"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `9` | `"8388608"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `10` | `"16777216"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `0` | `"off"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `1` | `"error"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `2` | `"warn"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `3` | `"info"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `4` | `"detail"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `5` | `"debug"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `6` | `"trace"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `0` | `"off"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `1` | `"error"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `2` | `"warn"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `3` | `"info"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `4` | `"detail"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `5` | `"debug"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `6` | `"trace"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `0` | `"off"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `1` | `"error"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `2` | `"warn"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `3` | `"info"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `4` | `"detail"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `5` | `"debug"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `6` | `"trace"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `0` | `"text"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_OUTPUT` | `1` | `"json"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `0` | `"cifs"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `1` | `"posix"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `2` | `"s3"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `0` | `"full"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `1` | `"diff"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `2` | `"incr"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"pause"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"promote"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"shutdown"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `3` | `"preserve"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `4` | `"none"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `5` | `"immediate"` |
| cfgRuleOptionAllowListValue | _\<ANY\>_ | `CFGOPT_TYPE` | `6` | `"default"` |

## cfgRuleOptionAllowListValueTotal

Total number of values allowed.

### Truth Table:

This function is valid when `cfgRuleOptionAllowList()` = `true`.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionAllowListValueTotal | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `3` |
| cfgRuleOptionAllowListValueTotal | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `2` |
| cfgRuleOptionAllowListValueTotal | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `2` |
| cfgRuleOptionAllowListValueTotal | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `7` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `11` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `7` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `7` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `7` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_OUTPUT` | `2` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `3` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `3` |
| cfgRuleOptionAllowListValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |

## cfgRuleOptionAllowRange

Is the option constrained to a range?

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `false` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgRuleOptionAllowRange | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `true` |

## cfgRuleOptionAllowRangeMax

Maximum value allowed (if the option is constrained to a range).

### Truth Table:

This function is valid when `cfgRuleOptionAllowRange()` = `true`.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `86400` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `9` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `9` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `604800` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `96` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `604800` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `999999999` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `999999999` |
| cfgRuleOptionAllowRangeMax | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `999999999` |

## cfgRuleOptionAllowRangeMin

Minimum value allowed (if the option is constrained to a range).

### Truth Table:

This function is valid when `cfgRuleOptionAllowRange()` = `true`.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `0.1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `0` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `0` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `0.1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `0.1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE` | `1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_DIFF` | `1` |
| cfgRuleOptionAllowRangeMin | _\<ANY\>_ | `CFGOPT_RETENTION_FULL` | `1` |

## cfgRuleOptionDefault

Default value.

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `NULL` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionDefault | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `"incr"` |
| cfgRuleOptionDefault | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `"default"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `"60"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `"backrest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `"4194304"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `"ssh"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_COMPRESS` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `"6"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"3"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `"1800"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB1_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB2_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB2_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB3_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB3_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB4_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB4_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB5_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB5_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB6_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB6_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB7_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB7_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `"/etc/pgbackrest.conf"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB8_PORT` | `"5432"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DB8_USER` | `"postgres"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_DELTA` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_FORCE` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_HARDLINK` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `"/tmp/pgbackrest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `"warn"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `"info"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `"warn"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `"/var/log/pgbackrest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"1073741824"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_ONLINE` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_OUTPUT` | `"text"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `"1830"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `"/var/lib/pgbackrest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `"posix"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_RESUME` | `"1"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"full"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_SET` | `"latest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `"/var/spool/pgbackrest"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_START_FAST` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `"pause"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_TEST` | `"0"` |
| cfgRuleOptionDefault | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `"5"` |

## cfgRuleOptionDepend

Does the option have a dependency on another option?

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `false` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionDepend | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| cfgRuleOptionDepend | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `true` |

## cfgRuleOptionDependOption

Name of the option that this option depends in order to be set.

### Truth Table:

This function is valid when `cfgRuleOptionDepend()` = `true`.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `CFGOPT_ONLINE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `CFGOPT_ARCHIVE_CHECK` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `CFGOPT_BACKUP_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `CFGOPT_BACKUP_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPT_BACKUP_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `CFGOPT_BACKUP_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `CFGOPT_DB1_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `CFGOPT_DB1_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `CFGOPT_DB1_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB1_USER` | `CFGOPT_DB1_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `CFGOPT_DB2_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `CFGOPT_DB2_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `CFGOPT_DB2_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB2_USER` | `CFGOPT_DB2_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `CFGOPT_DB3_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `CFGOPT_DB3_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `CFGOPT_DB3_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB3_USER` | `CFGOPT_DB3_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `CFGOPT_DB4_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `CFGOPT_DB4_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `CFGOPT_DB4_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB4_USER` | `CFGOPT_DB4_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `CFGOPT_DB5_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `CFGOPT_DB5_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `CFGOPT_DB5_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB5_USER` | `CFGOPT_DB5_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `CFGOPT_DB6_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `CFGOPT_DB6_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `CFGOPT_DB6_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB6_USER` | `CFGOPT_DB6_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `CFGOPT_DB7_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `CFGOPT_DB7_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `CFGOPT_DB7_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB7_USER` | `CFGOPT_DB7_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `CFGOPT_DB8_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `CFGOPT_DB8_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `CFGOPT_DB8_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_DB8_USER` | `CFGOPT_DB8_HOST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_FORCE` | `CFGOPT_ONLINE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `CFGOPT_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPT_REPO_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `CFGOPT_ARCHIVE_ASYNC` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TARGET` | `CFGOPT_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `CFGOPT_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPT_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `CFGOPT_TYPE` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `CFGOPT_TEST` |
| cfgRuleOptionDependOption | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `CFGOPT_TEST` |

## cfgRuleOptionDependValue

The depend option must have one of these values before this option is set.

### Truth Table:

This function is valid when `cfgRuleOptionDepend()` = `true`.

| Function | commandId | optionId | valueId | Result |
| -------- | --------- | -------- | ------- | ------ |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `0` | `"1"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `0` | `"1"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_FORCE` | `0` | `"0"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `0` | `"default"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `1` | `"name"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `2` | `"time"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `3` | `"xid"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `0` | `"s3"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `0` | `"1"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `0` | `"name"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `1` | `"time"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET` | `2` | `"xid"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `0` | `"name"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `1` | `"time"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `2` | `"xid"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `0` | `"time"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `1` | `"xid"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `0` | `"default"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `1` | `"name"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `2` | `"time"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `3` | `"xid"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `0` | `"1"` |
| cfgRuleOptionDependValue | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `0` | `"1"` |

## cfgRuleOptionDependValueTotal

Total depend values for this option.

### Truth Table:

This function is valid when `cfgRuleOptionDepend()` = `true`.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_BACKUP_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB1_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB2_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB3_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB4_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB5_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB6_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB7_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_CMD` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_CONFIG` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_SSH_PORT` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_DB8_USER` | `0` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_FORCE` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_RECOVERY_OPTION` | `4` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_FILE` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_CA_PATH` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_HOST` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_KEY_SECRET` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET` | `3` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `3` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `2` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TARGET_TIMELINE` | `4` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `1` |
| cfgRuleOptionDependValueTotal | _\<ANY\>_ | `CFGOPT_TEST_POINT` | `1` |

## cfgRuleOptionNameAlt

Alternate name for the option primarily used for deprecated names.

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `NULL` are excluded for brevity.

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_CMD` | `"db-cmd"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_CONFIG` | `"db-config"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_HOST` | `"db-host"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_PATH` | `"db-path"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_PORT` | `"db-port"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_SOCKET_PATH` | `"db-socket-path"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_SSH_PORT` | `"db-ssh-port"` |
| cfgRuleOptionNameAlt | `CFGOPT_DB1_USER` | `"db-user"` |
| cfgRuleOptionNameAlt | `CFGOPT_PROCESS_MAX` | `"thread-max"` |

## cfgRuleOptionNegate

Can the boolean option be negated?

### Truth Table:

Permutations that return `false` are excluded for brevity.

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionNegate | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgRuleOptionNegate | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgRuleOptionNegate | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgRuleOptionNegate | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionNegate | `CFGOPT_CHECKSUM_PAGE` | `true` |
| cfgRuleOptionNegate | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionNegate | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionNegate | `CFGOPT_HARDLINK` | `true` |
| cfgRuleOptionNegate | `CFGOPT_LINK_ALL` | `true` |
| cfgRuleOptionNegate | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionNegate | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionNegate | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionNegate | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionNegate | `CFGOPT_RESUME` | `true` |
| cfgRuleOptionNegate | `CFGOPT_START_FAST` | `true` |
| cfgRuleOptionNegate | `CFGOPT_STOP_AUTO` | `true` |

## cfgRuleOptionPrefix

Prefix when the option has an index > 1 (e.g. "db").

### Truth Table:

Permutations that return `NULL` are excluded for brevity.

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionPrefix | `CFGOPT_DB1_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB1_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB2_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB3_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB4_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB5_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB6_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB7_USER` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_CMD` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_CONFIG` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_HOST` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_SOCKET_PATH` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_SSH_PORT` | `"db"` |
| cfgRuleOptionPrefix | `CFGOPT_DB8_USER` | `"db"` |

## cfgRuleOptionRequired

Is the option required?

### Truth Table:

This function is valid when `cfgRuleOptionValid()` = `true`. Permutations that return `false` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionRequired | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionRequired | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionRequired | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| cfgRuleOptionRequired | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionRequired | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionRequired | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionRequired | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_COMMAND` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_DELTA` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_HARDLINK` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_HOST_ID` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LINK_ALL` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_OUTPUT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_PROCESS_MAX` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_RESUME` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_SET` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_SPOOL_PATH` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_START_FAST` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_STOP_AUTO` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TARGET` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TARGET_ACTION` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TEST` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TEST_DELAY` | `true` |
| cfgRuleOptionRequired | _\<ANY\>_ | `CFGOPT_TYPE` | `true` |

## cfgRuleOptionSection

Section that the option belongs in, NULL means command-line only.

### Truth Table:

Permutations that return `NULL` are excluded for brevity.

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_ASYNC` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_CHECK` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_COPY` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_MAX_MB` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_QUEUE_MAX` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_ARCHIVE_TIMEOUT` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_HOST` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_SSH_PORT` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_STANDBY` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BACKUP_USER` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_BUFFER_SIZE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_CHECKSUM_PAGE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_CMD_SSH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_COMPRESS` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_COMPRESS_LEVEL` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB_INCLUDE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB_TIMEOUT` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB1_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB1_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB1_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB1_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB1_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB1_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB1_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB1_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB2_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB2_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB2_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB3_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB3_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB3_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB4_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB4_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB4_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB5_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB5_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB5_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB6_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB6_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB6_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB7_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB7_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB7_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_CMD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB8_CONFIG` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_DB8_HOST` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_SOCKET_PATH` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_SSH_PORT` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_DB8_USER` | `"stanza"` |
| cfgRuleOptionSection | `CFGOPT_HARDLINK` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LINK_ALL` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LINK_MAP` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOCK_PATH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOG_LEVEL_CONSOLE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOG_LEVEL_FILE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOG_LEVEL_STDERR` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOG_PATH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_LOG_TIMESTAMP` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_NEUTRAL_UMASK` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_PROCESS_MAX` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_PROTOCOL_TIMEOUT` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RECOVERY_OPTION` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_PATH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_BUCKET` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_CA_FILE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_CA_PATH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_ENDPOINT` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_HOST` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_KEY` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_KEY_SECRET` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_REGION` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_S3_VERIFY_SSL` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_REPO_TYPE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RESUME` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RETENTION_ARCHIVE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RETENTION_DIFF` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_RETENTION_FULL` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_SPOOL_PATH` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_START_FAST` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_STOP_AUTO` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_TABLESPACE_MAP` | `"global"` |
| cfgRuleOptionSection | `CFGOPT_TABLESPACE_MAP_ALL` | `"global"` |

## cfgRuleOptionSecure

Secure options can never be passed on the commmand-line.

### Truth Table:

Permutations that return `false` are excluded for brevity.

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionSecure | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionSecure | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |

## cfgRuleOptionType

Secure options can never be passed on the commmand-line.

### Truth Table:

| Function | optionId | Result |
| -------- | -------- | ------ |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_ASYNC` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_CHECK` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_COPY` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_MAX_MB` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_QUEUE_MAX` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_ARCHIVE_TIMEOUT` | `CFGOPTDEF_TYPE_FLOAT` |
| cfgRuleOptionType | `CFGOPT_BACKUP_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_BACKUP_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_BACKUP_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_BACKUP_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_BACKUP_STANDBY` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_BACKUP_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_BUFFER_SIZE` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_CHECKSUM_PAGE` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_CMD_SSH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_COMMAND` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_COMPRESS` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_COMPRESS_LEVEL` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB_INCLUDE` | `CFGOPTDEF_TYPE_LIST` |
| cfgRuleOptionType | `CFGOPT_DB_TIMEOUT` | `CFGOPTDEF_TYPE_FLOAT` |
| cfgRuleOptionType | `CFGOPT_DB1_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB1_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB1_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB1_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB1_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB1_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB1_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB1_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB2_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB2_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB2_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB3_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB3_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB3_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB4_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB4_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB4_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB5_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB5_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB5_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB6_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB6_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB6_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB7_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB7_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB7_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_CMD` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_CONFIG` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB8_SOCKET_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DB8_SSH_PORT` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_DB8_USER` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_DELTA` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_FORCE` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_HARDLINK` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_HOST_ID` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_LINK_ALL` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_LINK_MAP` | `CFGOPTDEF_TYPE_HASH` |
| cfgRuleOptionType | `CFGOPT_LOCK_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_LOG_LEVEL_CONSOLE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_LOG_LEVEL_FILE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_LOG_LEVEL_STDERR` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_LOG_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_LOG_TIMESTAMP` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_NEUTRAL_UMASK` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_ONLINE` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_OUTPUT` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_PROCESS` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_PROCESS_MAX` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_PROTOCOL_TIMEOUT` | `CFGOPTDEF_TYPE_FLOAT` |
| cfgRuleOptionType | `CFGOPT_RECOVERY_OPTION` | `CFGOPTDEF_TYPE_HASH` |
| cfgRuleOptionType | `CFGOPT_REPO_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_BUCKET` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_CA_FILE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_CA_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_ENDPOINT` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_HOST` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_KEY` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_KEY_SECRET` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_REGION` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_REPO_S3_VERIFY_SSL` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_REPO_TYPE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_RESUME` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_RETENTION_ARCHIVE` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_RETENTION_DIFF` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_RETENTION_FULL` | `CFGOPTDEF_TYPE_INTEGER` |
| cfgRuleOptionType | `CFGOPT_SET` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_SPOOL_PATH` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_STANZA` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_START_FAST` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_STOP_AUTO` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_TABLESPACE_MAP` | `CFGOPTDEF_TYPE_HASH` |
| cfgRuleOptionType | `CFGOPT_TABLESPACE_MAP_ALL` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_TARGET` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_TARGET_ACTION` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_TARGET_EXCLUSIVE` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_TARGET_TIMELINE` | `CFGOPTDEF_TYPE_STRING` |
| cfgRuleOptionType | `CFGOPT_TEST` | `CFGOPTDEF_TYPE_BOOLEAN` |
| cfgRuleOptionType | `CFGOPT_TEST_DELAY` | `CFGOPTDEF_TYPE_FLOAT` |
| cfgRuleOptionType | `CFGOPT_TEST_POINT` | `CFGOPTDEF_TYPE_HASH` |
| cfgRuleOptionType | `CFGOPT_TYPE` | `CFGOPTDEF_TYPE_STRING` |

## cfgRuleOptionValid

Is the option valid for this command?

### Truth Table:

Permutations that return `false` are excluded for brevity.

| Function | commandId | optionId | Result |
| -------- | --------- | -------- | ------ |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_GET` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_ASYNC` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_MAX_MB` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_QUEUE_MAX` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_SPOOL_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_DELAY` | `true` |
| cfgRuleOptionValid | `CFGCMD_ARCHIVE_PUSH` | `CFGOPT_TEST_POINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_COPY` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_CHECKSUM_PAGE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_HARDLINK` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_MANIFEST_SAVE_THRESHOLD` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_RESUME` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_RETENTION_FULL` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_START_FAST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_STOP_AUTO` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_TEST` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_TEST_DELAY` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_TEST_POINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_BACKUP` | `CFGOPT_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_CHECK` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_ARCHIVE_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_CHECK` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_ARCHIVE_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_DIFF` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_RETENTION_FULL` | `true` |
| cfgRuleOptionValid | `CFGCMD_EXPIRE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_OUTPUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_INFO` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_COMMAND` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_HOST_ID` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_PROCESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_LOCAL` | `CFGOPT_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_COMMAND` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB3_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB4_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB5_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB6_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB7_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB8_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_PROCESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_REMOTE` | `CFGOPT_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB_INCLUDE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_DELTA` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LINK_ALL` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LINK_MAP` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_PROCESS_MAX` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_RECOVERY_OPTION` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_SET` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TABLESPACE_MAP_ALL` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_ACTION` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_EXCLUSIVE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TARGET_TIMELINE` | `true` |
| cfgRuleOptionValid | `CFGCMD_RESTORE` | `CFGOPT_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_CREATE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_STANDBY` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_BUFFER_SIZE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_COMPRESS_LEVEL_NETWORK` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB1_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB2_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB3_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB4_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB5_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB6_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB7_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_SOCKET_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_DB8_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_ONLINE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_PROTOCOL_TIMEOUT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STANZA_UPGRADE` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_START` | `CFGOPT_STANZA` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_BACKUP_USER` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_CMD_SSH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB1_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB1_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB1_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB1_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB2_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB2_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB2_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB2_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB3_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB3_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB3_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB4_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB4_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB4_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB4_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB5_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB5_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB5_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB5_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB6_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB6_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB6_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB6_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB7_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB7_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB7_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB7_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB8_CMD` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB8_CONFIG` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB8_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_DB8_SSH_PORT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_FORCE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOCK_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_CONSOLE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOG_LEVEL_STDERR` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOG_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_LOG_TIMESTAMP` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_NEUTRAL_UMASK` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_BUCKET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_FILE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_CA_PATH` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_ENDPOINT` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_HOST` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_KEY_SECRET` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_REGION` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_S3_VERIFY_SSL` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_REPO_TYPE` | `true` |
| cfgRuleOptionValid | `CFGCMD_STOP` | `CFGOPT_STANZA` | `true` |
