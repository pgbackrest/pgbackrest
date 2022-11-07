/***********************************************************************************************************************************
Storage Interface
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Storage link type
***********************************************************************************************************************************/
typedef enum
{
    // Symbolic (or soft) link
    storageLinkSym,

    // Hard link
    storageLinkHard,
} StorageLinkType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Storage Storage;

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "common/io/filter/group.h"
#include "common/time.h"
#include "common/type/param.h"
#include "storage/info.h"
#include "storage/iterator.h"
#include "storage/read.h"
#include "storage/storage.intern.h"
#include "storage/write.h"

/***********************************************************************************************************************************
Storage feature
***********************************************************************************************************************************/
typedef enum
{
    // Does the storage support paths/directories as something that needs to be created and deleted?  Object stores (e.g. S3) often
    // do not have paths/directories -- they are only inferred by the object name.  Therefore it doesn't make sense to create or
    // remove directories since this implies something is happening on the storage and in the case of objects stores it would be a
    // noop.  We'll error on any path operation (e.g. pathExists(), pathCreate(), non-recursive removes, error on missing paths,
    // etc.) for storage that does not support paths.
    storageFeaturePath,

    // Do paths need to be synced to ensure contents are durable?  storeageFeaturePath must also be enabled.
    storageFeaturePathSync,

    // Does the storage support hardlinks?  Hardlinks allow the same file to be linked into multiple paths to save space.
    storageFeatureHardLink,

    // Does the storage support symlinks?  Symlinks allow paths/files/links to be accessed from another path.
    storageFeatureSymLink,

    // Does the storage support detailed info, i.e. user, group, mode, link destination, etc.
    storageFeatureInfoDetail,
} StorageFeature;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Copy a file
#define storageCopyP(source, destination)                                                                                          \
    storageCopy(source, destination)

bool storageCopy(StorageRead *source, StorageWrite *destination);

// Does a file exist? This function is only for files, not paths.
typedef struct StorageExistsParam
{
    VAR_PARAM_HEADER;
    TimeMSec timeout;
} StorageExistsParam;

#define storageExistsP(this, pathExp, ...)                                                                                         \
    storageExists(this, pathExp, (StorageExistsParam){VAR_PARAM_INIT, __VA_ARGS__})

bool storageExists(const Storage *this, const String *pathExp, StorageExistsParam param);

// Read from storage into a buffer
typedef struct StorageGetParam
{
    VAR_PARAM_HEADER;
    size_t exactSize;
} StorageGetParam;

#define storageGetP(file, ...)                                                                                                     \
    storageGet(file, (StorageGetParam){VAR_PARAM_INIT, __VA_ARGS__})

Buffer *storageGet(StorageRead *file, StorageGetParam param);

// File/path info
typedef struct StorageInfoParam
{
    VAR_PARAM_HEADER;
    StorageInfoLevel level;
    bool ignoreMissing;
    bool followLink;
    bool noPathEnforce;
} StorageInfoParam;

