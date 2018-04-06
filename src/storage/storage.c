/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#include <string.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/driver/posix.h"
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
Get storage buffer size
***********************************************************************************************************************************/
size_t
storageBufferSize(const Storage *this)
{
    return this->bufferSize;
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

        // Loop until file exists or timeout
        do
        {
            // Call driver function
            result = storageDriverPosixExists(path);
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
    Buffer *result = NULL;

    // Call driver function if a file was passed
    if (file != NULL)
        result = storageDriverPosixGet(file);

    return result;
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageList(const Storage *this, const String *pathExp, StorageListParam param)
{
    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathNP(this, pathExp);

        // Move list up to the old context
        result = strLstMove(storageDriverPosixList(path, param.errorOnMissing, param.expression), MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    return result;
}


/***********************************************************************************************************************************
Open a file for reading
***********************************************************************************************************************************/
StorageFile *
storageOpenRead(const Storage *this, const String *fileExp, StorageOpenReadParam param)
{
    StorageFile *result = NULL;

    MEM_CONTEXT_NEW_BEGIN("StorageFile")
    {
        String *fileName = storagePathNP(this, fileExp);

        // Call driver function
        void *data = storageDriverPosixOpenRead(fileName, param.ignoreMissing);

        // Free mem contexts if missing files are ignored
        if (data != NULL)
            result = storageFileNew(this, fileName, storageFileTypeRead, data);
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

    MEM_CONTEXT_NEW_BEGIN("StorageFile")
    {
        String *fileName = storagePathNP(this, fileExp);

        // Call driver function
        result = storageFileNew(
            this, fileName, storageFileTypeWrite,
            storageDriverPosixOpenWrite(fileName, param.mode != 0 ? param.mode : this->modeFile));
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

        // Call driver function
        storageDriverPosixPathCreate(
            path, param.errorOnExists, param.noParentCreate, param.mode != 0 ? param.mode : this->modePath);
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
        storageDriverPosixPut(file, buffer);
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
void
storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param)
{
    ASSERT_STORAGE_ALLOWS_WRITE();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathNP(this, fileExp);

        // Call driver function
        storageDriverPosixRemove(file, param.errorOnMissing);
    }
    MEM_CONTEXT_TEMP_END();
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
