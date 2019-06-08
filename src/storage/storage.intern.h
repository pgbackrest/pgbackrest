/***********************************************************************************************************************************
Storage Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_INTERN_H
#define STORAGE_STORAGE_INTERN_H

#include "storage/read.intern.h"
#include "storage/storage.h"
#include "storage/write.intern.h"

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_MODE_FILE_DEFAULT                                   0640
#define STORAGE_MODE_PATH_DEFAULT                                   0750

/***********************************************************************************************************************************
Error messages
***********************************************************************************************************************************/
#define STORAGE_ERROR_READ_CLOSE                                    "unable to close file '%s' after read"
#define STORAGE_ERROR_READ_OPEN                                     "unable to open file '%s' for read"
#define STORAGE_ERROR_READ_MISSING                                  "unable to open missing file '%s' for read"

#define STORAGE_ERROR_INFO                                          "unable to get info for path/file '%s'"
#define STORAGE_ERROR_INFO_MISSING                                  "unable to get info for missing path/file '%s'"

#define STORAGE_ERROR_LIST                                          "unable to list files for path '%s'"
#define STORAGE_ERROR_LIST_MISSING                                  "unable to list files for missing path '%s'"

#define STORAGE_ERROR_LIST_INFO                                     "unable to list file info for path '%s'"
#define STORAGE_ERROR_LIST_INFO_MISSING                             "unable to list file info for missing path '%s'"

#define STORAGE_ERROR_PATH_REMOVE                                   "unable to remove path '%s'"
#define STORAGE_ERROR_PATH_REMOVE_FILE                              "unable to remove file '%s'"
#define STORAGE_ERROR_PATH_REMOVE_MISSING                           "unable to remove missing path '%s'"

#define STORAGE_ERROR_PATH_SYNC                                     "unable to sync path '%s'"
#define STORAGE_ERROR_PATH_SYNC_CLOSE                               "unable to close path '%s' after sync"
#define STORAGE_ERROR_PATH_SYNC_OPEN                                "unable to open path '%s' for sync"
#define STORAGE_ERROR_PATH_SYNC_MISSING                             "unable to sync missing path '%s'"

#define STORAGE_ERROR_WRITE_CLOSE                                   "unable to close file '%s' after write"
#define STORAGE_ERROR_WRITE_OPEN                                    "unable to open file '%s' for write"
#define STORAGE_ERROR_WRITE_MISSING                                 "unable to open file '%s' for write in missing path"
#define STORAGE_ERROR_WRITE_SYNC                                    "unable to sync file '%s' after write"

/***********************************************************************************************************************************
Path expression callback function type - used to modify paths based on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct StorageInterface
{
    // Features implemented by the storage driver
    uint64_t feature;

    bool (*exists)(void *driver, const String *file);
    StorageInfo (*info)(void *driver, const String *path, bool followLink);
    bool (*infoList)(void *driver, const String *file, StorageInfoListCallback callback, void *callbackData);
    StringList *(*list)(void *driver, const String *path, const String *expression);
    bool (*move)(void *driver, StorageRead *source, StorageWrite *destination);
    StorageRead *(*newRead)(void *driver, const String *file, bool ignoreMissing);
    StorageWrite *(*newWrite)(
        void *driver, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group,
        time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);
    void (*pathCreate)(void *driver, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
    bool (*pathExists)(void *driver, const String *path);
    bool (*pathRemove)(void *driver, const String *path, bool recurse);
    void (*pathSync)(void *driver, const String *path);
    void (*remove)(void *driver, const String *file, bool errorOnMissing);
} StorageInterface;

#define storageNewP(type, path, modeFile, modePath, write, pathExpressionFunction, driver, ...)                                    \
    storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, (StorageInterface){__VA_ARGS__})

Storage *storageNew(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, void *driver, StorageInterface interface);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
void *storageDriver(const Storage *this);
StorageInterface storageInterface(const Storage *this);

// The option is intended to be used only with the Perl interface since Perl is not tidy about where it reads.  It should be
// removed when the Perl interface is removed.
void storagePathEnforceSet(Storage *this, bool enforce);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_INTERFACE_TYPE                                                                                        \
    StorageInterface
#define FUNCTION_LOG_STORAGE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(&value, "StorageInterface", buffer, bufferSize)

#endif
