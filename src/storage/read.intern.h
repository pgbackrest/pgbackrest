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
    int compressLevel;                                              // Level to use for compression
    bool ignoreMissing;
    IoReadInterface ioInterface;
} StorageReadInterface;

StorageRead *storageReadNew(void *driver, const StorageReadInterface *interface);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
void *storageRead(const StorageRead *this);

#endif
