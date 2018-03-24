/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Storage structure
***********************************************************************************************************************************/
struct Storage
{
    const String *path;
    int mode;
    size_t bufferSize;
    StoragePathExpressionCallback pathExpressionFunction;
};

/***********************************************************************************************************************************
Storage mem context
***********************************************************************************************************************************/
static MemContext *storageMemContext = NULL;

/***********************************************************************************************************************************
New storage object
***********************************************************************************************************************************/
Storage *
storageNew(const String *path, int mode, size_t bufferSize, StoragePathExpressionCallback pathExpressionFunction)
{
    Storage *result = NULL;

    // Path is required
    if (path == NULL)
        THROW(AssertError, "storage base path cannot be null");

    // If storage mem context has not been initialized yet
    if (storageMemContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            storageMemContext = memContextNew("storage");
        }
        MEM_CONTEXT_END();
    }

    // Create the storage
    MEM_CONTEXT_BEGIN(storageMemContext)
    {
        result = (Storage *)memNew(sizeof(Storage));
        result->path = strDup(path);
        result->mode = mode;
        result->bufferSize = bufferSize;
        result->pathExpressionFunction = pathExpressionFunction;
    }
    MEM_CONTEXT_END();

    return result;
}

/***********************************************************************************************************************************
Check for errors on read.  This is a separate function for testing purposes.
***********************************************************************************************************************************/
static void
storageReadError(ssize_t actualBytes, const String *file)
{
    // Error occurred during write
    if (actualBytes == -1)
    {
        int errNo = errno;
        THROW(FileReadError, "unable to read '%s': %s", strPtr(file), strerror(errNo));
    }
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *
storageGet(const Storage *storage, const String *fileExp, bool ignoreMissing)
{
    Buffer volatile *result = NULL;
    volatile int fileHandle = -1;
    String *file = NULL;

    TRY_BEGIN()
    {
        // Build the path
        file = storagePath(storage, fileExp);

        // Open the file and handle errors
        fileHandle = open(strPtr(file), O_RDONLY, storage->mode);

        if (fileHandle == -1)
        {
            int errNo = errno;

            if (!ignoreMissing || errNo != ENOENT)
                THROW(FileOpenError, "unable to open '%s' for read: %s", strPtr(file), strerror(errNo));
        }
        else
        {
            // Create result buffer with buffer size
            ssize_t actualBytes = 0;
            size_t totalBytes = 0;

            do
            {
                // Allocate the buffer before first read
                if (result == NULL)
                    result = bufNew(storage->bufferSize);
                // Grow the buffer on subsequent reads
                else
                    bufResize((Buffer *)result, bufSize((Buffer *)result) + (size_t)storage->bufferSize);

                // Read and handle errors
                actualBytes = read(fileHandle, bufPtr((Buffer *)result) + totalBytes, storage->bufferSize);
                storageReadError(actualBytes, file);

                // Track total bytes read
                totalBytes += (size_t)actualBytes;
            }
            while (actualBytes != 0);

            // Resize buffer to total bytes read
            bufResize((Buffer *)result, totalBytes);
        }
    }
    CATCH_ANY()
    {
        // Free buffer on error if it was allocated
        if (result != NULL)
            bufFree((Buffer *)result);                              // {uncoverable - cannot error after buffer is allocated}

        RETHROW();
    }
    FINALLY()
    {
        // Close file
        if (fileHandle != -1)
            close(fileHandle);

        // Free file name
        strFree(file);
    }
    TRY_END();

    return (Buffer *)result;
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageList(const Storage *storage, const String *pathExp, const String *expression, bool ignoreMissing)
{
    StringList *result = NULL;

    String *path = NULL;
    DIR *dir = NULL;
    RegExp *regExp = NULL;

    TRY_BEGIN()
    {
        // Build the path
        path = storagePath(storage, pathExp);

        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (!ignoreMissing || errno != ENOENT)
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
        if (path != NULL)
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
Get the absolute path in the storage
***********************************************************************************************************************************/
String *
storagePath(const Storage *storage, const String *pathExp)
{
    String *result = NULL;


    // If there there is no path expression then return the base storage path
    if (pathExp == NULL)
    {
        result = strDup(storage->path);
    }
    else
    {
        // If the path expression is absolute then use it as is
        if ((strPtr(pathExp))[0] == '/')
        {
            // Make sure the base storage path is contained within the path expression
            if (!strEqZ(storage->path, "/"))
            {
                if (!strBeginsWith(pathExp, storage->path) ||
                    !(strSize(pathExp) == strSize(storage->path) || *(strPtr(pathExp) + strSize(storage->path)) == '/'))
                {
                    THROW(AssertError, "absolute path '%s' is not in base path '%s'", strPtr(pathExp), strPtr(storage->path));
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
                if (storage->pathExpressionFunction == NULL)
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
                    if (end[2] != 0)
                        path = strNew(end + 2);
                }

                // Evaluate the path
                pathEvaluated = storage->pathExpressionFunction(expression, path);

                // Evaluated path cannot be NULL
                if (pathEvaluated == NULL)
                    THROW(AssertError, "evaluated path '%s' cannot be null", strPtr(pathExp));

                // Assign evaluated path to path
                pathExp = pathEvaluated;

                // Free temp vars
                strFree(expression);
                strFree(path);
            }

            if (strEqZ(storage->path, "/"))
                result = strNewFmt("/%s", strPtr(pathExp));
            else
                result = strNewFmt("%s/%s", strPtr(storage->path), strPtr(pathExp));

            strFree(pathEvaluated);
        }
    }

    return result;
}

/***********************************************************************************************************************************
Check for errors on write.  This is a separate function for testing purposes.
***********************************************************************************************************************************/
static void
storageWriteError(ssize_t actualBytes, size_t expectedBytes, const String *file)
{
    // Error occurred during write
    if (actualBytes == -1)
    {
        int errNo = errno;
        THROW(FileWriteError, "unable to write '%s': %s", strPtr(file), strerror(errNo));
    }

    // Make sure that all expected bytes were written.  Cast to unsigned since we have already tested for -1.
    if ((size_t)actualBytes != expectedBytes)
        THROW(FileWriteError, "only wrote %lu byte(s) to '%s' but %lu byte(s) expected", actualBytes, strPtr(file), expectedBytes);
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void
storagePut(const Storage *storage, const String *fileExp, const Buffer *buffer)
{
    volatile int fileHandle = -1;
    String *file = NULL;

    TRY_BEGIN()
    {
        // Build the path
        file = storagePath(storage, fileExp);

        // Open the file and handle errors
        fileHandle = open(strPtr(file), O_CREAT | O_TRUNC | O_WRONLY, storage->mode);

        if (fileHandle == -1)
        {
            int errNo = errno;
            THROW(FileOpenError, "unable to open '%s' for write: %s", strPtr(file), strerror(errNo));
        }

        // Write data if buffer is not null.  Otherwise, and empty file is expected.
        if (buffer != NULL)
            storageWriteError(write(fileHandle, bufPtr(buffer), bufSize(buffer)), bufSize(buffer), file);
    }
    FINALLY()
    {
        if (fileHandle != -1)
            close(fileHandle);

        // Free file name
        strFree(file);
    }
    TRY_END();
}
