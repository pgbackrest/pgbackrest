/***********************************************************************************************************************************
Storage File Write Driver For Posix
***********************************************************************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/log.h"
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
    bool createPath;
    bool syncFile;
    bool syncPath;
    bool atomic;

    int handle;
};

/***********************************************************************************************************************************
File open constants

Since open is called more than once use constants to make sure these parameters are always the same
***********************************************************************************************************************************/
#define FILE_OPEN_FLAGS                                             (O_CREAT | O_TRUNC | O_WRONLY)
#define FILE_OPEN_PURPOSE                                           "write"

/***********************************************************************************************************************************
Create a new file
***********************************************************************************************************************************/
StorageFileWritePosix *
storageFileWritePosixNew(
    const String *name, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, name);
        FUNCTION_DEBUG_PARAM(MODE, modeFile);
        FUNCTION_DEBUG_PARAM(MODE, modePath);
        FUNCTION_DEBUG_PARAM(BOOL, createPath);
        FUNCTION_DEBUG_PARAM(BOOL, syncFile);
        FUNCTION_DEBUG_PARAM(BOOL, syncPath);
        FUNCTION_DEBUG_PARAM(BOOL, atomic);

        FUNCTION_TEST_ASSERT(name != NULL);
    FUNCTION_DEBUG_END();

    StorageFileWritePosix *this = NULL;

    ASSERT_DEBUG(name != NULL);

    // Create the file
    MEM_CONTEXT_NEW_BEGIN("StorageFileWritePosix")
    {
        this = memNew(sizeof(StorageFileWritePosix));
        this->memContext = MEM_CONTEXT_NEW();
        this->path = strPath(name);
        this->name = strDup(name);
        this->nameTmp = atomic ? strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strPtr(name)) : this->name;
        this->modeFile = modeFile;
        this->modePath = modePath;
        this->createPath = createPath;
        this->syncFile = syncFile;
        this->syncPath = syncPath;
        this->atomic = atomic;

        this->handle = -1;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_WRITE_POSIX, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageFileWritePosixOpen(StorageFileWritePosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->handle == -1);
    FUNCTION_DEBUG_END();

    // Open the file and handle errors
    this->handle = storageFilePosixOpen(
        this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, this->createPath, true, FILE_OPEN_PURPOSE);

    // If path is missing
    if (this->handle == -1)
    {
        // Create the path
        storageDriverPosixPathCreate(this->path, false, false, this->modePath);

        // Try the open again
        this->handle = storageFilePosixOpen(
            this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, false, true, FILE_OPEN_PURPOSE);
    }
    // On success set free callback to ensure file handle is freed
    else
        memContextCallback(this->memContext, (MemContextCallback)storageFileWritePosixFree, this);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write to a file
***********************************************************************************************************************************/
void
storageFileWritePosix(StorageFileWritePosix *this, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE_POSIX, this);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(buffer != NULL);
        FUNCTION_TEST_ASSERT(this->handle != -1);
    FUNCTION_DEBUG_END();

    // Write the data
    if (write(this->handle, bufPtr(buffer), bufUsed(buffer)) != (ssize_t)bufUsed(buffer))
        THROW_SYS_ERROR_FMT(FileWriteError, "unable to write '%s'", strPtr(this->name));

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Close the file
***********************************************************************************************************************************/
void
storageFileWritePosixClose(StorageFileWritePosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Sync the file
        if (this->syncFile)
            storageFilePosixSync(this->handle, this->name, true, false);

        // Close the file
        storageFilePosixClose(this->handle, this->name, true);

        // Rename from temp file
        if (this->atomic)
        {
            if (rename(strPtr(this->nameTmp), strPtr(this->name)) == -1)
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strPtr(this->nameTmp), strPtr(this->name));
        }

        // Sync the path
        if (this->syncPath)
            storageDriverPosixPathSync(this->path, false);

        // This marks the file as closed
        this->handle = -1;
    }

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Will the file be written atomically?

For the posix driver this means writing to a temp file first and then renaming once it is closed and synced.
***********************************************************************************************************************************/
bool
storageFileWritePosixAtomic(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->atomic);
}

/***********************************************************************************************************************************
Will the path be created for the file if it does not exist?
***********************************************************************************************************************************/
bool
storageFileWritePosixCreatePath(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->createPath);
}

/***********************************************************************************************************************************
Mode for the file to be created
***********************************************************************************************************************************/
mode_t
storageFileWritePosixModeFile(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->modeFile);
}

/***********************************************************************************************************************************
Mode for any paths that are created while writing the file
***********************************************************************************************************************************/
mode_t
storageFileWritePosixModePath(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->modePath);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageFileWritePosixName(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->name);
}

/***********************************************************************************************************************************
File path
***********************************************************************************************************************************/
const String *
storageFileWritePosixPath(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->path);
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageFileWritePosixSyncFile(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->syncFile);
}

/***********************************************************************************************************************************
Will the directory be synced to disk after the write is completed?
***********************************************************************************************************************************/
bool
storageFileWritePosixSyncPath(StorageFileWritePosix *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_FILE_WRITE_POSIX, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->syncPath);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageFileWritePosixFree(StorageFileWritePosix *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE_POSIX, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        storageFileWritePosixClose(this);
        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
