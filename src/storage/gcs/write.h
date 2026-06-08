/***********************************************************************************************************************************
GCS Storage File Write
***********************************************************************************************************************************/
#ifndef STORAGE_GCS_WRITE_H
#define STORAGE_GCS_WRITE_H

#include "storage/gcs/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageWriteGcs StorageWriteGcs;

FN_EXTERN StorageWriteGcs *storageWriteGcsNew(StorageGcs *storage, const String *name, size_t chunkSize, bool tag);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_WRITE_GCS_TYPE                                                                                        \
    StorageWriteGcs *
#define FUNCTION_LOG_STORAGE_WRITE_GCS_FORMAT(value, buffer, bufferSize)                                                           \
    objNameToLog(value, "StorageWriteGcs", buffer, bufferSize)

#endif
