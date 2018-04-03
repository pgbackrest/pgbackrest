/***********************************************************************************************************************************
Storage File
***********************************************************************************************************************************/
#ifndef STORAGE_FILE_H
#define STORAGE_FILE_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Types of storage files, i.e. read or write.  The storage module does not allow files to be opened for both read and write since this
is generally not supported by object stores.
***********************************************************************************************************************************/
typedef enum
{
    storageFileTypeRead,
    storageFileTypeWrite,
} StorageFileType;

/***********************************************************************************************************************************
Storage file object
***********************************************************************************************************************************/
typedef struct StorageFile StorageFile;

#include "storage/storage.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StorageFile *storageFileNew(const Storage *storage, String *name, StorageFileType type, void *data);

void *storageFileData(const StorageFile *this);
const String *storageFileName(const StorageFile *this);
const Storage *storageFileStorage(const StorageFile *this);

void storageFileFree(const StorageFile *this);

#endif
