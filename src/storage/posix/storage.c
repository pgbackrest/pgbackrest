/***********************************************************************************************************************************
Posix Storage
***********************************************************************************************************************************/
#include "build.auto.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/posix/common.h"
#include "storage/posix/read.h"
#include "storage/posix/storage.intern.h"
#include "storage/posix/write.h"

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
STRING_EXTERN(STORAGE_POSIX_TYPE_STR,                               STORAGE_POSIX_TYPE);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StoragePosix
{
    MemContext *memContext;                                         // Object memory context
    bool syncPath;                                                  // Will paths be synced?
};

/***********************************************************************************************************************************
Does a file exist? This function is only for files, not paths.
***********************************************************************************************************************************/
static bool
storagePosixExists(THIS_VOID,  const String *path)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    // Attempt to stat the file to determine if it exists
    struct stat statFile;

    // Any error other than entry not found should be reported
    if (stat(strPtr(path), &statFile) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to stat '%s'", strPtr(path));
    }
    // Else found
    else
        result = !S_ISDIR(statFile.st_mode);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
File/path info
***********************************************************************************************************************************/
static StorageInfo
storagePosixInfo(THIS_VOID, const String *file, bool ignoreMissing, bool followLink)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
        FUNCTION_LOG_PARAM(BOOL, followLink);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    StorageInfo result = {0};

    // Attempt to stat the file
    struct stat statFile;

    if ((followLink ? stat(strPtr(file), &statFile) : lstat(strPtr(file), &statFile)) == -1)
    {
        if (errno != ENOENT || !ignoreMissing)
            THROW_SYS_ERROR_FMT(FileOpenError, "unable to get info for '%s'", strPtr(file));
    }
    // On success load info into a structure
    else
    {
        result.exists = true;
        result.timeModified = statFile.st_mtime;

        // Get user name if it exists
        struct passwd *userData = getpwuid(statFile.st_uid);

        if (userData != NULL)
            result.user = strNew(userData->pw_name);

        // Get group name if it exists
        struct group *groupData = getgrgid(statFile.st_gid);

        if (groupData != NULL)
            result.group = strNew(groupData->gr_name);

        if (S_ISREG(statFile.st_mode))
        {
            result.type = storageTypeFile;
            result.size = (uint64_t)statFile.st_size;
        }
        else if (S_ISDIR(statFile.st_mode))
            result.type = storageTypePath;
        else if (S_ISLNK(statFile.st_mode))
        {
            result.type = storageTypeLink;

            char linkDestination[PATH_MAX];
            ssize_t linkDestinationSize = 0;

            THROW_ON_SYS_ERROR_FMT(
                (linkDestinationSize = readlink(strPtr(file), linkDestination, sizeof(linkDestination) - 1)) == -1,
                FileReadError, "unable to get destination for link '%s'", strPtr(file));

            result.linkDestination = strNewN(linkDestination, (size_t)linkDestinationSize);
        }
        else
            THROW_FMT(FileInfoError, "invalid type for '%s'", strPtr(file));

        result.mode = statFile.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    }

    FUNCTION_LOG_RETURN(STORAGE_INFO, result);
}

