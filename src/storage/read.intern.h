/***********************************************************************************************************************************
Storage Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_INTERN_H
#define STORAGE_READ_INTERN_H

#include "common/io/read.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct StorageReadInterface
{
    void (*filterGroup)(void *driver, IoFilterGroup *filterGroup);  // Set filter group
    bool (*open)(void *driver, bool ignoreMissing);                 // Open read
    size_t (*read)(void *driver, Buffer *buffer, bool block);       // Read bytes
    bool (*eof)(void *driver);                                      // Is read eof?
    int (*fd)(const void *driver);                                  // Read file descriptor (optional)
    void (*close)(void *driver);                                    // Close read
} StorageReadInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageRead *storageReadNew(
    const Storage *storage, const String *name, bool ignoreMissing, bool compressible, uint64_t offset, const Variant *limit,
    bool version, const String *versionId);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
FN_INLINE_ALWAYS const StorageReadInterface *
storageReadDriverInterface(const void *const driver)
{
    return *(const StorageReadInterface *const *)driver;
}

#endif
