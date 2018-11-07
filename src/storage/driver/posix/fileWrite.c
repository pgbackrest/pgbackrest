/***********************************************************************************************************************************
Posix Storage File Write Driver
***********************************************************************************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/io/write.intern.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/posix/common.h"
#include "storage/driver/posix/fileWrite.h"
#include "storage/fileWrite.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageDriverPosixFileWrite
{
    MemContext *memContext;
    const StorageDriverPosix *storage;
    StorageFileWrite *interface;
    IoWrite *io;

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
StorageDriverPosixFileWrite *
storageDriverPosixFileWriteNew(
    const StorageDriverPosix *storage, const String *name, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile,
    bool syncPath, bool atomic)
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
        FUNCTION_TEST_ASSERT(modeFile != 0);
        FUNCTION_TEST_ASSERT(modePath != 0);
    FUNCTION_DEBUG_END();

    StorageDriverPosixFileWrite *this = NULL;

    // Create the file
    MEM_CONTEXT_NEW_BEGIN("StorageDriverPosixFileWrite")
    {
        this = memNew(sizeof(StorageDriverPosixFileWrite));
        this->memContext = MEM_CONTEXT_NEW();
        this->storage = storage;

        this->interface = storageFileWriteNewP(
            strNew(STORAGE_DRIVER_POSIX_TYPE), this, .atomic = (StorageFileWriteInterfaceAtomic)storageDriverPosixFileWriteAtomic,
            .createPath = (StorageFileWriteInterfaceCreatePath)storageDriverPosixFileWriteCreatePath,
            .io = (StorageFileWriteInterfaceIo)storageDriverPosixFileWriteIo,
            .modeFile = (StorageFileWriteInterfaceModeFile)storageDriverPosixFileWriteModeFile,
            .modePath = (StorageFileWriteInterfaceModePath)storageDriverPosixFileWriteModePath,
            .name = (StorageFileWriteInterfaceName)storageDriverPosixFileWriteName,
            .syncFile = (StorageFileWriteInterfaceSyncFile)storageDriverPosixFileWriteSyncFile,
            .syncPath = (StorageFileWriteInterfaceSyncPath)storageDriverPosixFileWriteSyncPath);

        this->io = ioWriteNewP(
            this, .close = (IoWriteInterfaceClose)storageDriverPosixFileWriteClose,
            .open = (IoWriteInterfaceOpen)storageDriverPosixFileWriteOpen,
            .write = (IoWriteInterfaceWrite)storageDriverPosixFileWrite);

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

    FUNCTION_DEBUG_RESULT(STORAGE_DRIVER_POSIX_FILE_WRITE, this);
}

/***********************************************************************************************************************************
Open the file
***********************************************************************************************************************************/
void
storageDriverPosixFileWriteOpen(StorageDriverPosixFileWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_TEST_ASSERT(this->handle == -1);
    FUNCTION_DEBUG_END();

    // Open the file and handle errors
    this->handle = storageDriverPosixFileOpen(
        this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, this->createPath, true, FILE_OPEN_PURPOSE);

    // If path is missing
    if (this->handle == -1)
    {
        // Create the path
        storageDriverPosixPathCreate(this->storage, this->path, false, false, this->modePath);

        // Try the open again
        this->handle = storageDriverPosixFileOpen(
            this->nameTmp, FILE_OPEN_FLAGS, this->modeFile, false, true, FILE_OPEN_PURPOSE);
    }
    // On success set free callback to ensure file handle is freed
    else
        memContextCallback(this->memContext, (MemContextCallback)storageDriverPosixFileWriteFree, this);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write to a file
***********************************************************************************************************************************/
void
storageDriverPosixFileWrite(StorageDriverPosixFileWrite *this, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);
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
storageDriverPosixFileWriteClose(StorageDriverPosixFileWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    // Close if the file has not already been closed
    if (this->handle != -1)
    {
        // Sync the file
        if (this->syncFile)
            storageDriverPosixFileSync(this->handle, this->name, true, false);

        // Close the file
        storageDriverPosixFileClose(this->handle, this->name, true);

        // Rename from temp file
        if (this->atomic)
        {
            if (rename(strPtr(this->nameTmp), strPtr(this->name)) == -1)
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strPtr(this->nameTmp), strPtr(this->name));
        }

        // Sync the path
        if (this->syncPath)
            storageDriverPosixPathSync(this->storage, this->path, false);

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
storageDriverPosixFileWriteAtomic(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->atomic);
}

/***********************************************************************************************************************************
Will the path be created for the file if it does not exist?
***********************************************************************************************************************************/
bool
storageDriverPosixFileWriteCreatePath(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->createPath);
}

/***********************************************************************************************************************************
Get interface
***********************************************************************************************************************************/
StorageFileWrite *
storageDriverPosixFileWriteInterface(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(STORAGE_FILE_WRITE, this->interface);
}

/***********************************************************************************************************************************
Get I/O interface
***********************************************************************************************************************************/
IoWrite *
storageDriverPosixFileWriteIo(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(IO_WRITE, this->io);
}

/***********************************************************************************************************************************
Mode for the file to be created
***********************************************************************************************************************************/
mode_t
storageDriverPosixFileWriteModeFile(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->modeFile);
}

/***********************************************************************************************************************************
Mode for any paths that are created while writing the file
***********************************************************************************************************************************/
mode_t
storageDriverPosixFileWriteModePath(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(MODE, this->modePath);
}

/***********************************************************************************************************************************
File name
***********************************************************************************************************************************/
const String *
storageDriverPosixFileWriteName(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(CONST_STRING, this->name);
}

/***********************************************************************************************************************************
Will the file be synced after it is closed?
***********************************************************************************************************************************/
bool
storageDriverPosixFileWriteSyncFile(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->syncFile);
}

/***********************************************************************************************************************************
Will the directory be synced to disk after the write is completed?
***********************************************************************************************************************************/
bool
storageDriverPosixFileWriteSyncPath(const StorageDriverPosixFileWrite *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RESULT(BOOL, this->syncPath);
}

/***********************************************************************************************************************************
Free the file
***********************************************************************************************************************************/
void
storageDriverPosixFileWriteFree(StorageDriverPosixFileWrite *this)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE_DRIVER_POSIX_FILE_WRITE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
    {
        storageDriverPosixFileWriteClose(this);

        memContextCallbackClear(this->memContext);
        memContextFree(this->memContext);
    }

    FUNCTION_DEBUG_RESULT_VOID();
}
