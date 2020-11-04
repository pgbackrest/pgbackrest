/***********************************************************************************************************************************
Storage Helper
***********************************************************************************************************************************/
#ifndef STORAGE_HELPER_H
#define STORAGE_HELPER_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Storage path constants
***********************************************************************************************************************************/
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
// Initialize dry-run for the current command. No writes are allowed until dry-run has been intitialized and no writes are allowed
// after initialization if dry-run is true. Note that storageLocalWrite() is exempt from this rule. The primary purpose is to
// prevent damage to the repository from an error in dry-run coding in the individual commands.
void storageHelperDryRunInit(bool dryRun);

// Local storage object. Writable local storage should be used very sparingly. If writes are not needed then always use
// storageLocal() or a specific storage object instead.
const Storage *storageLocal(void);
const Storage *storageLocalWrite(void);

// PostgreSQL storage by cfgOptGrpPg index
const Storage *storagePgIdx(unsigned int pgIdx);
const Storage *storagePgIdxWrite(unsigned int pgIdx);

// PostgreSQL storage default (calculated from pg-default, when set, or the first cfgOptGrpPg index)
const Storage *storagePg(void);
const Storage *storagePgWrite(void);

// Repository storage
const Storage *storageRepo(void);
const Storage *storageRepoWrite(void);

// Spool storage
const Storage *storageSpool(void);
const Storage *storageSpoolWrite(void);

// Free all storage helper objects. This should be done on any config load to ensure that stanza changes are honored. ?? Currently
// this is only done in testing, but in the future it will likely be done in production as well.
void storageHelperFree(void);

#endif
