/***********************************************************************************************************************************
Storage Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/object.h"
#include "common/wait.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Storage
{
    MemContext *memContext;
    void *driver;
    StorageInterface interface;
    const String *type;

    const String *path;
    mode_t modeFile;
    mode_t modePath;
    bool write;
    bool pathEnforce;
    StoragePathExpressionCallback pathExpressionFunction;
};

OBJECT_DEFINE_FREE(STORAGE);

/***********************************************************************************************************************************
New storage object
***********************************************************************************************************************************/
Storage *
storageNew(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, void *driver, StorageInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(STORAGE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(path == NULL || (strSize(path) >= 1 && strPtr(path)[0] == '/'));
    ASSERT(driver != NULL);
    ASSERT(interface.exists != NULL);
    ASSERT(interface.list != NULL);
    ASSERT(interface.newRead != NULL);
    ASSERT(interface.newWrite != NULL);
    ASSERT(interface.pathRemove != NULL);
    ASSERT(interface.remove != NULL);

    Storage *this = NULL;
    this = (Storage *)memNew(sizeof(Storage));
    this->memContext = memContextCurrent();
    this->driver = driver;
    this->interface = interface;
    this->type = type;

    this->path = strDup(path);
    this->modeFile = modeFile;
    this->modePath = modePath;
    this->pathEnforce = true;
    this->write = write;
    this->pathExpressionFunction = pathExpressionFunction;

    FUNCTION_LOG_RETURN(STORAGE, this);
}

/***********************************************************************************************************************************
Copy a file
***********************************************************************************************************************************/
bool
storageCopy(StorageRead *source, StorageWrite *destination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
    FUNCTION_LOG_END();

    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Open source file
        if (ioReadOpen(storageReadIo(source)))
        {
            // Open the destination file now that we know the source file exists and is readable
            ioWriteOpen(storageWriteIo(destination));

            // Copy data from source to destination
            Buffer *read = bufNew(ioBufferSize());

            do
            {
                ioRead(storageReadIo(source), read);
                ioWrite(storageWriteIo(destination), read);
                bufUsedZero(read);
            }
            while (!ioReadEof(storageReadIo(source)));

            // Close the source and destination files
            ioReadClose(storageReadIo(source));
            ioWriteClose(storageWriteIo(destination));

            // Set result to indicate that the file was copied
            result = true;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
bool
storageExists(const Storage *this, const String *pathExp, StorageExistsParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(TIMEMSEC, param.timeout);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path to the file
        String *path = storagePathNP(this, pathExp);

        // Create Wait object of timeout > 0
        Wait *wait = param.timeout != 0 ? waitNew(param.timeout) : NULL;

        // Loop until file exists or timeout
        do
        {
            // Call driver function
            result = this->interface.exists(this->driver, path);
        }
        while (!result && wait != NULL && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Read from storage into a buffer
***********************************************************************************************************************************/
Buffer *
storageGet(StorageRead *file, StorageGetParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_READ, file);
        FUNCTION_LOG_PARAM(SIZE, param.exactSize);
    FUNCTION_LOG_END();

    ASSERT(file != NULL);

    Buffer *result = NULL;

    // If the file exists
    if (ioReadOpen(storageReadIo(file)))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // If exact read
            if (param.exactSize > 0)
            {
                result = bufNew(param.exactSize);
                ioRead(storageReadIo(file), result);

                // If an exact read make sure the size is as expected
                if (bufUsed(result) != param.exactSize)
                {
                    THROW_FMT(
                        FileReadError, "unable to read %zu byte(s) from '%s'", param.exactSize, strPtr(storageReadName(file)));
                }
            }
            // Else read entire file
            else
            {
                result = bufNew(0);
                Buffer *read = bufNew(ioBufferSize());

                do
                {
                    // Read data
                    ioRead(storageReadIo(file), read);

                    // Add to result and free read buffer
                    bufCat(result, read);
                    bufUsedZero(read);
                }
                while (!ioReadEof(storageReadIo(file)));
            }

            // Move buffer to parent context on success
            bufMove(result, MEM_CONTEXT_OLD());
        }
        MEM_CONTEXT_TEMP_END();

        ioReadClose(storageReadIo(file));
    }

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
StorageInfo
storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(BOOL, param.ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->interface.info != NULL);

    StorageInfo result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, fileExp);

        // Call driver function
        result = this->interface.info(this->driver, file, param.followLink);

        // Error if the file missing and not ignoring
        if (!result.exists && !param.ignoreMissing)
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO_MISSING, strPtr(file));

        // Dup the strings into the calling context
        memContextSwitch(MEM_CONTEXT_OLD());
        result.linkDestination = strDup(result.linkDestination);
        result.user = strDup(result.user);
        result.group = strDup(result.group);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Info for all files/paths in a path
***********************************************************************************************************************************/
bool
storageInfoList(
    const Storage *this, const String *pathExp, StorageInfoListCallback callback, void *callbackData, StorageInfoListParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(callback != NULL);
    ASSERT(this->interface.infoList != NULL);
    ASSERT(!param.errorOnMissing || storageFeature(this, storageFeaturePath));

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        result = this->interface.infoList(this->driver, path, callback, callbackData);

        if (!result && param.errorOnMissing)
            THROW_FMT(PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, strPtr(path));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageList(const Storage *this, const String *pathExp, StorageListParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, param.nullOnMissing);
        FUNCTION_LOG_PARAM(STRING, param.expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(!param.errorOnMissing || !param.nullOnMissing);
    ASSERT(!param.errorOnMissing || storageFeature(this, storageFeaturePath));

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Get the list
        result = this->interface.list(this->driver, path, param.expression);

        // If the path does not exist
        if (result == NULL)
        {
            // Error if requested
            if (param.errorOnMissing)
                THROW_FMT(PathMissingError, STORAGE_ERROR_LIST_MISSING, strPtr(path));

            // Build an empty list if the directory does not exist by default.  This makes the logic in calling functions simpler
            // when they don't care if the path is missing.
            if (!param.nullOnMissing)
                result = strLstNew();
        }

        // Move list up to the old context
        result = strLstMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Move a file
***********************************************************************************************************************************/
void
storageMove(const Storage *this, StorageRead *source, StorageWrite *destination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->interface.move != NULL);
    ASSERT(source != NULL);
    ASSERT(destination != NULL);
    ASSERT(!storageReadIgnoreMissing(source));
    ASSERT(strEq(this->type, storageReadType(source)));
    ASSERT(strEq(storageReadType(source), storageWriteType(destination)));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file can't be moved it will need to be copied
        if (!this->interface.move(this->driver, source, destination))
        {
            // Perform the copy
            storageCopyNP(source, destination);

            // Remove the source file
            this->interface.remove(this->driver, storageReadName(source), false);

            // Sync source path if the destination path was synced.  We know the source and destination paths are different because
            // the move did not succeed.  This will need updating when drivers other than Posix/CIFS are implemented becaue there's
            // no way to get coverage on it now.
            if (storageWriteSyncPath(destination))
                this->interface.pathSync(this->driver, strPath(storageReadName(source)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Open a file for reading
***********************************************************************************************************************************/
StorageRead *
storageNewRead(const Storage *this, const String *fileExp, StorageNewReadParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(BOOL, param.ignoreMissing);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, param.filterGroup);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = this->interface.newRead(this->driver, storagePathNP(this, fileExp), param.ignoreMissing);

        if (param.filterGroup != NULL)
            ioReadFilterGroupSet(storageReadIo(result), param.filterGroup);

        storageReadMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, result);
}

/***********************************************************************************************************************************
Open a file for writing
***********************************************************************************************************************************/
StorageWrite *
storageNewWrite(const Storage *this, const String *fileExp, StorageNewWriteParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.group);
        FUNCTION_LOG_PARAM(INT64, param.timeModified);
        FUNCTION_LOG_PARAM(BOOL, param.noCreatePath);
        FUNCTION_LOG_PARAM(BOOL, param.noSyncFile);
        FUNCTION_LOG_PARAM(BOOL, param.noSyncPath);
        FUNCTION_LOG_PARAM(BOOL, param.noAtomic);
        FUNCTION_LOG_PARAM(IO_FILTER_GROUP, param.filterGroup);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);

    StorageWrite *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = this->interface.newWrite(
            this->driver, storagePathNP(this, fileExp), param.modeFile != 0 ? param.modeFile : this->modeFile,
            param.modePath != 0 ? param.modePath : this->modePath, param.user, param.group, param.timeModified, !param.noCreatePath,
            !param.noSyncFile, !param.noSyncPath, !param.noAtomic);

        if (param.filterGroup != NULL)
            ioWriteFilterGroupSet(storageWriteIo(result), param.filterGroup);

        storageWriteMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, result);
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
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    String *result = NULL;

    // If there is no path expression then return the base storage path
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
            if (this->path != NULL && !strEqZ(this->path, "/"))
            {
                if (this->pathEnforce && (!strBeginsWith(pathExp, this->path) ||
                    !(strSize(pathExp) == strSize(this->path) || *(strPtr(pathExp) + strSize(this->path)) == '/')))
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
                const String *path = NULL;

                if (strSize(expression) < strSize(pathExp))
                {
                    // Error if path separator is not found
                    if (end[1] != '/')
                        THROW_FMT(AssertError, "'/' should separate expression and path '%s'", strPtr(pathExp));

                    // Only create path if there is something after the path separator
                    if (end[2] == 0)
                        THROW_FMT(AssertError, "path '%s' should not end in '/'", strPtr(pathExp));

                    path = STR(end + 2);
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
            }

            if (this->path == NULL)
                result = strDup(pathExp);
            else if (strEqZ(this->path, "/"))
                result = strNewFmt("/%s", strPtr(pathExp));
            else
                result = strNewFmt("%s/%s", strPtr(this->path), strPtr(pathExp));

            strFree(pathEvaluated);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, param.noParentCreate);
        FUNCTION_LOG_PARAM(MODE, param.mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->interface.pathCreate != NULL && storageFeature(this, storageFeaturePath));
    ASSERT(this->write);

    // It doesn't make sense to combine these parameters because if we are creating missing parent paths why error when they exist?
    // If this somehow wasn't caught in testing, the worst case is that the path would not be created and an error would be thrown.
    ASSERT(!(!param.noParentCreate && param.errorOnExists));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        this->interface.pathCreate(
            this->driver, path, param.errorOnExists, param.noParentCreate, param.mode != 0 ? param.mode : this->modePath);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Does a path exist?
***********************************************************************************************************************************/
bool
storagePathExists(const Storage *this, const String *pathExp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->interface.pathExists != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = this->interface.pathExists(this->driver, storagePathNP(this, pathExp));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
void
storagePathRemove(const Storage *this, const String *pathExp, StoragePathRemoveParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, param.recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);
    ASSERT(!param.errorOnMissing || storageFeature(this, storageFeaturePath));
    ASSERT(param.recurse || storageFeature(this, storageFeaturePath));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Call driver function
        if (!this->interface.pathRemove(this->driver, path, param.recurse) && param.errorOnMissing)
            THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_MISSING, strPtr(path));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
void storagePathSync(const Storage *this, const String *pathExp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);

    // Not all storage requires path sync so just do nothing if the function is not implemented
    if (this->interface.pathSync != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            this->interface.pathSync(this->driver, storagePathNP(this, pathExp));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Write a buffer to storage
***********************************************************************************************************************************/
void
storagePut(StorageWrite *file, const Buffer *buffer)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, file);
        FUNCTION_LOG_PARAM(BUFFER, buffer);
    FUNCTION_LOG_END();

    ASSERT(file != NULL);

    ioWriteOpen(storageWriteIo(file));
    ioWrite(storageWriteIo(file), buffer);
    ioWriteClose(storageWriteIo(file));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, fileExp);

        // Call driver function
        this->interface.remove(this->driver, file, param.errorOnMissing);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the storage driver
***********************************************************************************************************************************/
void *
storageDriver(const Storage *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->driver);
}

/***********************************************************************************************************************************
Is the feature supported by this storage?
***********************************************************************************************************************************/
bool
storageFeature(const Storage *this, StorageFeature feature)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
        FUNCTION_TEST_PARAM(ENUM, feature);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface.feature >> feature & 1);
}

/***********************************************************************************************************************************
Get the storage interface
***********************************************************************************************************************************/
StorageInterface
storageInterface(const Storage *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->interface);
}

/***********************************************************************************************************************************
Set whether absolute paths are required to be in the base path
***********************************************************************************************************************************/
void
storagePathEnforceSet(Storage *this, bool enforce)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
        FUNCTION_TEST_PARAM(BOOL, enforce);
    FUNCTION_TEST_END();

    this->pathEnforce = enforce;

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the storage type (posix, cifs, etc.)
***********************************************************************************************************************************/
const String *
storageType(const Storage *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->type);
}

/***********************************************************************************************************************************
Render as string for logging
***********************************************************************************************************************************/
String *
storageToLog(const Storage *this)
{
    return strNewFmt(
        "{type: %s, path: %s, write: %s}", strPtr(this->type), strPtr(strToLog(this->path)), cvtBoolToConstZ(this->write));
}
