/***********************************************************************************************************************************
Storage Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdio.h>
#include <string.h>

#include "common/debug.h"
#include "common/io/io.h"
#include "common/type/list.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Storage
{
    StoragePub pub;                                                 // Publicly accessible variables
    MemContext *memContext;
    const String *path;
    mode_t modeFile;
    mode_t modePath;
    bool write;
    StoragePathExpressionCallback *pathExpressionFunction;
};

/**********************************************************************************************************************************/
Storage *
storageNew(
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, void *driver, StorageInterface interface)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(STORAGE_INTERFACE, interface);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(strSize(path) >= 1 && strZ(path)[0] == '/');
    ASSERT(driver != NULL);
    ASSERT(interface.info != NULL);
    ASSERT(interface.list != NULL);
    ASSERT(interface.newRead != NULL);
    ASSERT(interface.newWrite != NULL);
    ASSERT(interface.pathRemove != NULL);
    ASSERT(interface.remove != NULL);

    Storage *this = (Storage *)memNew(sizeof(Storage));

    *this = (Storage)
    {
        .pub =
        {
            .type = type,
            .driver = driver,
            .interface = interface,
        },
        .memContext = memContextCurrent(),
        .path = strDup(path),
        .modeFile = modeFile,
        .modePath = modePath,
        .write = write,
        .pathExpressionFunction = pathExpressionFunction,
    };

    // If path sync feature is enabled then path feature must be enabled
    CHECK(
        AssertError, !storageFeature(this, storageFeaturePathSync) || storageFeature(this, storageFeaturePath),
        "path feature required");

    // If hardlink feature is enabled then path feature must be enabled
    CHECK(
        AssertError, !storageFeature(this, storageFeatureHardLink) || storageFeature(this, storageFeaturePath),
        "path feature required");

    // If symlink feature is enabled then path feature must be enabled
    CHECK(
        AssertError, !storageFeature(this, storageFeatureSymLink) || storageFeature(this, storageFeaturePath),
        "path feature required");

    // If link features are enabled then linkCreate must be implemented
    CHECK(
        AssertError,
        (!storageFeature(this, storageFeatureSymLink) && !storageFeature(this, storageFeatureHardLink)) ||
            interface.linkCreate != NULL,
        "linkCreate required");

    FUNCTION_LOG_RETURN(STORAGE, this);
}

/**********************************************************************************************************************************/
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
            ioCopyP(storageReadIo(source), storageWriteIo(destination));

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

/**********************************************************************************************************************************/
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
        Wait *wait = waitNew(param.timeout);

        // Loop until file exists or timeout
        do
        {
            // storageInfoLevelBasic is required here because storageInfoLevelExists will not return the type and this function
            // specifically wants to test existence of a *file*, not just the existence of anything with the specified name.
            StorageInfo info = storageInfoP(
                this, pathExp, .level = storageInfoLevelBasic, .ignoreMissing = true, .followLink = true);

            // Only exists if it is a file
            result = info.exists && info.type == storageTypeFile;
        }
        while (!result && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
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
                    THROW_FMT(FileReadError, "unable to read %zu byte(s) from '%s'", param.exactSize, strZ(storageReadName(file)));
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
            bufMove(result, memContextPrior());
        }
        MEM_CONTEXT_TEMP_END();

        ioReadClose(storageReadIo(file));
    }

    FUNCTION_LOG_RETURN(BUFFER, result);
}

/**********************************************************************************************************************************/
StorageInfo
storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(ENUM, param.level);
        FUNCTION_LOG_PARAM(BOOL, param.ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
        FUNCTION_LOG_PARAM(BOOL, param.noPathEnforce);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.interface.info != NULL);

    StorageInfo result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *file = storagePathP(this, fileExp, .noEnforce = param.noPathEnforce);

        // Call driver function
        if (param.level == storageInfoLevelDefault)
            param.level = storageFeature(this, storageFeatureInfoDetail) ? storageInfoLevelDetail : storageInfoLevelBasic;

        // If file is / then this is definitely a path so skip the call for drivers that do not support paths and do not provide
        // additional info to return. Also, some object stores (e.g. S3) behave strangely when getting info for /.
        if (strEq(file, FSLASH_STR) && !storageFeature(this, storageFeaturePath))
        {
            result = (StorageInfo){.level = param.level};
        }
        // Else call the driver
        else
            result = storageInterfaceInfoP(storageDriver(this), file, param.level, .followLink = param.followLink);

        // Error if the file missing and not ignoring
        if (!result.exists && !param.ignoreMissing)
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO_MISSING, strZ(file));

        // Dup the strings into the prior context
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result.linkDestination = strDup(result.linkDestination);
            result.user = strDup(result.user);
            result.group = strDup(result.group);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
