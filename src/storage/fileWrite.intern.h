/***********************************************************************************************************************************
Storage File Write Internal
***********************************************************************************************************************************/
#ifndef STORAGE_FILEWRITE_INTERN_H
#define STORAGE_FILEWRITE_INTERN_H

#include "storage/fileWrite.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PGBACKREST_BIN ".tmp"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef bool (*StorageFileWriteAtomic)(const void *data);
typedef bool (*StorageFileWriteCreatePath)(const void *data);
typedef IoWrite *(*StorageFileWriteIo)(const void *data);
typedef mode_t (*StorageFileWriteModeFile)(const void *data);
typedef mode_t (*StorageFileWriteModePath)(const void *data);
typedef const String *(*StorageFileWriteName)(const void *data);
typedef bool (*StorageFileWriteSyncFile)(const void *data);
typedef bool (*StorageFileWriteSyncPath)(const void *data);

typedef struct StorageFileWriteInterface
{
    StorageFileWriteAtomic atomic;
    StorageFileWriteCreatePath createPath;
    StorageFileWriteIo io;
    StorageFileWriteModeFile modeFile;
    StorageFileWriteModePath modePath;
    StorageFileWriteName name;
    StorageFileWriteSyncFile syncFile;
    StorageFileWriteSyncPath syncPath;
} StorageFileWriteInterface;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageFileWrite *storageFileWriteNew(const String *type, void *driver, const StorageFileWriteInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_FILE_WRITE_INTERFACE_TYPE                                                                           \
    StorageFileWriteInterface *
#define FUNCTION_DEBUG_STORAGE_FILE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                              \
    objToLog(value, "StorageFileWriteInterface", buffer, bufferSize)

#endif
