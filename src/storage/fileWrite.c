/***********************************************************************************************************************************
Storage File Write
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileWrite
{
    MemContext *memContext;
    StorageFileWritePosix *fileDriver;
};

/***********************************************************************************************************************************
Create a new storage file

This object expects its context to be created in advance.  This is so the calling function can add whatever data it wants without
required multiple functions and contexts to make it safe.
***********************************************************************************************************************************/
StorageFileWrite *
storageFileWriteNew(
    const String *name, mode_t modeFile, mode_t modePath, bool noCreatePath, bool noSyncFile, bool noSyncPath, bool noAtomic)
{
    StorageFileWrite *this = NULL;

    ASSERT_DEBUG(name != NULL);

    // Create the file.  The file driver is not created here because we don't want to open the file for write until we know that
    // there is a source file.
    MEM_CONTEXT_NEW_BEGIN("StorageFileWrite")
    {
        this = memNew(sizeof(StorageFileWrite));
        this->memContext = MEM_CONTEXT_NEW();

        this->fileDriver = storageFileWritePosixNew(name, modeFile, modePath, noCreatePath, noSyncFile, noSyncPath, noAtomic);
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageFileWriteOpen(StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    // Open the file
    storageFileWritePosixOpen(this->fileDriver);
}

/***********************************************************************************************************************************
Write to a file
***********************************************************************************************************************************/
void
storageFileWrite(StorageFileWrite *this, const Buffer *buffer)
{
    ASSERT_DEBUG(this != NULL);

    // Only write if there is data to write
    if (buffer != NULL && bufSize(buffer) > 0)
        storageFileWritePosix(this->fileDriver, buffer);
}

/***********************************************************************************************************************************
Move the file object to a new context
***********************************************************************************************************************************/
StorageFileWrite *
storageFileWriteMove(StorageFileWrite *this, MemContext *parentNew)
{
    if (this != NULL)
        memContextMove(this->memContext, parentNew);

    return this;
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileWriteClose(StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    storageFileWritePosixClose(this->fileDriver);
}

/***********************************************************************************************************************************
Will the file be written atomically?

Atomic writes means the file will be complete or be missing.  Filesystems have different ways to accomplish this.
***********************************************************************************************************************************/
bool
storageFileWriteAtomic(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixAtomic(this->fileDriver);
}

/***********************************************************************************************************************************
Will the path be created if required?
***********************************************************************************************************************************/
bool
storageFileWriteCreatePath(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixCreatePath(this->fileDriver);
}

/***********************************************************************************************************************************
Get file driver
***********************************************************************************************************************************/
StorageFileWritePosix *
storageFileWriteFileDriver(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->fileDriver;
}

/***********************************************************************************************************************************
Get file mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModeFile(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixModeFile(this->fileDriver);
}

/***********************************************************************************************************************************
Get path mode
***********************************************************************************************************************************/
mode_t
storageFileWriteModePath(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixModePath(this->fileDriver);
}

/***********************************************************************************************************************************
Get file name
***********************************************************************************************************************************/
const String *
storageFileWriteName(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixName(this->fileDriver);
}

/***********************************************************************************************************************************
Get file path
***********************************************************************************************************************************/
const String *
storageFileWritePath(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixPath(this->fileDriver);
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncFile(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixSyncFile(this->fileDriver);
}

/***********************************************************************************************************************************
Will the path be synced after the file is closed?
***********************************************************************************************************************************/
bool
storageFileWriteSyncPath(const StorageFileWrite *this)
{
    ASSERT_DEBUG(this != NULL);

    return storageFileWritePosixSyncPath(this->fileDriver);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileWriteFree(const StorageFileWrite *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
