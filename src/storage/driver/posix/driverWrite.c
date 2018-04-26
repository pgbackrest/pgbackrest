/***********************************************************************************************************************************
Storage File Write Driver For Posix
***********************************************************************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/memContext.h"
#include "storage/driver/posix/driverFile.h"
#include "storage/driver/posix/driverWrite.h"
#include "storage/driver/posix/driver.h"
#include "storage/fileWrite.h"

/***********************************************************************************************************************************
Storage file structure
***********************************************************************************************************************************/
struct StorageFileWritePosix
{
    MemContext *memContext;

    String *path;
    String *name;
    String *nameTmp;
    mode_t modeFile;
    mode_t modePath;
    bool noCreatePath;
    bool noSyncFile;
    bool noSyncPath;
    bool noAtomic;

    int handle;
};

/***********************************************************************************************************************************
File open constants

Since open is called more than once use constants to make sure these parameters are always the same
***********************************************************************************************************************************/
#define FILE_OPEN_FLAGS                                             (O_CREAT | O_TRUNC | O_WRONLY)
#define FILE_OPEN_PURPOSE                                           "write"
#define FILE_OPEN_ERROR                                             &FileOpenError

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageFileWritePosix *
storageFileWritePosixNew(
    const String *name, mode_t modeFile, mode_t modePath, bool noCreatePath, bool noSyncFile, bool noSyncPath, bool noAtomic)
{
    StorageFileWritePosix *this = NULL;

    ASSERT_DEBUG(name != NULL);

    // Create the file
    MEM_CONTEXT_NEW_BEGIN("StorageFileWritePosix")
    {
        this = memNew(sizeof(StorageFileWritePosix));
        this->memContext = MEM_CONTEXT_NEW();
        this->path = strPath(name);
        this->name = strDup(name);
        this->nameTmp = noAtomic ? this->name : strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strPtr(name));
        this->modeFile = modeFile;
        this->modePath = modePath;
        this->noCreatePath = noCreatePath;
        this->noSyncFile = noSyncFile;
        this->noSyncPath = noSyncPath;
        this->noAtomic = noAtomic;

        this->handle = -1;
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageFileWritePosixOpen(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);
    ASSERT_DEBUG(this->handle == -1);

    // Open the file and handle errors
    this->handle = storageFilePosixOpen(
        this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, !this->noCreatePath, FILE_OPEN_ERROR, FILE_OPEN_PURPOSE);

    // If path is missing
    if (this->handle == -1)
    {
        // Create the path
        storageDriverPosixPathCreate(this->path, false, false, this->modePath);

        // Try the open again
        this->handle = storageFilePosixOpen(
            this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, false, FILE_OPEN_ERROR, FILE_OPEN_PURPOSE);
    }
    // On success set free callback to ensure file handle is freed
    else
        memContextCallback(this->memContext, (MemContextCallback)storageFileWritePosixFree, this);
}

/***********************************************************************************************************************************
Write to a file
***********************************************************************************************************************************/
void
storageFileWritePosix(StorageFileWritePosix *this, const Buffer *buffer)
{
    ASSERT_DEBUG(this != NULL);
    ASSERT_DEBUG(buffer != NULL);
    ASSERT_DEBUG(this->handle != -1);

    // Write the data
    if (write(this->handle, bufPtr(buffer), bufSize(buffer)) != (ssize_t)bufSize(buffer))
        THROW_SYS_ERROR(FileWriteError, "unable to write '%s'", strPtr(this->name));
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileWritePosixClose(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Sync the file
        if (!this->noSyncFile)
            storageFilePosixSync(this->handle, this->name, &FileSyncError, false);

        // Close the file
        storageFilePosixClose(this->handle, this->name, &FileCloseError);

        // Rename from temp file
        if (!this->noAtomic)
        {
            if (rename(strPtr(this->nameTmp), strPtr(this->name)) == -1)
                THROW_SYS_ERROR(FileMoveError, "unable to move '%s' to '%s'", strPtr(this->nameTmp), strPtr(this->name));
        }

        // Sync the path
        if (!this->noSyncPath)
            storageDriverPosixPathSync(this->path, false);

        // This marks the file as closed
        this->handle = -1;
    }
}

/***********************************************************************************************************************************
Will the file be written atomically?

For the posix driver this means writing to a temp file first and then renaming once it is closed and synced.
***********************************************************************************************************************************/
bool
storageFileWritePosixAtomic(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return !this->noAtomic;
}

/***********************************************************************************************************************************
Will the path be created for the file if it does not exist?
***********************************************************************************************************************************/
bool
storageFileWritePosixCreatePath(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return !this->noCreatePath;
}

/***********************************************************************************************************************************
Mode for the file to be created
***********************************************************************************************************************************/
mode_t
storageFileWritePosixModeFile(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->modeFile;
}

/***********************************************************************************************************************************
Mode for any paths that are created while writing the file
***********************************************************************************************************************************/
mode_t
storageFileWritePosixModePath(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->modePath;
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageFileWritePosixName(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->name;
}

/***********************************************************************************************************************************
File path
***********************************************************************************************************************************/
const String *
storageFileWritePosixPath(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return this->path;
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageFileWritePosixSyncFile(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return !this->noSyncFile;
}

/***********************************************************************************************************************************
Will the directory be synced to disk after the write is completed?
***********************************************************************************************************************************/
bool
storageFileWritePosixSyncPath(StorageFileWritePosix *this)
{
    ASSERT_DEBUG(this != NULL);

    return !this->noSyncPath;
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileWritePosixFree(StorageFileWritePosix *this)
{
    if (this != NULL)
    {
        storageFileWritePosixClose(this);
        memContextFree(this->memContext);
    }
}
