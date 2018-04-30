/***********************************************************************************************************************************
Storage File Read
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "storage/fileRead.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileRead
{
    MemContext *memContext;
    StorageFileReadPosix *fileDriver;
};

/***********************************************************************************************************************************
Create a new storage file
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadNew(const String *name, bool ignoreMissing, size_t bufferSize)
{
    StorageFileRead *this = NULL;

    ASSERT_DEBUG(name != NULL);

    MEM_CONTEXT_NEW_BEGIN("StorageFileRead")
    {
        this = memNew(sizeof(StorageFileRead));
        this->memContext = memContextCurrent();

        // Call driver function
        this->fileDriver = storageFileReadPosixNew(name, ignoreMissing, bufferSize);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
bool
storageFileReadOpen(StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileReadPosixOpen(this->fileDriver);
}

/***********************************************************************************************************************************
Read data from the file
***********************************************************************************************************************************/
Buffer *
storageFileRead(StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileReadPosix(this->fileDriver);
}

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
StorageFileRead *
storageFileReadMove(StorageFileRead *this, MemContext *parentNew)
{
    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    return this;
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileReadClose(StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    storageFileReadPosixClose(this->fileDriver);
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
StorageFileReadPosix *
storageFileReadFileDriver(const StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->fileDriver;
}

/***********************************************************************************************************************************
Should a missing file be ignored?
***********************************************************************************************************************************/
bool
storageFileReadIgnoreMissing(const StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileReadPosixIgnoreMissing(this->fileDriver);
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileReadName(const StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileReadPosixName(this->fileDriver);
}

/***********************************************************************************************************************************
Get file size
***********************************************************************************************************************************/
size_t
storageFileReadSize(const StorageFileRead *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileReadPosixSize(this->fileDriver);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileReadFree(StorageFileRead *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
