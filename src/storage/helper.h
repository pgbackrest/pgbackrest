/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_HELPER_H
#define STORAGE_HELPER_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage path constants
***********************************************************************************************************************************/
#define STORAGE_SPOOL_ARCHIVE                                       "<SPOOL:ARCHIVE>"
STRING_DECLARE(STORAGE_SPOOL_ARCHIVE_STR);
#define STORAGE_SPOOL_ARCHIVE_IN                                    "<SPOOL:ARCHIVE:IN>"
STRING_DECLARE(STORAGE_SPOOL_ARCHIVE_IN_STR);
#define STORAGE_SPOOL_ARCHIVE_OUT                                   "<SPOOL:ARCHIVE:OUT>"
STRING_DECLARE(STORAGE_SPOOL_ARCHIVE_OUT_STR);

#define STORAGE_REPO_ARCHIVE                                        "<REPO:ARCHIVE>"
STRING_DECLARE(STORAGE_REPO_ARCHIVE_STR);
#define STORAGE_REPO_BACKUP                                         "<REPO:BACKUP>"
STRING_DECLARE(STORAGE_REPO_BACKUP_STR);

#define STORAGE_PATH_ARCHIVE                                        "archive"
STRING_DECLARE(STORAGE_PATH_ARCHIVE_STR);
#define STORAGE_PATH_BACKUP                                         "backup"
STRING_DECLARE(STORAGE_PATH_BACKUP_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize dry-run for the current command. No writes are allowed until dry-run has been initialized and no writes are allowed
// after initialization if dry-run is true. Note that storageLocalWrite() is exempt from this rule. The primary purpose is to
// prevent damage to the repository from an error in dry-run coding in the individual commands.
FN_EXTERN void storageHelperDryRunInit(bool dryRun);

// Initialize helpers to create storage other than built-in Posix
FN_EXTERN void storageHelperInit(const StorageHelper *helperList);

// Local storage object. Writable local storage should be used very sparingly. If writes are not needed then always use
// storageLocal() or a specific storage object instead.
FN_EXTERN const Storage *storageLocal(void);
FN_EXTERN const Storage *storageLocalWrite(void);

// PostgreSQL storage by cfgOptGrpPg index
FN_EXTERN const Storage *storagePgIdx(unsigned int pgIdx);
FN_EXTERN const Storage *storagePgIdxWrite(unsigned int pgIdx);

// PostgreSQL storage default (calculated from the pg option, when set, or the first cfgOptGrpPg index)
FN_EXTERN const Storage *storagePg(void);
FN_EXTERN const Storage *storagePgWrite(void);

// Repository storage by cfgOptGrpRepo index
FN_EXTERN const Storage *storageRepoIdx(unsigned int repoIdx);
FN_EXTERN const Storage *storageRepoIdxWrite(unsigned int repoIdx);

// Repository storage default (calculated from the repo option, when set, or the first cfgOptGrpPg index)
FN_EXTERN const Storage *storageRepo(void);
FN_EXTERN const Storage *storageRepoWrite(void);

// Return target time for repository storage, if any
FN_EXTERN time_t storageRepoTargetTime(void);

// Spool storage
FN_EXTERN const Storage *storageSpool(void);
FN_EXTERN const Storage *storageSpoolWrite(void);

// Free cached storage objects
FN_EXTERN void storageHelperFree(void);

#endif
