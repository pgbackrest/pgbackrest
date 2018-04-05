/***********************************************************************************************************************************
Storage Posix Driver
***********************************************************************************************************************************/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/driver/posix.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage file data - holds the file handle.
***********************************************************************************************************************************/
typedef struct StorageFileDataPosix
{
    MemContext *memContext;
    int handle;
} StorageFileDataPosix;

#define STORAGE_DATA(file)                                                                                                         \
    ((StorageFileDataPosix *)storageFileData(file))

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageDriverPosixExists(const String *path)
{
    bool result = false;

    // Attempt to stat the file to determine if it exists
    struct stat statFile;

    // Any error other than entry not found should be reported
    if (stat(strPtr(path), &statFile) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR(FileOpenError, "unable to stat '%s'", strPtr(path));
    }
    // Else found
    else
        result = !S_ISDIR(statFile.st_mode);

    return result;
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *
storageDriverPosixGet(const StorageFile *file)
{
    Buffer volatile *result = NULL;

    TRY_BEGIN()
    {
        // Create result buffer with buffer size
        ssize_t actualBytes = 0;
        size_t totalBytes = 0;

        do
        {
            size_t bufferSize = storageBufferSize(storageFileStorage(file));

            // Allocate the buffer before first read
            if (result == NULL)
                result = bufNew(bufferSize);
            // Grow the buffer on subsequent reads
            else
                bufResize((Buffer *)result, bufSize((Buffer *)result) + bufferSize);

            // Read and handle errors
            actualBytes = read(
                STORAGE_DATA(file)->handle, bufPtr((Buffer *)result) + totalBytes, bufferSize);

            // Error occurred during write
            if (actualBytes == -1)
                THROW_SYS_ERROR(FileReadError, "unable to read '%s'", strPtr(storageFileName(file)));

            // Track total bytes read
            totalBytes += (size_t)actualBytes;
        }
        while (actualBytes != 0);

        // Resize buffer to total bytes read
        bufResize((Buffer *)result, totalBytes);
    }
    CATCH_ANY()
    {
        // Free buffer on error if it was allocated
        bufFree((Buffer *)result);

        RETHROW();
    }
    FINALLY()
    {
        close(STORAGE_DATA(file)->handle);
        storageFileFree(file);
    }
    TRY_END();

    return (Buffer *)result;
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageDriverPosixList(const String *path, bool errorOnMissing, const String *expression)
{
    StringList *result = NULL;

    DIR *dir = NULL;
    RegExp *regExp = NULL;

    TRY_BEGIN()
    {
        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR(PathOpenError, "unable to open directory '%s' for read", strPtr(path));
        }
        else
        {
            // Prepare regexp if an expression was passed
            if (expression != NULL)
                regExp = regExpNew(expression);

            // Create the string list now that we know the directory is valid
            result = strLstNew();

            // Read the directory entries
            struct dirent *dirEntry = readdir(dir);

            while (dirEntry != NULL)
            {
                String *entry = strNew(dirEntry->d_name);

                // Exclude current/parent directory and apply the expression if specified
                if (!strEqZ(entry, ".") && !strEqZ(entry, "..") && (regExp == NULL || regExpMatch(regExp, entry)))
                    strLstAdd(result, entry);
                else
                    strFree(entry);

                dirEntry = readdir(dir);
            }
        }
    }
    CATCH_ANY()
    {
        // Free list on error
        strLstFree(result);

        RETHROW();
    }
    FINALLY()
    {
        if (dir != NULL)
            closedir(dir);

        if (regExp != NULL)
            regExpFree(regExp);
    }
    TRY_END();

    return result;
}

/***********************************************************************************************************************************
Open a file for reading
***********************************************************************************************************************************/
void *
storageDriverPosixOpenRead(const String *file, bool ignoreMissing)
{
    StorageFileDataPosix *result = NULL;

    // Open the file and handle errors
    int fileHandle = open(strPtr(file), O_RDONLY, 0);

    if (fileHandle == -1)
    {
        // Error unless ignore missing is specified
        if (!ignoreMissing || errno != ENOENT)
            THROW_SYS_ERROR(FileOpenError, "unable to open '%s' for read", strPtr(file));
    }
    // Else create the storage file and data
    else
    {
        MEM_CONTEXT_NEW_BEGIN("StorageFileDataPosix")
        {
            result = memNew(sizeof(StorageFileDataPosix));
            result->memContext = MEM_CONTEXT_NEW();
            result->handle = fileHandle;
        }
        MEM_CONTEXT_NEW_END();
    }

    return result;
}

/***********************************************************************************************************************************
Open a file for writing
***********************************************************************************************************************************/
void *
storageDriverPosixOpenWrite(const String *file, mode_t mode)
{
    // Open the file and handle errors
    int fileHandle = open(strPtr(file), O_CREAT | O_TRUNC | O_WRONLY, mode);

    if (fileHandle == -1)
        THROW_SYS_ERROR(FileOpenError, "unable to open '%s' for write", strPtr(file));

    // Create the storage file and data
    StorageFileDataPosix *result = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileDataPosix")
    {
        result = memNew(sizeof(StorageFileDataPosix));
        result->memContext = MEM_CONTEXT_NEW();
        result->handle = fileHandle;
    }
    MEM_CONTEXT_NEW_END();

    return result;
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storageDriverPosixPathCreate(const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    // Attempt to create the directory
    if (mkdir(strPtr(path), mode) == -1)
    {
        // If the parent path does not exist then create it if allowed
        if (errno == ENOENT && !noParentCreate)
        {
            storageDriverPosixPathCreate(strPath(path), errorOnExists, noParentCreate, mode);
            storageDriverPosixPathCreate(path, errorOnExists, noParentCreate, mode);
        }
        // Ignore path exists if allowed
        else if (errno != EEXIST || errorOnExists)
            THROW_SYS_ERROR(PathCreateError, "unable to create path '%s'", strPtr(path));
    }
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void
storageDriverPosixPut(const StorageFile *file, const Buffer *buffer)
{
    TRY_BEGIN()
    {
        if (write(STORAGE_DATA(file)->handle, bufPtr(buffer), bufSize(buffer)) != (ssize_t)bufSize(buffer))
            THROW_SYS_ERROR(FileWriteError, "unable to write '%s'", strPtr(storageFileName(file)));
    }
    FINALLY()
    {
        close(STORAGE_DATA(file)->handle);
        storageFileFree(file);
    }
    TRY_END();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageDriverPosixRemove(const String *file, bool errorOnMissing)
{
    // Attempt to unlink the file
    if (unlink(strPtr(file)) == -1)
    {
        if (errorOnMissing || errno != ENOENT)
            THROW_SYS_ERROR(FileRemoveError, "unable to remove '%s'", strPtr(file));
    }
}
