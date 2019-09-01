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
#define STORAGE_REPO_BACKUP                                         "<REPO:BACKUP>"

#define STORAGE_PATH_ARCHIVE                                        "archive"
    STRING_DECLARE(STORAGE_PATH_ARCHIVE_STR);
#define STORAGE_PATH_BACKUP                                         "backup"
    STRING_DECLARE(STORAGE_PATH_BACKUP_STR);

/***********************************************************************************************************************************
Repository storage types
***********************************************************************************************************************************/
#define STORAGE_TYPE_CIFS                                           "cifs"
#define STORAGE_TYPE_POSIX                                          "posix"
#define STORAGE_TYPE_S3                                             "s3"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
const Storage *storageLocal(void);
const Storage *storageLocalWrite(void);
const Storage *storagePg(void);
const Storage *storagePgId(unsigned int hostId);
const Storage *storagePgWrite(void);
const Storage *storagePgIdWrite(unsigned int hostId);
const Storage *storageRepo(void);
const Storage *storageRepoWrite(void);
const Storage *storageSpool(void);
const Storage *storageSpoolWrite(void);

void storageHelperFree(void);

#endif
