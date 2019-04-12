/***********************************************************************************************************************************
Common functions and definitions for backup and expire commands
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_COMMON_H
#define COMMAND_BACKUP_COMMON_H

#include <stdbool.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
backupRegExpGet returns an anchored regex string for filtering backups based on the type (at least one type is required to be true)
***********************************************************************************************************************************/
typedef struct BackupRegExpGetParam
{
    bool full;
    bool differential;
    bool incremental;
} BackupRegExpGetParam;

#define backupRegExpGetP(...)                                                                                                      \
    backupRegExpGet((BackupRegExpGetParam){__VA_ARGS__})

String *backupRegExpGet(BackupRegExpGetParam param);

#endif
