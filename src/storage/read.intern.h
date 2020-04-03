/***********************************************************************************************************************************
Storage Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_READ_INTERN_H
#define STORAGE_READ_INTERN_H

#include "common/io/read.intern.h"
#include "storage/read.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct StorageReadInterface
{
    const String *type;
    const String *name;
    bool compressible;                                              // Is this file compressible?
    unsigned int compressLevel;                                     // Level to use for compression
    bool ignoreMissing;
    const Variant *limit;                                           // Limit how many bytes are read (NULL for no limit)
    IoReadInterface ioInterface;
} StorageReadInterface;

StorageRead *storageReadNew(void *driver, const StorageReadInterface *interface);

#endif
