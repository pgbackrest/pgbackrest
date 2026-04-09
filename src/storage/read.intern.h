/***********************************************************************************************************************************
Storage Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_INTERN_H
#define STORAGE_READ_INTERN_H

#include "common/io/read.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageReadInterface
{
    const String *name;                                             // File name
    uint64_t offset;                                                // Where to start reading in the file
    Variant *limit;                                                 // Limit how many bytes are read (NULL for no limit)
    bool ignoreMissing;                                             // Ignore missing file?
    bool version;                                                   // Read version
    const String *versionId;                                        // File version to read
} StorageReadInterface;

typedef struct StorageReadNewParam
{
    VAR_PARAM_HEADER;
    bool retry;                                                     // Are read retries allowed?
    bool version;                                                   // Read version
    const String *versionId;                                        // File version to read
} StorageReadNewParam;

#define storageReadNewP(driver, type, name, ignoreMissing, offset, limit, ioInterface, ...)                                        \
    storageReadNew(                                                                                                                \
        driver, type, name, ignoreMissing, offset, limit, ioInterface, (StorageReadNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN StorageRead *storageReadNew(
    void *driver, StringId type, const String *name, bool ignoreMissing, uint64_t offset, const Variant *limit,
    const IoReadInterface *ioInterface, StorageReadNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadPub
{
    StorageReadInterface interface;                                 // File data (name, driver type, etc.)
    IoRead *io;                                                     // Read interface
    StringId type;                                                  // Storage type
} StorageReadPub;

// Read interface
FN_INLINE_ALWAYS const StorageReadInterface *
storageReadInterface(const StorageRead *const this)
{
    return &THIS_PUB(StorageRead)->interface;
}

#endif
