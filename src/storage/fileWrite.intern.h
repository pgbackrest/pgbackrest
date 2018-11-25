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
#define STORAGE_FILE_TEMP_EXT                                       PROJECT_BIN ".tmp"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef bool (*StorageFileWriteInterfaceAtomic)(const void *data);
typedef bool (*StorageFileWriteInterfaceCreatePath)(const void *data);
typedef IoWrite *(*StorageFileWriteInterfaceIo)(const void *data);
typedef mode_t (*StorageFileWriteInterfaceModeFile)(const void *data);
typedef mode_t (*StorageFileWriteInterfaceModePath)(const void *data);
typedef const String *(*StorageFileWriteInterfaceName)(const void *data);
typedef bool (*StorageFileWriteInterfaceSyncFile)(const void *data);
typedef bool (*StorageFileWriteInterfaceSyncPath)(const void *data);

typedef struct StorageFileWriteInterface
{
    StorageFileWriteInterfaceAtomic atomic;
    StorageFileWriteInterfaceCreatePath createPath;
    StorageFileWriteInterfaceIo io;
    StorageFileWriteInterfaceModeFile modeFile;
    StorageFileWriteInterfaceModePath modePath;
    StorageFileWriteInterfaceName name;
    StorageFileWriteInterfaceSyncFile syncFile;
    StorageFileWriteInterfaceSyncPath syncPath;
} StorageFileWriteInterface;

#define storageFileWriteNewP(type, driver, ...)                                                                                    \
    storageFileWriteNew(type, driver, (StorageFileWriteInterface){__VA_ARGS__})

StorageFileWrite *storageFileWriteNew(const String *type, void *driver, StorageFileWriteInterface interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_FILE_WRITE_INTERFACE_TYPE                                                                           \
    StorageFileWriteInterface
#define FUNCTION_DEBUG_STORAGE_FILE_WRITE_INTERFACE_FORMAT(value, buffer, bufferSize)                                              \
    objToLog(&value, "StorageFileWriteInterface", buffer, bufferSize)

#endif
