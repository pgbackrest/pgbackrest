/***********************************************************************************************************************************
Storage File
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/memContext.h"
#include "storage/file.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFile
{
    MemContext *memContext;
    const Storage *storage;
    String *name;
    StorageFileType type;
    void *data;
};

/***********************************************************************************************************************************
Create a new storage file

This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageFile *storageFileNew(const Storage *storage, String *name, StorageFileType type, void *data)
{
    ASSERT_DEBUG(storage != NULL);
    ASSERT_DEBUG(name != NULL);
    ASSERT_DEBUG(data != NULL);

    StorageFile *this = memNew(sizeof(StorageFile));
    this->memContext = memContextCurrent();
    this->storage = storage;
    this->name = name;
    this->type = type;
    this->data = data;

    return this;
}

/***********************************************************************************************************************************
Get file data
***********************************************************************************************************************************/
void *
storageFileData(const StorageFile *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->data;
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileName(const StorageFile *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->name;
}

/***********************************************************************************************************************************
Get file storage object
***********************************************************************************************************************************/
const Storage *
storageFileStorage(const StorageFile *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->storage;
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileFree(const StorageFile *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
