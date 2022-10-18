/***********************************************************************************************************************************
Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#ifndef COMMAND_BACKUP_COMMON_H
#define COMMAND_BACKUP_COMMON_H

#include <stdbool.h>
#include <time.h>

#include "common/compress/helper.h"
#include "common/type/string.h"
#include "info/infoBackup.h"

/***********************************************************************************************************************************
Backup constants
***********************************************************************************************************************************/
#define BACKUP_PATH_HISTORY                                         "backup.history"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Determine the path/file where the file is backed up in the repo
typedef struct BackupFileRepoPathParam
{
    const String *manifestName;                                     // File name in manifest
    uint64_t bundleId;                                              // Is the file bundled?
    CompressType compressType;                                      // Is the file compressed?
} BackupFileRepoPathParam;

#define backupFileRepoPathP(backupLabel, ...)                                                                                          \
    backupFileRepoPath(backupLabel, (BackupFileRepoPathParam){__VA_ARGS__})

String *backupFileRepoPath(const String *backupLabel, BackupFileRepoPathParam param);

// Format a backup label from a type and timestamp with an optional prior label
String *backupLabelFormat(BackupType type, const String *backupLabelPrior, time_t timestamp);

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

// Create a symlink to the specified backup (if symlinks are supported)
void backupLinkLatest(const String *backupLabel, unsigned int repoIdx);

#endif
