/***********************************************************************************************************************************
Storage Interface
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define STORAGE_TYPE                                                Storage
#define STORAGE_PREFIX                                              storage

typedef struct Storage Storage;

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "common/io/filter/group.h"
#include "common/time.h"
#include "common/type/param.h"
#include "storage/info.h"
#include "storage/read.h"
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

    // Is the storage able to do compression and therefore store the file more efficiently than what was written?  If so, the size
    // will need to checked after write to see if it is different.
    storageFeatureCompress,

    // Does the storage support hardlinks?  Hardlinks allow the same file to be linked into multiple paths to save space.
    storageFeatureHardLink,

    // Can the storage limit the amount of data read from a file?
    storageFeatureLimitRead,

    // Does the storage support symlinks?  Symlinks allow paths/files/links to be accessed from another path.
    storageFeatureSymLink,
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
    bool ignoreMissing;
    bool followLink;
    bool noPathEnforce;
} StorageInfoParam;

#define storageInfoP(this, fileExp, ...)                                                                                           \
    storageInfo(this, fileExp, (StorageInfoParam){VAR_PARAM_INIT, __VA_ARGS__})

StorageInfo storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param);

// Info for all files/paths in a path
typedef void (*StorageInfoListCallback)(void *callbackData, const StorageInfo *info);

typedef struct StorageInfoListParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;
    bool recurse;
    SortOrder sortOrder;
    const String *expression;
} StorageInfoListParam;

#define storageInfoListP(this, fileExp, callback, callbackData, ...)                                                               \
    storageInfoList(this, fileExp, callback, callbackData, (StorageInfoListParam){VAR_PARAM_INIT, __VA_ARGS__})

bool storageInfoList(
    const Storage *this, const String *pathExp, StorageInfoListCallback callback, void *callbackData, StorageInfoListParam param);

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

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// Is the feature supported by this storage?
bool storageFeature(const Storage *this, StorageFeature feature);

// Storage type (posix, cifs, etc.)
const String *storageType(const Storage *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageToLog(const Storage *this);

#define FUNCTION_LOG_STORAGE_TYPE                                                                                                  \
    Storage *
#define FUNCTION_LOG_STORAGE_FORMAT(value, buffer, bufferSize)                                                                     \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, storageToLog, buffer, bufferSize)

#endif
