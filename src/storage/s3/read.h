/***********************************************************************************************************************************
S3 Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_S3_READ_H
#define STORAGE_S3_READ_H

#include "storage/read.h"
#include "storage/s3/storage.intern.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadS3New(
    StorageS3 *storage, const String *name, bool ignoreMissing, uint64_t offset, const Variant *limit, bool version,
    const String *versionId);

#endif
