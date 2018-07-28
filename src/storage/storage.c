/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/driver/posix/driver.h"
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
    bool write;
    StoragePathExpressionCallback pathExpressionFunction;
};

/***********************************************************************************************************************************
New storage object
***********************************************************************************************************************************/
Storage *
storageNew(const String *path, StorageNewParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, path);
        FUNCTION_DEBUG_PARAM(MODE, param.modeFile);
        FUNCTION_DEBUG_PARAM(MODE, param.modePath);
        FUNCTION_DEBUG_PARAM(BOOL, param.write);
        FUNCTION_DEBUG_PARAM(FUNCTIONP, param.pathExpressionFunction);

        FUNCTION_DEBUG_ASSERT(path != NULL);
    FUNCTION_DEBUG_END();

    Storage *this = NULL;

    // Create the storage
    MEM_CONTEXT_NEW_BEGIN("Storage")
    {
        this = (Storage *)memNew(sizeof(Storage));
        this->memContext = MEM_CONTEXT_NEW();
        this->path = strDup(path);
        this->modeFile = param.modeFile == 0 ? STORAGE_FILE_MODE_DEFAULT : param.modeFile;
        this->modePath = param.modePath == 0 ? STORAGE_PATH_MODE_DEFAULT : param.modePath;
        this->write = param.write;
        this->pathExpressionFunction = param.pathExpressionFunction;
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_DEBUG_RESULT(STORAGE, this);
}

/***********************************************************************************************************************************
Copy a file
***********************************************************************************************************************************/
bool
storageCopy(StorageFileRead *source, StorageFileWrite *destination)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, source);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE, destination);

        FUNCTION_TEST_ASSERT(source != NULL);
        FUNCTION_TEST_ASSERT(destination != NULL);
    FUNCTION_DEBUG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Open source file
        if (ioReadOpen(storageFileReadIo(source)))
        {
            // Open the destination file now that we know the source file exists and is readable
            ioWriteOpen(storageFileWriteIo(destination));

            // Copy data from source to destination
            Buffer *read = bufNew(ioBufferSize(source));

            do
            {
                ioRead(storageFileReadIo(source), read);
                ioWrite(storageFileWriteIo(destination), read);
                bufUsedZero(read);
            }
            while (!ioReadEof(storageFileReadIo(source)));

            // Close the source and destination files
            ioReadClose(storageFileReadIo(source));
            ioWriteClose(storageFileWriteIo(destination));

            // Set result to indicate that the file was copied
            result = true;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Does a file/path exist?
***********************************************************************************************************************************/
bool
storageExists(const Storage *this, const String *pathExp, StorageExistsParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, pathExp);
        FUNCTION_DEBUG_PARAM(DOUBLE, param.timeout);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(param.timeout >= 0);
    FUNCTION_DEBUG_END();

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Create Wait object of timeout > 0
        Wait *wait = param.timeout != 0 ? waitNew(param.timeout) : NULL;

        // Loop until file exists or timeout
        do
        {
            // Call driver function
            result = storageDriverPosixExists(path);
        }
        while (!result && wait != NULL && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *
storageGet(StorageFileRead *file, StorageGetParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, file);
        FUNCTION_DEBUG_PARAM(SIZE, param.exactSize);

        FUNCTION_TEST_ASSERT(file != NULL);
    FUNCTION_DEBUG_END();

    Buffer *result = NULL;

    // If the file exists
    if (ioReadOpen(storageFileReadIo(file)))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // If exact read
            if (param.exactSize > 0)
            {
                result = bufNew(param.exactSize);
                ioRead(storageFileReadIo(file), result);

                // If an exact read make sure the size is as expected
                if (bufUsed(result) != param.exactSize)
                {
                    THROW_FMT(
                        FileReadError, "unable to read %zu byte(s) from '%s'", param.exactSize, strPtr(storageFileReadName(file)));
                }
            }
            // Else read entire file
            else
            {
                result = bufNew(0);
                Buffer *read = bufNew(ioBufferSize(file));

                do
                {
                    // Read data
                    ioRead(storageFileReadIo(file), read);

                    // Add to result and free read buffer
                    bufCat(result, read);
                    bufUsedZero(read);
                }
                while (!ioReadEof(storageFileReadIo(file)));
            }

            // Move buffer to parent context on success
            bufMove(result, MEM_CONTEXT_OLD());
        }
        MEM_CONTEXT_TEMP_END();

        ioReadClose(storageFileReadIo(file));
    }

    FUNCTION_DEBUG_RESULT(BUFFER, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
StorageInfo
storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, fileExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.ignoreMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    StorageInfo result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, fileExp);

        // Call driver function
        result = storageDriverPosixInfo(file, param.ignoreMissing);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageList(const Storage *this, const String *pathExp, StorageListParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, pathExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.errorOnMissing);
        FUNCTION_DEBUG_PARAM(STRING, param.expression);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Move list up to the old context
        result = strLstMove(storageDriverPosixList(path, param.errorOnMissing, param.expression), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STRING_LIST, result);
}