#define storageInfoP(this, fileExp, ...)                                                                                           \
    storageInfo(this, fileExp, (StorageInfoParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageInfo storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param);

// Iterator for all files/links/paths in a path which returns different info based on the value of the level parameter
typedef struct StorageNewItrParam
{
    VAR_PARAM_HEADER;
    StorageInfoLevel level;
    bool errorOnMissing;
    bool nullOnMissing;
    bool recurse;
    SortOrder sortOrder;
    const String *expression;
} StorageNewItrParam;

#define storageNewItrP(this, fileExp, ...)                                                                                         \
    storageNewItr(this, fileExp, (StorageNewItrParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageIterator *storageNewItr(const Storage *this, const String *pathExp, StorageNewItrParam param);

// Get a list of files from a directory
typedef struct StorageListParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;
    bool nullOnMissing;
    const String *expression;
} StorageListParam;

#define storageListP(this, pathExp, ...)                                                                                           \
    storageList(this, pathExp, (StorageListParam){VAR_PARAM_INIT, __VA_ARGS__})

StringList *storageList(const Storage *this, const String *pathExp, StorageListParam param);

// Move a file
#define storageMoveP(this, source, destination)                                                                                    \
    storageMove(this, source, destination)

void storageMove(const Storage *this, StorageRead *source, StorageWrite *destination);

// Open a file for reading
typedef struct StorageNewReadParam
{
    VAR_PARAM_HEADER;
    bool ignoreMissing;
    bool compressible;

    // Where to start reading in the file
    const uint64_t offset;

    // Limit bytes to read from the file (must be varTypeUInt64). NULL for no limit.
    const Variant *limit;
} StorageNewReadParam;

#define storageNewReadP(this, pathExp, ...)                                                                                        \
    storageNewRead(this, pathExp, (StorageNewReadParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageRead *storageNewRead(const Storage *this, const String *fileExp, StorageNewReadParam param);

// Open a file for writing
typedef struct StorageNewWriteParam
{
    VAR_PARAM_HEADER;
    bool noCreatePath;
    bool noSyncFile;
    bool noSyncPath;
    bool noAtomic;

    // Do not truncate file if it exists. Use this only in cases where the file will be manipulated directly through the file
    // handle, which should always be the exception and indicates functionality that should be added to the storage interface.
    bool noTruncate;

    bool compressible;
    mode_t modeFile;
    mode_t modePath;
    time_t timeModified;
    const String *user;
    const String *group;
} StorageNewWriteParam;

#define storageNewWriteP(this, pathExp, ...)                                                                                       \
    storageNewWrite(this, pathExp, (StorageNewWriteParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageWrite *storageNewWrite(const Storage *this, const String *fileExp, StorageNewWriteParam param);

// Get absolute path in the storage
typedef struct StoragePathParam
{
    VAR_PARAM_HEADER;
    bool noEnforce;
} StoragePathParam;

#define storagePathP(this, pathExp, ...)                                                                                                \
    storagePath(this, pathExp, (StoragePathParam){VAR_PARAM_INIT, __VA_ARGS__})

String *storagePath(const Storage *this, const String *pathExp, StoragePathParam param);

// Create a path
typedef struct StoragePathCreateParam
{
    VAR_PARAM_HEADER;
    bool errorOnExists;
    bool noParentCreate;
    mode_t mode;
} StoragePathCreateParam;

#define storagePathCreateP(this, pathExp, ...)                                                                                     \
    storagePathCreate(this, pathExp, (StoragePathCreateParam){VAR_PARAM_INIT, __VA_ARGS__})

void storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param);

// Does a path exist?
#define storagePathExistsP(this, pathExp)                                                                                          \
    storagePathExists(this, pathExp)

bool storagePathExists(const Storage *this, const String *pathExp);

// Remove a path
typedef struct StoragePathRemoveParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;
    bool recurse;
} StoragePathRemoveParam;

#define storagePathRemoveP(this, pathExp, ...)                                                                                     \
    storagePathRemove(this, pathExp, (StoragePathRemoveParam){VAR_PARAM_INIT, __VA_ARGS__})

void storagePathRemove(const Storage *this, const String *pathExp, StoragePathRemoveParam param);

// Sync a path
#define storagePathSyncP(this, pathExp)                                                                                            \
    storagePathSync(this, pathExp)

void storagePathSync(const Storage *this, const String *pathExp);

// Write a buffer to storage
#define storagePutP(file, buffer)                                                                                                  \
    storagePut(file, buffer)

void storagePut(StorageWrite *file, const Buffer *buffer);

// Remove a file
typedef struct StorageRemoveParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;
} StorageRemoveParam;

#define storageRemoveP(this, fileExp, ...)                                                                                         \
    storageRemove(this, fileExp, (StorageRemoveParam){VAR_PARAM_INIT, __VA_ARGS__})

void storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param);

// Create a hard or symbolic link
typedef struct StorageLinkCreateParam
{
    VAR_PARAM_HEADER;

    // Flag to create hard or symbolic link
    StorageLinkType linkType;
} StorageLinkCreateParam;

#define storageLinkCreateP(this, target, linkPath, ...)                                                                            \
    storageLinkCreate(this, target, linkPath, (StorageLinkCreateParam){VAR_PARAM_INIT, __VA_ARGS__})

void storageLinkCreate(const Storage *this, const String *target, const String *linkPath, StorageLinkCreateParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Is the feature supported by this storage?
FN_INLINE_ALWAYS bool
storageFeature(const Storage *const this, const StorageFeature feature)
{
    return THIS_PUB(Storage)->interface.feature >> feature & 1;
}

// Storage type (posix, cifs, etc.)
FN_INLINE_ALWAYS StringId
storageType(const Storage *const this)
{
    return THIS_PUB(Storage)->type;
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageToLog(const Storage *this);

#define FUNCTION_LOG_STORAGE_TYPE                                                                                                  \
    Storage *
#define FUNCTION_LOG_STORAGE_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageToLog, buffer, bufferSize)

#endif
