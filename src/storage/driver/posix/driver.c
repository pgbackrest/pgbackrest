/***********************************************************************************************************************************
Storage Driver Posix
***********************************************************************************************************************************/
// So lstat() will work on older glib versions
#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200112L
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/memContext.h"
#include "common/regExp.h"
#include "storage/driver/posix/driver.h"
#include "storage/driver/posix/driverFile.h"
#include "storage/storage.h"

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
File/path info
***********************************************************************************************************************************/
StorageInfo
storageDriverPosixInfo(const String *file, bool ignoreMissing)
{
    StorageInfo result = {0};

    // Attempt to stat the file
    struct stat statFile;

    if (lstat(strPtr(file), &statFile) == -1)
    {
        if (errno != ENOENT || !ignoreMissing)
            THROW_SYS_ERROR(FileOpenError, "unable to get info for '%s'", strPtr(file));
    }
    // On success load info into a structure
    else
    {
        result.exists = true;

        if (S_ISREG(statFile.st_mode))
        {
            result.type = storageTypeFile;
            result.size = (size_t)statFile.st_size;
        }
        else if (S_ISDIR(statFile.st_mode))
            result.type = storageTypePath;
        else if (S_ISLNK(statFile.st_mode))
            result.type = storageTypeLink;
        else
            THROW(FileInfoError, "invalid type for '%s'", strPtr(file));

        result.mode = statFile.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
    }

    return result;
}

/***********************************************************************************************************************************
Get a list of files from a directory
***********************************************************************************************************************************/
StringList *
storageDriverPosixList(const String *path, bool errorOnMissing, const String *expression)
{
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
                THROW_SYS_ERROR(PathOpenError, "unable to open path '%s' for read", strPtr(path));
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
                    String *entry = strNew(dirEntry->d_name);

                    // Exclude current/parent directory and apply the expression if specified
                    if (!strEqZ(entry, ".") && !strEqZ(entry, "..") && (regExp == NULL || regExpMatch(regExp, entry)))
                        strLstAdd(result, entry);

                    strFree(entry);

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

    return result;
}

/***********************************************************************************************************************************
Move a file
***********************************************************************************************************************************/
bool
storageDriverPosixMove(StorageFileReadPosix *source, StorageFileWritePosix *destination)
{
    bool result = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *sourceFile = storageFileReadPosixName(source);
        const String *destinationFile = storageFileWritePosixName(destination);
        const String *destinationPath = storageFileWritePosixPath(destination);

        // Attempt to move the file
        if (rename(strPtr(sourceFile), strPtr(destinationFile)) == -1)
        {
            // Detemine which file/path is missing
            if (errno == ENOENT)
            {
                if (!storageDriverPosixExists(sourceFile))
                    THROW_SYS_ERROR(FileMissingError, "unable to move missing file '%s'", strPtr(sourceFile));

                if (!storageFileWritePosixCreatePath(destination))
                {
                    THROW_SYS_ERROR(
                        PathMissingError, "unable to move '%s' to missing path '%s'", strPtr(sourceFile), strPtr(destinationPath));
                }

                storageDriverPosixPathCreate(destinationPath, false, false, storageFileWritePosixModePath(destination));
                result = storageDriverPosixMove(source, destination);
            }
            // Else the destination is on a different device so a copy will be needed
            else if (errno == EXDEV)
            {
                result = false;
            }
            else
                THROW_SYS_ERROR(FileMoveError, "unable to move '%s' to '%s'", strPtr(sourceFile), strPtr(destinationFile));
        }
        // Sync paths on success
        else
        {
            // Sync source path if the destination path was synced and the paths are not equal
            if (storageFileWritePosixSyncPath(destination))
            {
                String *sourcePath = strPath(sourceFile);

                if (!strEq(destinationPath, sourcePath))
                    storageDriverPosixPathSync(sourcePath, false);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

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
Remove a path
***********************************************************************************************************************************/
void
storageDriverPosixPathRemove(const String *path, bool errorOnMissing, bool recurse)
{
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Recurse if requested
        if (recurse)
        {
            // Get a list of files in this path
            StringList *fileList = storageDriverPosixList(path, errorOnMissing, NULL);

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
                            storageDriverPosixPathRemove(file, false, true);
                        // Else error
                        else
                            THROW_SYS_ERROR(PathRemoveError, "unable to remove path/file '%s'", strPtr(file));
                    }
                }
            }
        }

        // Delete the path
        if (rmdir(strPtr(path)) == -1)
        {
            if (errorOnMissing || errno != ENOENT)
                THROW_SYS_ERROR(PathRemoveError, "unable to remove path '%s'", strPtr(path));
        }
    }
    MEM_CONTEXT_TEMP_END();
}

/***********************************************************************************************************************************
Sync a path
***********************************************************************************************************************************/
void
storageDriverPosixPathSync(const String *path, bool ignoreMissing)
{
    // Open directory and handle errors
    int handle = storageFilePosixOpen(path, O_RDONLY, 0, ignoreMissing, &PathOpenError, "sync");

    // On success
    if (handle != -1)
    {
        // Attempt to sync the directory
        storageFilePosixSync(handle, path, &PathSyncError, true);

        // Close the directory
        storageFilePosixClose(handle, path, &PathCloseError);
    }
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
