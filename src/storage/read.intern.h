/***********************************************************************************************************************************
Storage Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_INTERN_H
#define STORAGE_READ_INTERN_H

#include "common/io/read.h"
#include "storage/range.h"

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
    bool proxy;                                                     // Is a proxy for another read object?
    bool version;                                                   // Read version
    const String *versionId;                                        // File version to read
} StorageReadNewParam;

#define storageReadNewP(driver, type, name, ignoreMissing, rangeList, ioInterface, ...)                                            \
    storageReadNew(driver, type, name, ignoreMissing, rangeList, ioInterface, (StorageReadNewParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN StorageRead *storageReadNew(
    void *driver, StringId type, const String *name, bool ignoreMissing, const StorageRangeList *rangeList,
    const IoReadInterface *ioInterface, StorageReadNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadPub
{
    StorageReadInterface interface;                                 // File data (name, driver type, etc.)
    IoRead *io;                                                     // Read interface
    const StorageRangeList *rangeList;                              // Range list (for reading ranges from a file)
    bool ignoreMissing;                                             // Ignore missing file?
    StringId type;                                                  // Storage type
} StorageReadPub;

// Read interface
FN_INLINE_ALWAYS const StorageReadInterface *
storageReadInterface(const StorageRead *const this)
{
    return &THIS_PUB(StorageRead)->interface;
}

#endif