/***********************************************************************************************************************************
Info for all files/paths in a path
***********************************************************************************************************************************/
static void
storagePosixInfoListEntry(
    StoragePosix *this, const String *path, const String *name, StorageInfoListCallback callback, void *callbackData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_POSIX, this);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(FUNCTIONP, callback);
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(name != NULL);
    ASSERT(callback != NULL);

    if (!strEqZ(name, ".."))
    {
        String *pathInfo = strEqZ(name, ".") ? strDup(path) : strNewFmt("%s/%s", strPtr(path), strPtr(name));

        StorageInfo storageInfo = storagePosixInfo(this, pathInfo, true, false);

        if (storageInfo.exists)
        {
            storageInfo.name = name;
            callback(callbackData, &storageInfo);
        }

        strFree(pathInfo);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static bool
storagePosixInfoList(THIS_VOID, const String *path, bool errorOnMissing, StorageInfoListCallback callback, void *callbackData)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(FUNCTIONP, callback);
        FUNCTION_LOG_PARAM_P(VOID, callbackData);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(callback != NULL);

    DIR *dir = NULL;
    bool result = false;

    TRY_BEGIN()
    {
        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathOpenError, "unable to open path '%s' for read", strPtr(path));
        }
        else
        {
            // Directory was found
            result = true;

            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Read the directory entries
                struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    // Get info and perform callback
                    storagePosixInfoListEntry(this, path, STR(dirEntry->d_name), callback, callbackData);

                    // Get next entry
                    dirEntry = readdir(dir);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    FINALLY()
    {
        if (dir != NULL)
            closedir(dir);
    }
    TRY_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
static StringList *
storagePosixList(THIS_VOID, const String *path, bool errorOnMissing, const String *expression)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    StringList *result = NULL;
    DIR *dir = NULL;

    TRY_BEGIN()
    {
        // Open the directory for read
        dir = opendir(strPtr(path));

        // If the directory could not be opened process errors but ignore missing directories when specified
        if (!dir)
        {
            if (errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathOpenError, "unable to open path '%s' for read", strPtr(path));
        }
        else
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Prepare regexp if an expression was passed
                RegExp *regExp = (expression == NULL) ? NULL : regExpNew(expression);

                // Create the string list now that we know the directory is valid
                result = strLstNew();

                // Read the directory entries
                struct dirent *dirEntry = readdir(dir);

                while (dirEntry != NULL)
                {
                    const String *entry = STR(dirEntry->d_name);

                    // Exclude current/parent directory and apply the expression if specified
                    if (!strEqZ(entry, ".") && !strEqZ(entry, "..") && (regExp == NULL || regExpMatch(regExp, entry)))
                        strLstAdd(result, entry);

                    dirEntry = readdir(dir);
                }

                // Move finished list up to the old context
                strLstMove(result, MEM_CONTEXT_OLD());
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    FINALLY()
    {
        if (dir != NULL)
            closedir(dir);
    }
    TRY_END();

    FUNCTION_LOG_RETURN(STRING_LIST, result);
}

/***********************************************************************************************************************************
Move a path/file
***********************************************************************************************************************************/
static bool
storagePosixMove(THIS_VOID,  StorageRead *source, StorageWrite *destination)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STORAGE_READ, source);
        FUNCTION_LOG_PARAM(STORAGE_WRITE, destination);
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
            // Detemine which file/path is missing
            if (errno == ENOENT)
            {
                if (!storagePosixExists(this, sourceFile))
                    THROW_SYS_ERROR_FMT(FileMissingError, "unable to move missing file '%s'", strPtr(sourceFile));

                if (!storageWriteCreatePath(destination))
                {
                    THROW_SYS_ERROR_FMT(
                        PathMissingError, "unable to move '%s' to missing path '%s'", strPtr(sourceFile), strPtr(destinationPath));
                }

                storagePosixPathCreate(this, destinationPath, false, false, storageWriteModePath(destination));
                result = storagePosixMove(this, source, destination);
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
                    storagePosixPathSync(this, sourcePath, false);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
New file read object
***********************************************************************************************************************************/
static StorageRead *
storagePosixNewRead(THIS_VOID, const String *file, bool ignoreMissing)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(STORAGE_READ, storageReadPosixNew(this, file, ignoreMissing));
}