StorageIterator *
storageNewItr(const Storage *const this, const String *const pathExp, StorageNewItrParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
        FUNCTION_LOG_PARAM(ENUM, param.level);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, param.recurse);
        FUNCTION_LOG_PARAM(BOOL, param.nullOnMissing);
        FUNCTION_LOG_PARAM(ENUM, param.sortOrder);
        FUNCTION_LOG_PARAM(STRING, param.expression);
        FUNCTION_LOG_PARAM(BOOL, param.recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.interface.list != NULL);
    ASSERT(!param.errorOnMissing || storageFeature(this, storageFeaturePath));

    StorageIterator *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Info level
        if (param.level == storageInfoLevelDefault)
            param.level = storageFeature(this, storageFeatureInfoDetail) ? storageInfoLevelDetail : storageInfoLevelBasic;

        result = storageItrMove(
            storageItrNew(
                storageDriver(this), storagePathP(this, pathExp), param.level, param.errorOnMissing, param.nullOnMissing,
                param.recurse, param.sortOrder, param.expression),
            memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_ITERATOR, result);
}

/**********************************************************************************************************************************/
void storageLinkCreate(
    const Storage *const this, const String *const target, const String *const linkPath, const StorageLinkCreateParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, target);
        FUNCTION_LOG_PARAM(STRING, linkPath);
        FUNCTION_LOG_PARAM(ENUM, param.linkType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);
    ASSERT(target != NULL);
    ASSERT(linkPath != NULL);
    ASSERT(this->pub.interface.linkCreate != NULL);
    ASSERT(
        (param.linkType == storageLinkSym && storageFeature(this, storageFeatureSymLink)) ||
        (param.linkType == storageLinkHard && storageFeature(this, storageFeatureHardLink)));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storageInterfaceLinkCreateP(storageDriver(this), target, linkPath, .linkType = param.linkType);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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

    StringList *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        StorageIterator *const storageItr = storageNewItrP(
            this, pathExp, .errorOnMissing = param.errorOnMissing, .nullOnMissing = param.nullOnMissing,
            .expression = param.expression);

        if (storageItr != NULL)
        {
            result = strLstNew();

            while (storageItrMore(storageItr))
                strLstAdd(result, storageItrNext(storageItr).name);

            strLstMove(result, memContextPrior());
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/**********************************************************************************************************************************/
void
storageMove(const Storage *this, StorageRead *source, StorageWrite *destination)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.interface.move != NULL);
    ASSERT(source != NULL);
    ASSERT(destination != NULL);
    ASSERT(!storageReadIgnoreMissing(source));
    ASSERT(storageType(this) == storageReadType(source));
    ASSERT(storageReadType(source) == storageWriteType(destination));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If the file can't be moved it will need to be copied
        if (!storageInterfaceMoveP(storageDriver(this), source, destination))
        {
            // Perform the copy
            storageCopyP(source, destination);

            // Remove the source file
            storageInterfaceRemoveP(storageDriver(this), storageReadName(source));

            // Sync source path if the destination path was synced.  We know the source and destination paths are different because
            // the move did not succeed.  This will need updating when drivers other than Posix/CIFS are implemented because there's
            // no way to get coverage on it now.
            if (storageWriteSyncPath(destination))
                storageInterfacePathSyncP(storageDriver(this), strPath(storageReadName(source)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageRead *
storageNewRead(const Storage *this, const String *fileExp, StorageNewReadParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, fileExp);
        FUNCTION_LOG_PARAM(BOOL, param.ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(param.limit == NULL || varType(param.limit) == varTypeUInt64);

    StorageRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = storageReadMove(
            storageInterfaceNewReadP(
                storageDriver(this), storagePathP(this, fileExp), param.ignoreMissing, .compressible = param.compressible,
                .offset = param.offset, .limit = param.limit),
            memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_READ, result);
}

/**********************************************************************************************************************************/
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
        FUNCTION_LOG_PARAM(BOOL, param.noTruncate);
        FUNCTION_LOG_PARAM(BOOL, param.compressible);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);
    // noTruncate does not work with atomic writes because a new file is always created for atomic writes
    ASSERT(!param.noTruncate || param.noAtomic);

    StorageWrite *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = storageWriteMove(
            storageInterfaceNewWriteP(
                storageDriver(this), storagePathP(this, fileExp), .modeFile = param.modeFile != 0 ? param.modeFile : this->modeFile,
                .modePath = param.modePath != 0 ? param.modePath : this->modePath, .user = param.user, .group = param.group,
                .timeModified = param.timeModified, .createPath = !param.noCreatePath, .syncFile = !param.noSyncFile,
                .syncPath = !param.noSyncPath, .atomic = !param.noAtomic, .truncate = !param.noTruncate,
                .compressible = param.compressible),
            memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_WRITE, result);
}

/**********************************************************************************************************************************/
String *
storagePath(const Storage *this, const String *pathExp, StoragePathParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE, this);
        FUNCTION_TEST_PARAM(STRING, pathExp);
        FUNCTION_TEST_PARAM(BOOL, param.noEnforce);
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
        if ((strZ(pathExp))[0] == '/')
        {
            // Make sure the base storage path is contained within the path expression
            if (!strEqZ(this->path, "/"))
            {
                if (!param.noEnforce && (!strBeginsWith(pathExp, this->path) ||
                    !(strSize(pathExp) == strSize(this->path) || *(strZ(pathExp) + strSize(this->path)) == '/')))
                {
                    THROW_FMT(AssertError, "absolute path '%s' is not in base path '%s'", strZ(pathExp), strZ(this->path));
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
            if ((strZ(pathExp))[0] == '<')
            {
                if (this->pathExpressionFunction == NULL)
                    THROW_FMT(AssertError, "expression '%s' not valid without callback function", strZ(pathExp));

                // Get position of the expression end
                char *end = strchr(strZ(pathExp), '>');

                // Error if end is not found
                if (end == NULL)
                    THROW_FMT(AssertError, "end > not found in path expression '%s'", strZ(pathExp));

                // Create a string from the expression
                String *expression = strNewZN(strZ(pathExp), (size_t)(end - strZ(pathExp) + 1));

                // Create a string from the path if there is anything left after the expression
                String *path = NULL;

                if (strSize(expression) < strSize(pathExp))
                {
                    // Error if path separator is not found
                    if (end[1] != '/')
                        THROW_FMT(AssertError, "'/' should separate expression and path '%s'", strZ(pathExp));

                    // Only create path if there is something after the path separator
                    if (end[2] == 0)
                        THROW_FMT(AssertError, "path '%s' should not end in '/'", strZ(pathExp));

                    path = strNewZ(end + 2);
                }

                // Evaluate the path
                pathEvaluated = this->pathExpressionFunction(expression, path);

                // Evaluated path cannot be NULL
                if (pathEvaluated == NULL)
                    THROW_FMT(AssertError, "evaluated path '%s' cannot be null", strZ(pathExp));

                // Assign evaluated path to path
                pathExp = pathEvaluated;

                // Free temp vars
                strFree(expression);
                strFree(path);
            }

            if (strEqZ(this->path, "/"))
                result = strNewFmt("/%s", strZ(pathExp));
            else
                result = strNewFmt("%s/%s", strZ(this->path), strZ(pathExp));

            strFree(pathEvaluated);
        }
    }

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
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
    ASSERT(this->pub.interface.pathCreate != NULL && storageFeature(this, storageFeaturePath));
    ASSERT(this->write);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the path
        String *path = storagePathP(this, pathExp);

        // Call driver function
        storageInterfacePathCreateP(
            storageDriver(this), path, param.errorOnExists, param.noParentCreate, param.mode != 0 ? param.mode : this->modePath);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
storagePathExists(const Storage *this, const String *pathExp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storageFeature(this, storageFeaturePath));

    // storageInfoLevelBasic is required here because storageInfoLevelExists will not return the type and this function specifically
    // wants to test existence of a *path*, not just the existence of anything with the specified name.
    StorageInfo info = storageInfoP(this, pathExp, .level = storageInfoLevelBasic, .ignoreMissing = true, .followLink = true);

    FUNCTION_LOG_RETURN(BOOL, info.exists && info.type == storageTypePath);
}

/**********************************************************************************************************************************/
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
        String *path = storagePathP(this, pathExp);

        // Call driver function
        if (!storageInterfacePathRemoveP(storageDriver(this), path, param.recurse) && param.errorOnMissing)
        {
            THROW_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_MISSING, strZ(path));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void storagePathSync(const Storage *this, const String *pathExp)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, this);
        FUNCTION_LOG_PARAM(STRING, pathExp);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(this->write);

    // Not all storage requires path sync so just do nothing if the function is not implemented
    if (this->pub.interface.pathSync != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            storageInterfacePathSyncP(storageDriver(this), storagePathP(this, pathExp));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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

/**********************************************************************************************************************************/
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
        String *file = storagePathP(this, fileExp);

        // Call driver function
        storageInterfaceRemoveP(storageDriver(this), file, .errorOnMissing = param.errorOnMissing);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
String *
storageToLog(const Storage *this)
{
    return strNewFmt(
        "{type: %s, path: %s, write: %s}", strZ(strIdToStr(storageType(this))), strZ(strToLog(this->path)),
        cvtBoolToConstZ(this->write));
}
