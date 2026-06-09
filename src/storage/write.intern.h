/***********************************************************************************************************************************
Storage Write Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_WRITE_INTERN_H
#define STORAGE_WRITE_INTERN_H

#include "common/io/write.h"
#include "storage/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PROJECT_BIN ".tmp"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct StorageWriteInterface
{
    void (*filterGroup)(void *driver, IoFilterGroup *filterGroup);  // Set filter group (optional)
    void (*open)(void *driver);                                     // Open write
    void (*write)(void *driver, const Buffer *buffer);              // Write bytes
    void (*seek)(void *driver, uint64_t position);                  // Seek (optional)
    int (*fd)(const void *driver);                                  // Write file descriptor (optional)
    void (*close)(void *driver);                                    // Close write
} StorageWriteInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageWrite *storageWriteNew(
    const Storage *storage, const String *name, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic, bool truncate, bool compressible);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
FN_INLINE_ALWAYS const StorageWriteInterface *
storageWriteDriverInterface(const void *const driver)
{
    return *(const StorageWriteInterface *const *)driver;
}

#endif
