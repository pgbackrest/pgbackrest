/***********************************************************************************************************************************
Storage Write Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_WRITE_INTERN_H
#define STORAGE_WRITE_INTERN_H

#include "common/io/write.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PROJECT_BIN ".tmp"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageWriteInterface
{
    const String *name;                                             // File name
    bool createPath;                                                // Will the path be created if required?
    bool atomic;                                                    // Are writes atomic?
    bool truncate;                                                  // Truncate file if it exists
    bool syncPath;                                                  // Will path be synced?
    bool syncFile;                                                  // Will file be synced?
    const String *user;                                             // Set user ownership
    const String *group;                                            // Set group ownership
    mode_t modePath;                                                // Path mode
    mode_t modeFile;                                                // File mode
    time_t timeModified;                                            // Set modification time
} StorageWriteInterface;

typedef struct StorageWriteNewParam
{
    VAR_PARAM_HEADER;
    const String *user;                                             // Set user ownership
    const String *group;                                            // Set group ownership
    mode_t modePath;                                                // Path mode
    mode_t modeFile;                                                // File mode
    time_t timeModified;                                            // Set modification time
} StorageWriteNewParam;

#define storageWriteNewP(driver, type, name, createPath, atomic, truncate, syncPath, syncFile, ioInterface, ...)                   \
    storageWriteNew(                                                                                                               \
        driver, type, name, createPath, atomic, truncate, syncPath, syncFile, ioInterface,                                         \
        (StorageWriteNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN StorageWrite *storageWriteNew(
    void *driver, StringId type, const String *name, bool createPath, bool atomic, bool truncate, bool syncPath, bool syncFile,
    const IoWriteInterface *ioInterface, StorageWriteNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageWritePub
{
    const StorageWriteInterface *interface;                         // File data (name, driver type, etc.)
    IoWrite *io;                                                    // Write interface
    StringId type;                                                  // Storage type
} StorageWritePub;

// Write interface
FN_INLINE_ALWAYS const StorageWriteInterface *
storageWriteInterface(const StorageWrite *const this)
{
    return THIS_PUB(StorageWrite)->interface;
}

#endif
