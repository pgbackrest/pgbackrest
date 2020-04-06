/***********************************************************************************************************************************
Posix Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/user.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_POSIX_TYPE_STR,                               STORAGE_POSIX_TYPE);

/***********************************************************************************************************************************
Define PATH_MAX if it is not defined
***********************************************************************************************************************************/
#ifndef PATH_MAX
    #define PATH_MAX                                                (4 * 1024)
#endif

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StoragePosix
{
    STORAGE_COMMON_MEMBER;
    MemContext *memContext;                                         // Object memory context
};

/**********************************************************************************************************************************/
static StorageInfo
storagePosixInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(BOOL, param.followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {.level = level};

    // Stat the file to check if it exists
    struct stat statFile;

    if ((param.followLink ? stat(strPtr(file), &statFile) : lstat(strPtr(file), &statFile)) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO, strPtr(file));
    }
    // On success the file exists
    else
    {
        result.exists = true;

        // Add basic level info
        if (result.level >= storageInfoLevelBasic)
        {
            result.timeModified = statFile.st_mtime;

            if (S_ISREG(statFile.st_mode))
            {
                result.type = storageTypeFile;
                result.size = (uint64_t)statFile.st_size;
            }
            else if (S_ISDIR(statFile.st_mode))
                result.type = storageTypePath;
            else if (S_ISLNK(statFile.st_mode))
                result.type = storageTypeLink;
            else
                result.type = storageTypeSpecial;
        }

        // Add detail level info
        if (result.level >= storageInfoLevelDetail)
        {
            result.groupId = statFile.st_gid;
            result.group = groupNameFromId(result.groupId);
            result.userId = statFile.st_uid;
            result.user = userNameFromId(result.userId);
            result.mode = statFile.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);

            if (result.type == storageTypeLink)
            {
                char linkDestination[PATH_MAX];
                ssize_t linkDestinationSize = 0;

                THROW_ON_SYS_ERROR_FMT(
                    (linkDestinationSize = readlink(strPtr(file), linkDestination, sizeof(linkDestination) - 1)) == -1,
                    FileReadError, "unable to get destination for link '%s'", strPtr(file));

                result.linkDestination = strNewN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
// Helper function to get info for a file if it exists.  This logic can't live directly in storagePosixInfoList() because there is
// a race condition where a file might exist while listing the directory but it is gone before stat() can be called.  In order to
// get complete test coverage this function must be split out.
static void
storagePosixInfoListEntry(
    StoragePosix *this, const String *path, const String *name, StorageInfoLevel level, StorageInfoListCallback callback,
    void *callbackData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_POSIX, this);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(ENUM, level);
        FUNCTION_TEST_PARAM(FUNCTIONP, callback);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);
    ASSERT(callback != NULL);

    StorageInfo storageInfo = storageInterfaceInfoP(
        this, strEq(name, DOT_STR) ? strDup(path) : strNewFmt("%s/%s", strPtr(path), strPtr(name)), level);

    if (storageInfo.exists)
    {
        storageInfo.name = name;
        callback(callbackData, &storageInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storagePosixInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    bool result = false;

    // Open the directory for read
    DIR *dir = opendir(strPtr(path));

    // If the directory could not be opened process errors and report missing directories
    if (dir == NULL)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_LIST_INFO, strPtr(path));
    }
    else
    {
        // Directory was found
        result = true;

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                // Read the directory entries
                struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    const String *name = STR(dirEntry->d_name);

                    // Always skip ..
                    if (!strEq(name, DOTDOT_STR))
                    {
                        // If only making a list of files that exist then no need to go get detailed info which requires calling
                        // stat() and is therefore relatively slow
                        if (level == storageInfoLevelExists)
                        {
                            callback(callbackData, &(StorageInfo){.name = name, .level = storageInfoLevelExists, .exists = true});
                        }
                        // Else more info is required which requires a call to stat()
                        else
                            storagePosixInfoListEntry(this, path, name, level, callback, callbackData);
                    }

                    // Get next entry
                    dirEntry = readdir(dir);

                    // Reset the memory context occasionally so we don't use too much memory or slow down processing
                    MEM_CONTEXT_TEMP_RESET(1000);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        FINALLY()
        {
            closedir(dir);
        }
        TRY_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static bool
storagePosixMove(THIS_VOID, StorageRead *source, StorageWrite *destination, StorageInterfaceMoveParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(source != NULL);
    ASSERT(destination != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *sourceFile = storageReadName(source);
        const String *destinationFile = storageWriteName(destination);
        const String *destinationPath = strPath(destinationFile);

        // Attempt to move the file
        if (rename(strPtr(sourceFile), strPtr(destinationFile)) == -1)
        {
            // Determine which file/path is missing
            if (errno == ENOENT)
            {
                // Check if the source is missing. Rename does not follow links so there is no need to set followLink.
                if (!storageInterfaceInfoP(this, sourceFile, storageInfoLevelExists).exists)
                    THROW_SYS_ERROR_FMT(FileMissingError, "unable to move missing source '%s'", strPtr(sourceFile));

                if (!storageWriteCreatePath(destination))
                {
                    THROW_SYS_ERROR_FMT(
                        PathMissingError, "unable to move '%s' to missing path '%s'", strPtr(sourceFile), strPtr(destinationPath));
                }

                storageInterfacePathCreateP(this, destinationPath, false, false, storageWriteModePath(destination));
                result = storageInterfaceMoveP(this, source, destination);
            }
            // Else the destination is on a different device so a copy will be needed
            else if (errno == EXDEV)
            {
                result = false;
            }
            else
                THROW_SYS_ERROR_FMT(FileMoveError, "unable to move '%s' to '%s'", strPtr(sourceFile), strPtr(destinationFile));
        }
        // Sync paths on success
        else
        {
            // Sync source path if the destination path was synced and the paths are not equal
            if (storageWriteSyncPath(destination))
            {
                String *sourcePath = strPath(sourceFile);

                if (!strEq(destinationPath, sourcePath))
                    storageInterfacePathSyncP(this, sourcePath);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
static StorageRead *
storagePosixNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadPosixNew(this, file, ignoreMissing, param.limit));
}

/**********************************************************************************************************************************/
static StorageWrite *
storagePosixNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(STRING, param.user);
        FUNCTION_LOG_PARAM(STRING, param.group);
        FUNCTION_LOG_PARAM(TIME, param.timeModified);
        FUNCTION_LOG_PARAM(BOOL, param.createPath);
        FUNCTION_LOG_PARAM(BOOL, param.syncFile);
        FUNCTION_LOG_PARAM(BOOL, param.syncPath);
        FUNCTION_LOG_PARAM(BOOL, param.atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWritePosixNew(
            this, file, param.modeFile, param.modePath, param.user, param.group, param.timeModified, param.createPath,
            param.syncFile, this->interface.pathSync != NULL ? param.syncPath : false, param.atomic));
}

/**********************************************************************************************************************************/
void
storagePosixPathCreate(
    THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode, StorageInterfacePathCreateParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // Attempt to create the directory
    if (mkdir(strPtr(path), mode) == -1)
    {
        // If the parent path does not exist then create it if allowed
        if (errno == ENOENT && !noParentCreate)
        {
            storageInterfacePathCreateP(this, strPath(path), errorOnExists, noParentCreate, mode);
            storageInterfacePathCreateP(this, path, errorOnExists, noParentCreate, mode);
        }
        // Ignore path exists if allowed
        else if (errno != EEXIST || errorOnExists)
            THROW_SYS_ERROR_FMT(PathCreateError, "unable to create path '%s'", strPtr(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct StoragePosixPathRemoveData
{
    StoragePosix *driver;                                           // Driver
    const String *path;                                             // Path
} StoragePosixPathRemoveData;

static void
storagePosixPathRemoveCallback(void *callbackData, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM_P(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(info != NULL);

    if (!strEqZ(info->name, "."))
    {
        StoragePosixPathRemoveData *data = callbackData;

        String *file = strNewFmt("%s/%s", strPtr(data->path), strPtr(info->name));

        // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
        if (unlink(strPtr(file)) == -1)
        {
            // These errors indicate that the entry is actually a path so we'll try to delete it that way
            if (errno == EPERM || errno == EISDIR)              // {uncovered_branch - no EPERM on tested systems}
            {
                storageInterfacePathRemoveP(data->driver, file, true);
            }
            // Else error
            else
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strPtr(file));
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storagePosixPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            StoragePosixPathRemoveData data =
            {
                .driver = this,
                .path = path,
            };

            // Remove all sub paths/files
            storageInterfaceInfoListP(this, path, storageInfoLevelExists, storagePosixPathRemoveCallback, &data);
        }

        // Delete the path
        if (rmdir(strPtr(path)) == -1)
        {
            if (errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strPtr(path));

            // Path does not exist
            result = false;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
void
storagePosixPathSync(THIS_VOID, const String *path, StorageInterfacePathSyncParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // Open directory and handle errors
    int handle = open(strPtr(path), O_RDONLY, 0);

    // Handle errors
    if (handle == -1)
    {
        if (errno == ENOENT)
            THROW_FMT(PathMissingError, STORAGE_ERROR_PATH_SYNC_MISSING, strPtr(path));
        else
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_PATH_SYNC_OPEN, strPtr(path));
    }
    else
    {
        // Attempt to sync the directory
        if (fsync(handle) == -1)
        {
            int errNo = errno;

            // Close the handle to free resources but don't check for failure
            close(handle);

            THROW_SYS_ERROR_CODE_FMT(errNo, PathSyncError, STORAGE_ERROR_PATH_SYNC, strPtr(path));
        }

        THROW_ON_SYS_ERROR_FMT(close(handle) == -1, PathCloseError, STORAGE_ERROR_PATH_SYNC_CLOSE, strPtr(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static void
storagePosixRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, param.errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to unlink the file
    if (unlink(strPtr(file)) == -1)
    {
        if (param.errorOnMissing || errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strPtr(file));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
static const StorageInterface storageInterfacePosix =
{
    .feature = 1 << storageFeaturePath | 1 << storageFeatureCompress | 1 << storageFeatureLimitRead,

    .info = storagePosixInfo,
    .infoList = storagePosixInfoList,
    .move = storagePosixMove,
    .newRead = storagePosixNewRead,
    .newWrite = storagePosixNewWrite,
    .pathCreate = storagePosixPathCreate,
    .pathRemove = storagePosixPathRemove,
    .pathSync = storagePosixPathSync,
    .remove = storagePosixRemove,
};

Storage *
storagePosixNewInternal(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, pathSync);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Initialze user module
    userInit();

    // Create the object
    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StoragePosix")
    {
        StoragePosix *driver = memNew(sizeof(StoragePosix));

        *driver = (StoragePosix)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .interface = storageInterfacePosix,
        };

        // Disable path sync when not supported
        if (!pathSync)
            driver->interface.pathSync = NULL;

        // If this is a posix driver then add link features
        if (strEq(type, STORAGE_POSIX_TYPE_STR))
            driver->interface.feature |=
                1 << storageFeatureHardLink | 1 << storageFeatureSymLink | 1 << storageFeaturePathSync |
                1 << storageFeatureInfoDetail;

        this = storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}

Storage *
storagePosixNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE, storagePosixNewInternal(STORAGE_POSIX_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, true));
}
