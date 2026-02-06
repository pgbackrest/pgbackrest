/***********************************************************************************************************************************
Azure Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_READ_H
#define STORAGE_AZURE_READ_H

#include "storage/azure/storage.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadAzureNew(
    StorageAzure *storage, const String *name, bool ignoreMissing, const StorageRangeList *rangeList, bool version,
    const String *versionId);

#endif
