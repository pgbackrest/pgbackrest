/***********************************************************************************************************************************
Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_COMMON_H
#define COMMAND_BACKUP_COMMON_H

#include <stdbool.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Returns an anchored regex string for filtering backups based on the type (at least one type is required to be true)
***********************************************************************************************************************************/
typedef struct BackupRegExpParam
{
    bool full;
    bool differential;
    bool incremental;
} BackupRegExpParam;

#define backupRegExpP(...)                                                                                                         \
    backupRegExp((BackupRegExpParam){__VA_ARGS__})

String *backupRegExp(BackupRegExpParam param);

#endif
