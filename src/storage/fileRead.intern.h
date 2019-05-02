/***********************************************************************************************************************************
Storage File Read Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_FILEREAD_INTERN_H
#define STORAGE_FILEREAD_INTERN_H

#include "common/io/read.intern.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct StorageFileReadInterface
{
    const String * type;
    const String * name;
    bool ignoreMissing;
    IoReadInterface ioInterface;
} StorageFileReadInterface;

StorageFileRead *storageFileReadNew(void *driver, const StorageFileReadInterface *interface);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
void *storageFileReadDriver(const StorageFileRead *this);

#endif
