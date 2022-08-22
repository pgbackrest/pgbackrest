/***********************************************************************************************************************************
Storage Interface Internal

Storage drivers are implemented using this interface.

The interface has required and optional functions. Currently the optional functions are only implemented by the Posix driver which
can store either a repository or a PostgreSQL cluster. Drivers that are intended to store repositories only need to implement the
required functions.

The behavior of required functions is further modified by storage features defined by the StorageFeature enum. Details are included
in the description of each function.
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_INTERN_H
#define STORAGE_STORAGE_INTERN_H

#include "common/type/param.h"
#include "storage/info.h"
#include "storage/list.h"
#include "storage/read.h"
#include "storage/write.h"

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
#define STORAGE_ERROR_READ_SEEK                                     "unable to seek to %" PRIu64 " in file '%s'"

#define STORAGE_ERROR_INFO                                          "unable to get info for path/file '%s'"
#define STORAGE_ERROR_INFO_MISSING                                  "unable to get info for missing path/file '%s'"

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
typedef String *StoragePathExpressionCallback(const String *expression, const String *path);

/***********************************************************************************************************************************
Storage info callback function type - used to return storage info
***********************************************************************************************************************************/
typedef void (*StorageListCallback)(void *callbackData, const StorageInfo *info);

/***********************************************************************************************************************************
Required interface functions
***********************************************************************************************************************************/
// Get information about a file/link/path
//
// The level parameter controls the amount of information that will be returned. See the StorageInfo type and StorageInfoLevel enum
// for details about information that must be provided at each level. The driver should only return the amount of information
// requested even if more is available. All drivers must implement the storageInfoLevelExists and storageInfoLevelBasic levels. Only
// drivers with the storageFeatureInfoDetail feature must implement the storageInfoLevelDetail level.
typedef struct StorageInterfaceInfoParam
{
    VAR_PARAM_HEADER;

    // Should symlinks be followed?  Only required on storage that supports symlinks.
    bool followLink;
} StorageInterfaceInfoParam;

typedef StorageInfo StorageInterfaceInfo(
    void *thisVoid, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param);

