/***********************************************************************************************************************************
Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_COMMON_H
#define COMMAND_BACKUP_COMMON_H

#include <stdbool.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Backup constants
***********************************************************************************************************************************/
#define BACKUP_PATH_HISTORY                                         "backup.history"

/***********************************************************************************************************************************
Backup type enum and constants
***********************************************************************************************************************************/
typedef enum
{
    backupTypeFull,
    backupTypeDiff,
    backupTypeIncr,
} BackupType;

#define BACKUP_TYPE_FULL                                            "full"
    STRING_DECLARE(BACKUP_TYPE_FULL_STR);
#define BACKUP_TYPE_DIFF                                            "diff"
    STRING_DECLARE(BACKUP_TYPE_DIFF_STR);
#define BACKUP_TYPE_INCR                                            "incr"
    STRING_DECLARE(BACKUP_TYPE_INCR_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Returns an anchored regex string for filtering backups based on the type (at least one type is required to be true)
typedef struct BackupRegExpParam
{
    bool full;
    bool differential;
    bool incremental;
    bool noAnchorEnd;
} BackupRegExpParam;

#define backupRegExpP(...)                                                                                                         \
    backupRegExp((BackupRegExpParam){__VA_ARGS__})

String *backupRegExp(BackupRegExpParam param);

// Convert text backup type to an enum and back
BackupType backupType(const String *type);
const String *backupTypeStr(BackupType type);

// Create a symlink to the specified backup (if symlinks are supported)
void backupLinkLatest(const String *backupLabel, unsigned int repoIdx);

#endif
