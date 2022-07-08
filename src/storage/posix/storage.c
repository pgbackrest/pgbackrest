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
#include "common/regExp.h"
#include "common/user.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"

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

    if ((param.followLink ? stat(strZ(file), &statFile) : lstat(strZ(file), &statFile)) == -1)
    {
        if (errno != ENOENT)                                                                                        // {vm_covered}
            THROW_SYS_ERROR_FMT(FileOpenError, STORAGE_ERROR_INFO, strZ(file));                                     // {vm_covered}
    }
    // On success the file exists
    else
    {
        result.exists = true;

        // Add type info (no need set file type since it is the default)
        if (result.level >= storageInfoLevelType && !S_ISREG(statFile.st_mode))
        {
            if (S_ISDIR(statFile.st_mode))
                result.type = storageTypePath;
            else if (S_ISLNK(statFile.st_mode))
                result.type = storageTypeLink;
            else
                result.type = storageTypeSpecial;
        }

        // Add basic level info
        if (result.level >= storageInfoLevelBasic)
        {
            result.timeModified = statFile.st_mtime;

            if (result.type == storageTypeFile)
                result.size = (uint64_t)statFile.st_size;
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
                    (linkDestinationSize = readlink(strZ(file), linkDestination, sizeof(linkDestination) - 1)) == -1,
                    FileReadError, "unable to get destination for link '%s'", strZ(file));

                result.linkDestination = strNewZN(linkDestination, (size_t)linkDestinationSize);
            }
        }
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
// Helper function to get info for a file if it exists. This logic can't live directly in storagePosixList() because there is a race
// condition where a file might exist while listing the directory but it is gone before stat() can be called. In order to get
// complete test coverage this function must be split out.
static void
storagePosixListEntry(
    StoragePosix *const this, StorageList *const list, const String *const path, const char *const name,
    const StorageInfoLevel level)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_POSIX, this);
        FUNCTION_TEST_PARAM(STORAGE_LIST, list);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRINGZ, name);
        FUNCTION_TEST_PARAM(ENUM, level);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(list != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);

    StorageInfo info = storageInterfaceInfoP(this, strNewFmt("%s/%s", strZ(path), name), level);

    if (info.exists)
    {
        info.name = STR(name);
        storageLstAdd(list, &info);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static StorageList *
storagePosixList(THIS_VOID, const String *const path, const StorageInfoLevel level, const StorageInterfaceListParam param)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        (void)param;                                                // No parameters are used
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StorageList *result = NULL;

    // Open the directory for read
    DIR *const dir = opendir(strZ(path));

    // If the directory could not be opened process errors and report missing directories
    if (dir == NULL)
    {
        if (errno != ENOENT)                                                                                        // {vm_covered}
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_LIST_INFO, strZ(path));                                // {vm_covered}
    }
    // Directory was found
    else
    {
        result = storageLstNew(level);

        TRY_BEGIN()
        {
            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                // Read the directory entries
                const struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    // Always skip . and ..
                    if (!strEqZ(DOT_STR, dirEntry->d_name) && !strEqZ(DOTDOT_STR, dirEntry->d_name))
                    {
                        // If only making a list of files that exist then no need to go get detailed info which requires calling
                        // stat() and is therefore relatively slow
                        if (level == storageInfoLevelExists)
                        {
                            storageLstAdd(
                                result,
                                &(StorageInfo)
                                {
                                    .name = STR(dirEntry->d_name),
                                    .level = storageInfoLevelExists,
                                    .exists = true,
                                });
                        }
                        // Else more info is required which requires a call to stat()
                        else
                            storagePosixListEntry(this, result, path, dirEntry->d_name, level);
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

    FUNCTION_LOG_RETURN(STORAGE_LIST, result);
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
        if (rename(strZ(sourceFile), strZ(destinationFile)) == -1)
        {
            // Determine which file/path is missing
            if (errno == ENOENT)
            {
                // Check if the source is missing. Rename does not follow links so there is no need to set followLink.
                if (!storageInterfaceInfoP(this, sourceFile, storageInfoLevelExists).exists)
                    THROW_SYS_ERROR_FMT(FileMissingError, "unable to move missing source '%s'", strZ(sourceFile));

                if (!storageWriteCreatePath(destination))
                {
                    THROW_SYS_ERROR_FMT(
                        PathMissingError, "unable to move '%s' to missing path '%s'", strZ(sourceFile), strZ(destinationPath));
                }

                storageInterfacePathCreateP(this, destinationPath, false, false, storageWriteModePath(destination));
                result = storageInterfaceMoveP(this, source, destination);
            }
            // Else the destination is on a different device so a copy will be needed
            else if (errno == EXDEV)                                                                                // {vm_covered}
            {
                result = false;
            }
            else
            {
                THROW_SYS_ERROR_FMT(                                                                                // {vm_covered}
                    FileMoveError, "unable to move '%s' to '%s'", strZ(sourceFile), strZ(destinationFile));         // {vm_covered}
            }
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
        FUNCTION_LOG_PARAM(UINT64, param.offset);
        FUNCTION_LOG_PARAM(VARIANT, param.limit);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadPosixNew(this, file, ignoreMissing, param.offset, param.limit));
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
    THIS_VOID, const String *const path, const bool errorOnExists, const bool noParentCreate, const mode_t mode,
    const StorageInterfacePathCreateParam param)
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
    if (mkdir(strZ(path), mode) == -1)
    {
        // If the parent path does not exist then create it if allowed
        if (errno == ENOENT && !noParentCreate)
        {
            String *const pathParent = strPath(path);

            storageInterfacePathCreateP(this, pathParent, errorOnExists, noParentCreate, mode);
            storageInterfacePathCreateP(this, path, errorOnExists, noParentCreate, mode);

            strFree(pathParent);
        }
        // Ignore path exists if allowed
        else if (errno != EEXIST || errorOnExists)
            THROW_SYS_ERROR_FMT(PathCreateError, "unable to create path '%s'", strZ(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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
            StorageList *const list = storageInterfaceListP(this, path, storageInfoLevelExists);

            if (list != NULL)
            {
                MEM_CONTEXT_TEMP_RESET_BEGIN()
                {
                    for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
                    {
                        const String *const file = strNewFmt("%s/%s", strZ(path), strZ(storageLstGet(list, listIdx).name));

                        // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
                        if (unlink(strZ(file)) == -1)                                                               // {vm_covered}
                        {
                            // These errors indicate that the entry is actually a path so we'll try to delete it that way
                            if (errno == EPERM || errno == EISDIR)               // {uncovered_branch - no EPERM on tested systems}
                            {
                                storageInterfacePathRemoveP(this, file, true);
                            }
                            // Else error
                            else
                                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE_FILE, strZ(file));   // {vm_covered}
                        }

                        // Reset the memory context occasionally so we don't use too much memory or slow down processing
                        MEM_CONTEXT_TEMP_RESET(1000);
                    }
                }
                MEM_CONTEXT_TEMP_END();
            }
        }

        // Delete the path
        if (rmdir(strZ(path)) == -1)
        {
            if (errno != ENOENT)                                                                                    // {vm_covered}
                THROW_SYS_ERROR_FMT(PathRemoveError, STORAGE_ERROR_PATH_REMOVE, strZ(path));                        // {vm_covered}

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
    int fd = open(strZ(path), O_RDONLY, 0);

    // Handle errors
    if (fd == -1)
    {
        if (errno == ENOENT)                                                                                        // {vm_covered}
            THROW_FMT(PathMissingError, STORAGE_ERROR_PATH_SYNC_MISSING, strZ(path));
        else
            THROW_SYS_ERROR_FMT(PathOpenError, STORAGE_ERROR_PATH_SYNC_OPEN, strZ(path));                           // {vm_covered}
    }
    else
    {
        // Attempt to sync the directory
        if (fsync(fd) == -1)
        {
            int errNo = errno;

            // Close the file descriptor to free resources but don't check for failure
            close(fd);

            THROW_SYS_ERROR_CODE_FMT(errNo, PathSyncError, STORAGE_ERROR_PATH_SYNC, strZ(path));
        }

        THROW_ON_SYS_ERROR_FMT(close(fd) == -1, PathCloseError, STORAGE_ERROR_PATH_SYNC_CLOSE, strZ(path));
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
    if (unlink(strZ(file)) == -1)
    {
        if (param.errorOnMissing || errno != ENOENT)                                                                // {vm_covered}
            THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strZ(file));                              // {vm_covered}
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
static const StorageInterface storageInterfacePosix =
{
    .feature = 1 << storageFeaturePath,

    .info = storagePosixInfo,
    .list = storagePosixList,
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
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool pathSync)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, pathSync);
    FUNCTION_LOG_END();

    ASSERT(type != 0);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Initialize user module
    userInit();

    // Create the object
    Storage *this = NULL;

    OBJ_NEW_BEGIN(StoragePosix, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = MEM_CONTEXT_QTY_MAX)
    {
        StoragePosix *driver = OBJ_NEW_ALLOC();

        *driver = (StoragePosix)
        {
            .interface = storageInterfacePosix,
        };

        // Disable path sync when not supported
        if (!pathSync)
            driver->interface.pathSync = NULL;

        // If this is a posix driver then add link features
        if (type == STORAGE_POSIX_TYPE)
            driver->interface.feature |=
                1 << storageFeatureHardLink | 1 << storageFeatureSymLink | 1 << storageFeaturePathSync |
                1 << storageFeatureInfoDetail;

        this = storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, driver->interface);
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE, this);
}

Storage *
storagePosixNew(const String *path, StoragePosixNewParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, param.modeFile);
        FUNCTION_LOG_PARAM(MODE, param.modePath);
        FUNCTION_LOG_PARAM(BOOL, param.write);
        FUNCTION_LOG_PARAM(FUNCTIONP, param.pathExpressionFunction);
    FUNCTION_LOG_END();

    FUNCTION_LOG_RETURN(
        STORAGE,
        storagePosixNewInternal(
            STORAGE_POSIX_TYPE, path, param.modeFile == 0 ? STORAGE_MODE_FILE_DEFAULT : param.modeFile,
            param.modePath == 0 ? STORAGE_MODE_PATH_DEFAULT : param.modePath, param.write, param.pathExpressionFunction, true));
}
