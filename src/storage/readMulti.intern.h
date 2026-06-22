/***********************************************************************************************************************************
Storage Read Multi Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_MULTI_INTERN_H
#define STORAGE_READ_MULTI_INTERN_H

#include "common/io/read.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef struct StorageReadMultiInterface
{
    void (*add)(void *driver, const String *file, StorageReadMultiAddParam param);  // Add a file to the read
    void (*filterGroup)(void *driver, IoFilterGroup *filterGroup);  // Set filter group (optional)
    bool (*open)(void *driver);                                     // Open read
    size_t (*read)(void *driver, Buffer *buffer, bool block);       // Read bytes
    bool (*eof)(void *driver);                                      // Is read eof?
    void (*close)(void *driver);                                    // Close read
} StorageReadMultiInterface;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadMulti *storageReadMultiNew(const Storage *storage, unsigned int prefetch, uint64_t readOver);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadMultiPub
{
    IoRead *io;                                                     // Read interface
} StorageReadMultiPub;

FN_INLINE_ALWAYS const StorageReadMultiInterface *
storageReadMultiDriverInterface(const void *const driver)
{
    return *(const StorageReadMultiInterface *const *)driver;
}

#endif
