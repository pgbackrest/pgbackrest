/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage structure
***********************************************************************************************************************************/
struct Storage
{
    MemContext *memContext;
    String *path;
    mode_t modeFile;
    mode_t modePath;
    size_t bufferSize;
    bool write;
    StoragePathExpressionCallback pathExpressionFunction;
};

/***********************************************************************************************************************************
Storage file data - holds the file handle.  This should eventually be moved to the Posix/CIFS driver.
***********************************************************************************************************************************/
typedef struct StorageFileDataPosix
{
    MemContext *memContext;
    int handle;
} StorageFileDataPosix;

#define STORAGE_DATA(file)                                                                                                         \
    ((StorageFileDataPosix *)storageFileData(file))

/***********************************************************************************************************************************
Debug Asserts
***********************************************************************************************************************************/
// Check that commands that write are not allowed unless the storage is writable
#define ASSERT_STORAGE_ALLOWS_WRITE()                                                                                                             \
    ASSERT(this->write == true)

/***********************************************************************************************************************************
New storage object
***********************************************************************************************************************************/
Storage *
storageNew(const String *path, StorageNewParam param)
{
    Storage *this = NULL;

    // Path is required
    if (path == NULL)
        THROW(AssertError, "storage base path cannot be null");

    // Create the storage
    MEM_CONTEXT_NEW_BEGIN("Storage")
    {
        this = (Storage *)memNew(sizeof(Storage));
        this->memContext = MEM_CONTEXT_NEW();
        this->path = strDup(path);
        this->modeFile = param.modeFile == 0 ? STORAGE_FILE_MODE_DEFAULT : param.modeFile;
        this->modePath = param.modePath == 0 ? STORAGE_PATH_MODE_DEFAULT : param.modePath;
        this->bufferSize = param.bufferSize == 0 ? STORAGE_BUFFER_SIZE_DEFAULT : param.bufferSize;
        this->write = param.write;
        this->pathExpressionFunction = param.pathExpressionFunction;
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageExists(const Storage *this, const String *pathExp, StorageExistsParam param)
{
    bool result = false;

    // Timeout can't be negative
    ASSERT_DEBUG(param.timeout >= 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Create Wait object of timeout > 0
        Wait *wait = param.timeout != 0 ? waitNew(param.timeout) : NULL;

        // Attempt to stat the file to determine if it exists
        struct stat statFile;

        do
        {
            // Any error other than entry not found should be reported
            if (stat(strPtr(path), &statFile) == -1)
            {
                if (errno != ENOENT)
                    THROW_SYS_ERROR(FileOpenError, "unable to stat '%s'", strPtr(path));
            }
            // Else found
            else
                result = true;
        }
        while (!result && wait != NULL && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *
storageGet(const StorageFile *file)
{
    Buffer volatile *result = NULL;

    // Nothing to do unless a file was passed
    if (file != NULL)
    {
        TRY_BEGIN()
        {
            // Create result buffer with buffer size
            ssize_t actualBytes = 0;
            size_t totalBytes = 0;

            do
            {
                // Allocate the buffer before first read
                if (result == NULL)
                    result = bufNew(storageFileStorage(file)->bufferSize);
                // Grow the buffer on subsequent reads
                else
                    bufResize((Buffer *)result, bufSize((Buffer *)result) + (size_t)storageFileStorage(file)->bufferSize);

                // Read and handle errors
                actualBytes = read(
                    STORAGE_DATA(file)->handle, bufPtr((Buffer *)result) + totalBytes, storageFileStorage(file)->bufferSize);

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
    }

    return (Buffer *)result;
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageList(const Storage *this, const String *pathExp, StorageListParam param)
{
    StringList *result = NULL;

    String *path = NULL;
    DIR *dir = NULL;
    RegExp *regExp = NULL;

    TRY_BEGIN()
    {
        // Build the path
        path = storagePathNP(this, pathExp);

        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (param.errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR(PathOpenError, "unable to open directory '%s' for read", strPtr(path));
        }
        else
        {
            // Prepare regexp if an expression was passed
            if (param.expression != NULL)
                regExp = regExpNew(param.expression);

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
        strFree(path);

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
StorageFile *
storageOpenRead(const Storage *this, const String *fileExp, StorageOpenReadParam param)
{
    StorageFile *result = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileRead")
    {
        String *fileName = storagePathNP(this, fileExp);
        int fileHandle;

        // Open the file and handle errors
        fileHandle = open(strPtr(fileName), O_RDONLY, 0);

        if (fileHandle == -1)
        {
            // Error unless ignore missing is specified
            if (!param.ignoreMissing || errno != ENOENT)
                THROW_SYS_ERROR(FileOpenError, "unable to open '%s' for read", strPtr(fileName));

            // Free mem contexts if missing files are ignored
            memContextSwitch(MEM_CONTEXT_OLD());
            memContextFree(MEM_CONTEXT_NEW());
        }
        else
        {
            // Create the storage file and data
            StorageFileDataPosix *data = NULL;

            MEM_CONTEXT_NEW_BEGIN("StorageFileReadDataPosix")
            {
                data = memNew(sizeof(StorageFileDataPosix));
                data->memContext = MEM_CONTEXT_NEW();
                data->handle = fileHandle;
            }
            MEM_CONTEXT_NEW_END();

            result = storageFileNew(this, fileName, storageFileTypeRead, data);
        }
    }
    MEM_CONTEXT_NEW_END();

    return result;
}

/***********************************************************************************************************************************
Open a file for writing
***********************************************************************************************************************************/
StorageFile *
storageOpenWrite(const Storage *this, const String *fileExp, StorageOpenWriteParam param)
{
    ASSERT_STORAGE_ALLOWS_WRITE();

    StorageFile *result = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFileWrite")
    {
        String *fileName = storagePathNP(this, fileExp);
        int fileHandle;

        // Open the file and handle errors
        fileHandle = open(
            strPtr(fileName), O_CREAT | O_TRUNC | O_WRONLY, param.mode == 0 ? this->modeFile : param.mode);

        if (fileHandle == -1)
            THROW_SYS_ERROR(FileOpenError, "unable to open '%s' for write", strPtr(fileName));

        // Create the storage file and data
        StorageFileDataPosix *data = NULL;

        MEM_CONTEXT_NEW_BEGIN("StorageFileReadDataPosix")
        {
            data = memNew(sizeof(StorageFileDataPosix));
            data->memContext = MEM_CONTEXT_NEW();
            data->handle = fileHandle;
        }
        MEM_CONTEXT_NEW_END();

        result = storageFileNew(this, fileName, storageFileTypeWrite, data);
    }
    MEM_CONTEXT_NEW_END();

    return result;
}

/***********************************************************************************************************************************
Get the absolute path in the storage
***********************************************************************************************************************************/
String *
storagePath(const Storage *this, const String *pathExp)
{
    String *result = NULL;

    // If there there is no path expression then return the base storage path
    if (pathExp == NULL)
    {
        result = strDup(this->path);
    }
    else
    {
        // If the path expression is absolute then use it as is
        if ((strPtr(pathExp))[0] == '/')
        {
            // Make sure the base storage path is contained within the path expression
            if (!strEqZ(this->path, "/"))
            {
                if (!strBeginsWith(pathExp, this->path) ||
                    !(strSize(pathExp) == strSize(this->path) || *(strPtr(pathExp) + strSize(this->path)) == '/'))
                {
                    THROW(AssertError, "absolute path '%s' is not in base path '%s'", strPtr(pathExp), strPtr(this->path));
                }
            }

            result = strDup(pathExp);
        }
        // Else path expression is relative so combine it with the base storage path
        else
        {
            // There may or may not be a path expression that needs to be evaluated
            String *pathEvaluated = NULL;

            // Check if there is a path expression that needs to be evaluated
            if ((strPtr(pathExp))[0] == '<')
            {
                if (this->pathExpressionFunction == NULL)
                    THROW(AssertError, "expression '%s' not valid without callback function", strPtr(pathExp));

                // Get position of the expression end
                char *end = strchr(strPtr(pathExp), '>');

                // Error if end is not found
                if (end == NULL)
                    THROW(AssertError, "end > not found in path expression '%s'", strPtr(pathExp));

                // Create a string from the expression
                String *expression = strNewN(strPtr(pathExp), (size_t)(end - strPtr(pathExp) + 1));

                // Create a string from the path if there is anything left after the expression
                String *path = NULL;

                if (strSize(expression) < strSize(pathExp))
                {
                    // Error if path separator is not found
                    if (end[1] != '/')
                        THROW(AssertError, "'/' should separate expression and path '%s'", strPtr(pathExp));

                    // Only create path if there is something after the path separator
                    if (end[2] == 0)
                        THROW(AssertError, "path '%s' should not end in '/'", strPtr(pathExp));

                    path = strNew(end + 2);
                }

                // Evaluate the path
                pathEvaluated = this->pathExpressionFunction(expression, path);

                // Evaluated path cannot be NULL
                if (pathEvaluated == NULL)
                    THROW(AssertError, "evaluated path '%s' cannot be null", strPtr(pathExp));

                // Assign evaluated path to path
                pathExp = pathEvaluated;

                // Free temp vars
                strFree(expression);
                strFree(path);
            }

            if (strEqZ(this->path, "/"))
                result = strNewFmt("/%s", strPtr(pathExp));
            else
                result = strNewFmt("%s/%s", strPtr(this->path), strPtr(pathExp));

            strFree(pathEvaluated);
        }
    }

    return result;
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param)
{
    ASSERT_STORAGE_ALLOWS_WRITE();

    // It doesn't make sense to combine these parameters because if we are creating missing parent paths why error when they exist?
    // If this somehow wasn't caught in testing, the worst case is that the path would not be created and an error would be thrown.
    ASSERT_DEBUG(!(param.noParentCreate && param.errorOnExists));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Attempt to create the directory
        if (mkdir(strPtr(path), param.mode != 0 ? param.mode : STORAGE_PATH_MODE_DEFAULT) == -1)
        {
            // If the parent path does not exist then create it if allowed
            if (errno == ENOENT && !param.noParentCreate)
            {
                storagePathCreate(this, strPath(path), param);
                storagePathCreate(this, path, param);
            }
            // Ignore path exists if allowed
            else if (errno != EEXIST || param.errorOnExists)
                THROW_SYS_ERROR(PathCreateError, "unable to create path '%s'", strPtr(path));
        }
    }
    MEM_CONTEXT_TEMP_END();
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void
storagePut(const StorageFile *file, const Buffer *buffer)
{
    // Write data if buffer is not null.  Otherwise, an empty file is expected.
    if (buffer != NULL)
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
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageRemove(const Storage *this, const String *pathExp, StorageRemoveParam param)
{
    ASSERT_STORAGE_ALLOWS_WRITE();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, pathExp);

        // Attempt to unlink the file
        if (unlink(strPtr(file)) == -1)
        {
            if (param.errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR(FileRemoveError, "unable to remove '%s'", strPtr(file));
        }
    }
    MEM_CONTEXT_TEMP_END();
}

/***********************************************************************************************************************************
Stat a file
***********************************************************************************************************************************/
StorageStat *
storageStat(const Storage *this, const String *pathExp, StorageStatParam param)
{
    StorageStat *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Attempt to stat the file
        struct stat statFile;

        if (stat(strPtr(path), &statFile) == -1)
        {
            if (errno != ENOENT || !param.ignoreMissing)
                THROW_SYS_ERROR(FileOpenError, "unable to stat '%s'", strPtr(path));
        }
        // Else set stats
        else
        {
            memContextSwitch(MEM_CONTEXT_OLD());
            result = memNew(sizeof(StorageStat));

            result->mode = statFile.st_mode & 0777;

            memContextSwitch(MEM_CONTEXT_TEMP());
        }
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}

/***********************************************************************************************************************************
Free storage
***********************************************************************************************************************************/
void
storageFree(const Storage *this)
{
    if (this != NULL)
        memContextFree(this->memContext);
}
