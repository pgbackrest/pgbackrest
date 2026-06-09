/***********************************************************************************************************************************
Azure Storage File Write
***********************************************************************************************************************************/
#ifndef STORAGE_AZURE_WRITE_H
#define STORAGE_AZURE_WRITE_H

#include "storage/azure/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageWriteAzure StorageWriteAzure;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWriteAzure *storageWriteAzureNew(StorageAzure *storage, const String *name, uint64_t fileId, size_t blockSize);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_AZURE_TYPE                                                                                      \
    StorageWriteAzure *
#define FUNCTION_LOG_STORAGE_WRITE_AZURE_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(value, "StorageWriteAzure", buffer, bufferSize)

#endif
