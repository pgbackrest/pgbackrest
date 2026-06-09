/***********************************************************************************************************************************
Azure Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_READ_H
#define STORAGE_AZURE_READ_H

#include "storage/azure/storage.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadAzure StorageReadAzure;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadAzure *storageReadAzureNew(
    StorageAzure *storage, const String *name, uint64_t offset, const Variant *limit, const String *versionId);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_AZURE_TYPE                                                                                       \
    StorageReadAzure *
#define FUNCTION_LOG_STORAGE_READ_AZURE_FORMAT(value, buffer, bufferSize)                                                          \
    objNameToLog(value, "StorageReadAzure", buffer, bufferSize)

#endif