#define storageInterfaceInfoP(thisVoid, file, level, ...)                                                                          \
    STORAGE_COMMON_INTERFACE(thisVoid).info(thisVoid, file, level, (StorageInterfaceInfoParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Create a file read object.  The file should not be opened immediately -- open() will be called on the IoRead interface when the
// file needs to be opened.
typedef struct StorageInterfaceNewReadParam
{
    VAR_PARAM_HEADER;

    // Is the file compressible? This is used when the file must be moved across a network and temporary compression is helpful.
    bool compressible;

    // Where to start reading in the file
    const uint64_t offset;

    // Limit bytes read from the file. NULL for no limit.
    const Variant *limit;
} StorageInterfaceNewReadParam;

typedef StorageRead *StorageInterfaceNewRead(
    void *thisVoid, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param);

#define storageInterfaceNewReadP(thisVoid, file, ignoreMissing, ...)                                                               \
    STORAGE_COMMON_INTERFACE(thisVoid).newRead(                                                                                    \
        thisVoid, file, ignoreMissing, (StorageInterfaceNewReadParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Create a file write object.  The file should not be opened immediately -- open() will be called on the IoWrite interface when the
// file needs to be opened.
typedef struct StorageInterfaceNewWriteParam
{
    VAR_PARAM_HEADER;

    // File/path mode for storage that supports Posix-style permissions.  modePath is only used in conjunction with createPath.
    mode_t modeFile;
    mode_t modePath;

    // User/group name
    const String *user;
    const String *group;

    // Modified time
    time_t timeModified;

    // Will paths be created as needed?
    bool createPath;

    // Sync file/path when required by the storage
    bool syncFile;
    bool syncPath;

    // Ensure the file is written atomically. If this is false it's OK to write atomically if that's all the storage supports
    // (e.g. S3). Non-atomic writes are used in some places where there is a performance advantage and atomicity is not needed.
    bool atomic;

    // Is the file compressible?  This is used when the file must be moved across a network and temporary compression is helpful.
    bool compressible;
} StorageInterfaceNewWriteParam;

typedef StorageWrite *StorageInterfaceNewWrite(void *thisVoid, const String *file, StorageInterfaceNewWriteParam param);

#define storageInterfaceNewWriteP(thisVoid, file, ...)                                                                             \
    STORAGE_COMMON_INTERFACE(thisVoid).newWrite(thisVoid, file, (StorageInterfaceNewWriteParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Get info for all files/links/paths in a path (does not recurse or return info about the path passed to the function)
//
// See storageInterfaceInfoP() for usage of the level parameter.
typedef struct StorageInterfaceListParam
{
    VAR_PARAM_HEADER;

    // Regular expression used to filter the results. The expression is always checked in the callback passed to
    // storageInterfaceIterP() so checking the expression in the driver is entirely optional. The driver should only use the
    // expression if it can improve performance or limit network transfer.
    //
    // Partial matching of the expression is fine as long as nothing that should match is excluded, e.g. it is OK to prefix match
    // using the prefix returned from regExpPrefix(). This may cause extra results to be sent to the callback but won't exclude
    // anything that matches the expression exactly.
    const String *expression;
} StorageInterfaceListParam;

typedef StorageList *StorageInterfaceList(
    void *thisVoid, const String *path, StorageInfoLevel level, StorageInterfaceListParam param);

#define storageInterfaceListP(thisVoid, path, level, ...)                                                                          \
    STORAGE_COMMON_INTERFACE(thisVoid).list(                                                                                       \
        thisVoid, path, level, (StorageInterfaceListParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Remove a path (and optionally recurse)
typedef struct StorageInterfacePathRemoveParam
{
    VAR_PARAM_HEADER;
} StorageInterfacePathRemoveParam;

typedef bool StorageInterfacePathRemove(void *thisVoid, const String *path, bool recurse, StorageInterfacePathRemoveParam param);

#define storageInterfacePathRemoveP(thisVoid, path, recurse, ...)                                                                  \
    STORAGE_COMMON_INTERFACE(thisVoid).pathRemove(                                                                                 \
        thisVoid, path, recurse, (StorageInterfacePathRemoveParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Remove a file
typedef struct StorageInterfaceRemoveParam
{
    VAR_PARAM_HEADER;

    // Error when the file to delete is missing
    bool errorOnMissing;
} StorageInterfaceRemoveParam;

typedef void StorageInterfaceRemove(void *thisVoid, const String *file, StorageInterfaceRemoveParam param);

#define storageInterfaceRemoveP(thisVoid, file, ...)                                                                               \
    STORAGE_COMMON_INTERFACE(thisVoid).remove(thisVoid, file, (StorageInterfaceRemoveParam){VAR_PARAM_INIT, __VA_ARGS__})

/***********************************************************************************************************************************
Optional interface functions
***********************************************************************************************************************************/
// Move a path/file atomically
typedef struct StorageInterfaceMoveParam
{
    VAR_PARAM_HEADER;
} StorageInterfaceMoveParam;

typedef bool StorageInterfaceMove(void *thisVoid, StorageRead *source, StorageWrite *destination, StorageInterfaceMoveParam param);

#define storageInterfaceMoveP(thisVoid, source, destination, ...)                                                                  \
    STORAGE_COMMON_INTERFACE(thisVoid).move(                                                                                       \
        thisVoid, source, destination, (StorageInterfaceMoveParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Create a path
typedef struct StorageInterfacePathCreateParam
{
    VAR_PARAM_HEADER;
} StorageInterfacePathCreateParam;

typedef void StorageInterfacePathCreate(
    void *thisVoid, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode,
    StorageInterfacePathCreateParam param);

#define storageInterfacePathCreateP(thisVoid, path, errorOnExists, noParentCreate, mode, ...)                                      \
    STORAGE_COMMON_INTERFACE(thisVoid).pathCreate(                                                                                 \
        thisVoid, path, errorOnExists, noParentCreate, mode, (StorageInterfacePathCreateParam){VAR_PARAM_INIT, __VA_ARGS__})

// ---------------------------------------------------------------------------------------------------------------------------------
// Sync a path
typedef struct StorageInterfacePathSyncParam
{
    VAR_PARAM_HEADER;
} StorageInterfacePathSyncParam;

typedef void StorageInterfacePathSync(void *thisVoid, const String *path, StorageInterfacePathSyncParam param);

#define storageInterfacePathSyncP(thisVoid, path, ...)                                                                             \
    STORAGE_COMMON_INTERFACE(thisVoid).pathSync(thisVoid, path, (StorageInterfacePathSyncParam){VAR_PARAM_INIT, __VA_ARGS__})

/***********************************************************************************************************************************
Storage type and helper function struct

An array of this struct must be passed to storageHelperInit() to enable storage drivers other than built-in Posix.
***********************************************************************************************************************************/
typedef Storage *(*StorageHelperFunction)(unsigned int repoIdx, bool write, StoragePathExpressionCallback pathExpressionCallback);

typedef struct StorageHelper
{
    StringId type;                                                  // StringId that identifies the storage driver
    StorageHelperFunction helper;                                   // Function that creates the storage
} StorageHelper;

#define STORAGE_END_HELPER                                          {.type = 0}

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageInterface
{
    // Features implemented by the storage driver
    uint64_t feature;

    // Required functions
    StorageInterfaceInfo *info;
    StorageInterfaceList *list;
    StorageInterfaceNewRead *newRead;
    StorageInterfaceNewWrite *newWrite;
    StorageInterfacePathRemove *pathRemove;
    StorageInterfaceRemove *remove;

    // Optional functions
    StorageInterfaceMove *move;
    StorageInterfacePathCreate *pathCreate;
    StorageInterfacePathSync *pathSync;
} StorageInterface;

#define storageNewP(type, path, modeFile, modePath, write, pathExpressionFunction, driver, ...)                                    \
    storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, (StorageInterface){__VA_ARGS__})

Storage *storageNew(
    StringId type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, void *driver, StorageInterface interface);

/***********************************************************************************************************************************
Common members to include in every storage driver and macros to extract the common elements
***********************************************************************************************************************************/
#define STORAGE_COMMON_MEMBER                                                                                                      \
    StorageInterface interface                                      /* Storage interface */

typedef struct StorageCommon
{
    STORAGE_COMMON_MEMBER;
} StorageCommon;

#define STORAGE_COMMON(thisVoid)                                                                                                   \
    ((const StorageCommon *)thisVoid)
#define STORAGE_COMMON_INTERFACE(thisVoid)                                                                                         \
    (STORAGE_COMMON(thisVoid)->interface)

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct StoragePub
{
    StringId type;                                                  // Storage type
    void *driver;                                                   // Storage driver
    StorageInterface interface;                                     // Storage interface
} StoragePub;

// Storage driver
__attribute__((always_inline)) static inline void *
storageDriver(const Storage *const this)
{
    return THIS_PUB(Storage)->driver;
}

// Storage interface
__attribute__((always_inline)) static inline StorageInterface
storageInterface(const Storage *const this)
{
    return THIS_PUB(Storage)->interface;
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_INTERFACE_TYPE                                                                                        \
    StorageInterface
#define FUNCTION_LOG_STORAGE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(&value, "StorageInterface", buffer, bufferSize)

#endif