/***********************************************************************************************************************************
Move a file
***********************************************************************************************************************************/
void
storageMove(StorageFileRead *source, StorageFileWrite *destination)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_READ, source);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE, destination);

        FUNCTION_TEST_ASSERT(source != NULL);
        FUNCTION_TEST_ASSERT(destination != NULL);
        FUNCTION_DEBUG_ASSERT(!storageFileReadIgnoreMissing(source));
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file can't be moved it will need to be copied
        if (!storageDriverPosixMove(storageFileReadDriver(source), storageFileWriteFileDriver(destination)))
        {
            // Perform the copy
            storageCopyNP(source, destination);

            // Remove the source file
            storageDriverPosixRemove(storageFileReadName(source), false);

            // Sync source path if the destination path was synced.  We know the source and destination paths are different because
            // the move did not succeed.  This will need updating when drivers other than Posix/CIFS are implemented.
            if (storageFileWriteSyncPath(destination))
                storageDriverPosixPathSync(strPath(storageFileReadName(source)), false);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Open a file for reading
***********************************************************************************************************************************/
StorageFileRead *
storageNewRead(const Storage *this, const String *fileExp, StorageNewReadParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, fileExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.ignoreMissing);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, param.filterGroup);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_DEBUG_END();

    StorageFileRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = storageFileReadNew(storagePathNP(this, fileExp), param.ignoreMissing);

        if (param.filterGroup != NULL)
            ioReadFilterGroupSet(storageFileReadIo(result), param.filterGroup);

        storageFileReadMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_READ, result);
}

/***********************************************************************************************************************************
Open a file for writing
***********************************************************************************************************************************/
StorageFileWrite *
storageNewWrite(const Storage *this, const String *fileExp, StorageNewWriteParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, fileExp);
        FUNCTION_DEBUG_PARAM(MODE, param.modeFile);
        FUNCTION_DEBUG_PARAM(MODE, param.modePath);
        FUNCTION_DEBUG_PARAM(BOOL, param.noCreatePath);
        FUNCTION_DEBUG_PARAM(BOOL, param.noSyncFile);
        FUNCTION_DEBUG_PARAM(BOOL, param.noSyncPath);
        FUNCTION_DEBUG_PARAM(BOOL, param.noAtomic);
        FUNCTION_DEBUG_PARAM(IO_FILTER_GROUP, param.filterGroup);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->write);
    FUNCTION_DEBUG_END();

    StorageFileWrite *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = storageFileWriteNew(
            storagePathNP(this, fileExp), param.modeFile != 0 ? param.modeFile : this->modeFile,
            param.modePath != 0 ? param.modePath : this->modePath, param.noCreatePath, param.noSyncFile, param.noSyncPath,
            param.noAtomic);

        if (param.filterGroup != NULL)
            ioWriteFilterGroupSet(storageFileWriteIo(result), param.filterGroup);

        storageFileWriteMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(STORAGE_FILE_WRITE, result);
}

