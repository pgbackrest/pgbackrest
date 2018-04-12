/***********************************************************************************************************************************
Storage File
***********************************************************************************************************************************/
#ifndef STORAGE_FILE_H
#define STORAGE_FILE_H

/***********************************************************************************************************************************
Storage file object
***********************************************************************************************************************************/
typedef struct StorageFile StorageFile;

/***********************************************************************************************************************************
Types of storage files, i.e. read or write.  The storage module does not allow files to be opened for both read and write since this
is generally not supported by object stores.
***********************************************************************************************************************************/
typedef enum
{
    storageFileTypeRead,
    storageFileTypeWrite,
} StorageFileType;

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
