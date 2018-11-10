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
const Storage *storageRepo(void);
const Storage *storageSpool(void);
const Storage *storageSpoolWrite(void);

#endif