/***********************************************************************************************************************************
New file write object
***********************************************************************************************************************************/
static StorageWrite *
storagePosixNewWrite(
    THIS_VOID, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group, time_t timeModified,
    bool createPath, bool syncFile, bool syncPath, bool atomic)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(STRING, user);
        FUNCTION_LOG_PARAM(STRING, group);
        FUNCTION_LOG_PARAM(INT64, timeModified);
        FUNCTION_LOG_PARAM(BOOL, createPath);
        FUNCTION_LOG_PARAM(BOOL, syncFile);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
        FUNCTION_LOG_PARAM(BOOL, atomic);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    FUNCTION_LOG_RETURN(
        STORAGE_WRITE,
        storageWritePosixNew(
            this, file, modeFile, modePath, user, group, timeModified, createPath, syncFile, this->syncPath ? syncPath : false,
            atomic));
}

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
void
storagePosixPathCreate(THIS_VOID, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnExists);
        FUNCTION_LOG_PARAM(BOOL, noParentCreate);
        FUNCTION_LOG_PARAM(MODE, mode);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    // Attempt to create the directory
    if (mkdir(strPtr(path), mode) == -1)
    {
        // If the parent path does not exist then create it if allowed
        if (errno == ENOENT && !noParentCreate)
        {
            storagePosixPathCreate(this, strPath(path), errorOnExists, noParentCreate, mode);
            storagePosixPathCreate(this, path, errorOnExists, noParentCreate, mode);
        }
        // Ignore path exists if allowed
        else if (errno != EEXIST || errorOnExists)
            THROW_SYS_ERROR_FMT(PathCreateError, "unable to create path '%s'", strPtr(path));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Does a path exist? This function is only for paths, not file.
***********************************************************************************************************************************/
static bool
storagePosixPathExists(THIS_VOID,  const String *path)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    bool result = false;

    // Attempt to stat the file to determine if it exists
    struct stat statPath;

    // Any error other than entry not found should be reported
    if (stat(strPtr(path), &statPath) == -1)
    {
        if (errno != ENOENT)
            THROW_SYS_ERROR_FMT(PathOpenError, "unable to stat '%s'", strPtr(path));
    }
    // Else found
    else
        result = S_ISDIR(statPath.st_mode);

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
static void
storagePosixPathRemove(THIS_VOID, const String *path, bool errorOnMissing, bool recurse)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            // Get a list of files in this path
            StringList *fileList = storagePosixList(this, path, errorOnMissing, NULL);

            // Only continue if the path exists
            if (fileList != NULL)
            {
                // Delete all paths and files
                for (unsigned int fileIdx = 0; fileIdx < strLstSize(fileList); fileIdx++)
                {
                    String *file = strNewFmt("%s/%s", strPtr(path), strPtr(strLstGet(fileList, fileIdx)));

                    // Rather than stat the file to discover what type it is, just try to unlink it and see what happens
                    if (unlink(strPtr(file)) == -1)
                    {
                        // These errors indicate that the entry is actually a path so we'll try to delete it that way
                        if (errno == EPERM || errno == EISDIR)              // {uncovered - EPERM is not returned on tested systems}
                            storagePosixPathRemove(this, file, false, true);
                        // Else error
                        else
                            THROW_SYS_ERROR_FMT(PathRemoveError, "unable to remove path/file '%s'", strPtr(file));
                    }
                }
            }
        }

        // Delete the path
        if (rmdir(strPtr(path)) == -1)
        {
            if (errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR_FMT(PathRemoveError, "unable to remove path '%s'", strPtr(path));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
void
storagePosixPathSync(THIS_VOID, const String *path, bool ignoreMissing)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, ignoreMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);

    if (this->syncPath)
    {
        // Open directory and handle errors
        int handle = storagePosixFileOpen(path, O_RDONLY, 0, ignoreMissing, false, "sync");

        // On success
        if (handle != -1)
        {
            // Attempt to sync the directory
            storagePosixFileSync(handle, path, false, true);

            // Close the directory
            storagePosixFileClose(handle, path, false);
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
static void
storagePosixRemove(THIS_VOID, const String *file, bool errorOnMissing)
{
    THIS(StoragePosix);

    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_POSIX, this);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);

    // Attempt to unlink the file
    if (unlink(strPtr(file)) == -1)
    {
        if (errorOnMissing || errno != ENOENT)
            THROW_SYS_ERROR_FMT(FileRemoveError, "unable to remove '%s'", strPtr(file));
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
New object
***********************************************************************************************************************************/
Storage *
storagePosixNewInternal(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, bool syncPath)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, type);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(MODE, modeFile);
        FUNCTION_LOG_PARAM(MODE, modePath);
        FUNCTION_LOG_PARAM(BOOL, write);
        FUNCTION_LOG_PARAM(FUNCTIONP, pathExpressionFunction);
        FUNCTION_LOG_PARAM(BOOL, syncPath);
    FUNCTION_LOG_END();

    ASSERT(type != NULL);
    ASSERT(path != NULL);
    ASSERT(modeFile != 0);
    ASSERT(modePath != 0);

    // Create the object
    Storage *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("StoragePosix")
    {
        StoragePosix *driver = memNew(sizeof(StoragePosix));
        driver->memContext = MEM_CONTEXT_NEW();
        driver->syncPath = syncPath;

        this = storageNewP(
            type, path, modeFile, modePath, write, pathExpressionFunction, driver, .exists = storagePosixExists,
            .info = storagePosixInfo, .infoList = storagePosixInfoList, .list = storagePosixList, .move = storagePosixMove,
            .newRead = storagePosixNewRead, .newWrite = storagePosixNewWrite, .pathCreate = storagePosixPathCreate,
            .pathExists = storagePosixPathExists, .pathRemove = storagePosixPathRemove, .pathSync = storagePosixPathSync,
            .remove = storagePosixRemove);
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
        STORAGE,
        storagePosixNewInternal(
            STORAGE_POSIX_TYPE_STR, path, modeFile, modePath, write, pathExpressionFunction, true));
}