/***********************************************************************************************************************************
Get the absolute path in the storage
***********************************************************************************************************************************/
String *
storagePath(const Storage *this, const String *pathExp)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
        FUNCTION_TEST_PARAM(STRING, pathExp);

        FUNCTION_TEST_ASSERT(this != NULL);
    FUNCTION_TEST_END();

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
                    THROW_FMT(AssertError, "absolute path '%s' is not in base path '%s'", strPtr(pathExp), strPtr(this->path));
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
                    THROW_FMT(AssertError, "expression '%s' not valid without callback function", strPtr(pathExp));

                // Get position of the expression end
                char *end = strchr(strPtr(pathExp), '>');

                // Error if end is not found
                if (end == NULL)
                    THROW_FMT(AssertError, "end > not found in path expression '%s'", strPtr(pathExp));

                // Create a string from the expression
                String *expression = strNewN(strPtr(pathExp), (size_t)(end - strPtr(pathExp) + 1));

                // Create a string from the path if there is anything left after the expression
                String *path = NULL;

                if (strSize(expression) < strSize(pathExp))
                {
                    // Error if path separator is not found
                    if (end[1] != '/')
                        THROW_FMT(AssertError, "'/' should separate expression and path '%s'", strPtr(pathExp));

                    // Only create path if there is something after the path separator
                    if (end[2] == 0)
                        THROW_FMT(AssertError, "path '%s' should not end in '/'", strPtr(pathExp));

                    path = strNew(end + 2);
                }

                // Evaluate the path
                pathEvaluated = this->pathExpressionFunction(expression, path);

                // Evaluated path cannot be NULL
                if (pathEvaluated == NULL)
                    THROW_FMT(AssertError, "evaluated path '%s' cannot be null", strPtr(pathExp));

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

    FUNCTION_TEST_RESULT(STRING, result);
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, pathExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.errorOnExists);
        FUNCTION_DEBUG_PARAM(BOOL, param.noParentCreate);
        FUNCTION_DEBUG_PARAM(MODE, param.mode);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->write);

        // It doesn't make sense to combine these parameters because if we are creating missing parent paths why error when they
        // exist? If this somehow wasn't caught in testing, the worst case is that the path would not be created and an error would
        // be thrown.
        FUNCTION_TEST_ASSERT(!(param.noParentCreate && param.errorOnExists));
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        storageDriverPosixPathCreate(
            path, param.errorOnExists, param.noParentCreate, param.mode != 0 ? param.mode : this->modePath);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
void
storagePathRemove(const Storage *this, const String *pathExp, StoragePathRemoveParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, pathExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.errorOnMissing);
        FUNCTION_DEBUG_PARAM(BOOL, param.recurse);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->write);
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        storageDriverPosixPathRemove(path, param.errorOnMissing, param.recurse);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
void storagePathSync(const Storage *this, const String *pathExp, StoragePathSyncParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, pathExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.ignoreMissing);

        FUNCTION_TEST_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->write);
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        storageDriverPosixPathSync(path, param.ignoreMissing);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void
storagePut(StorageFileWrite *file, const Buffer *buffer)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE_FILE_WRITE, file);
        FUNCTION_DEBUG_PARAM(BUFFER, buffer);

        FUNCTION_DEBUG_ASSERT(file != NULL);
    FUNCTION_DEBUG_END();

    ioWriteOpen(storageFileWriteIo(file));
    ioWrite(storageFileWriteIo(file), buffer);
    ioWriteClose(storageFileWriteIo(file));

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
        FUNCTION_DEBUG_PARAM(STRING, fileExp);
        FUNCTION_DEBUG_PARAM(BOOL, param.errorOnMissing);

        FUNCTION_DEBUG_ASSERT(this != NULL);
        FUNCTION_DEBUG_ASSERT(this->write);
    FUNCTION_DEBUG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, fileExp);

        // Call driver function
        storageDriverPosixRemove(file, param.errorOnMissing);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Convert to a zero-terminated string for logging
***********************************************************************************************************************************/
size_t
storageToLog(const Storage *this, char *buffer, size_t bufferSize)
{
    size_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *string = NULL;

        if (this == NULL)
            string = strNew("null");
        else
            string = strNewFmt("{path: %s, write: %s}", strPtr(strQuoteZ(this->path, "\"")), cvtBoolToConstZ(this->write));

        result = (size_t)snprintf(buffer, bufferSize, "%s", strPtr(string));
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
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STORAGE, this);
    FUNCTION_DEBUG_END();

    if (this != NULL)
        memContextFree(this->memContext);

    FUNCTION_DEBUG_RESULT_VOID();
}
