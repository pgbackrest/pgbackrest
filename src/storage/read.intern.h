/***********************************************************************************************************************************
Storage Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_INTERN_H
#define STORAGE_READ_INTERN_H

#include "common/io/read.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadNew(
    const Storage *storage, const String *name, bool ignoreMissing, bool compressible, uint64_t offset, const Variant *limit,
    bool version, const String *versionId);

#endif
