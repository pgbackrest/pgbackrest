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
    StringId type;                                                  // Storage type
    const String *name;
    bool compressible;                                              // Is this file compressible?
    unsigned int compressLevel;                                     // Level to use for compression
    bool ignoreMissing;
    uint64_t offset;                                                // Where to start reading in the file
    const Variant *limit;                                           // Limit how many bytes are read (NULL for no limit)
    bool version;                                                   // Read version
    const String *versionId;                                        // File version to read
    IoReadInterface ioInterface;
} StorageReadInterface;

FN_EXTERN StorageRead *storageReadNew(void *driver, const StorageReadInterface *interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StorageReadPub
{
    const StorageReadInterface *interface;                          // File data (name, driver type, etc.)
    IoRead *io;                                                     // Read interface
} StorageReadPub;

// Read interface
FN_INLINE_ALWAYS const StorageReadInterface *
storageReadInterface(const StorageRead *const this)
{
    return THIS_PUB(StorageRead)->interface;
}

#endif
