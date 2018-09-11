/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <sys/types.h>

/***********************************************************************************************************************************
Storage object
***********************************************************************************************************************************/
typedef struct Storage Storage;

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "common/io/filter/group.h"
#include "storage/fileRead.h"
#include "storage/fileWrite.h"
#include "storage/info.h"

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_FILE_MODE_DEFAULT                                   0640
#define STORAGE_PATH_MODE_DEFAULT                                   0750

/***********************************************************************************************************************************
Path expression callback function type - used to modify paths based on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
storageNew
***********************************************************************************************************************************/
typedef struct StorageNewParam
{
    mode_t modeFile;
    mode_t modePath;
    bool write;
    StoragePathExpressionCallback pathExpressionFunction;
} StorageNewParam;

#define storageNewP(path, ...)                                                                                                     \
    storageNew(path, (StorageNewParam){__VA_ARGS__})
#define storageNewNP(path)                                                                                                         \
    storageNew(path, (StorageNewParam){0})

Storage *storageNew(const String *path, StorageNewParam param);

/***********************************************************************************************************************************
storageCopy
***********************************************************************************************************************************/
#define storageCopyNP(source, destination)                                                                                         \
    storageCopy(source, destination)

bool storageCopy(StorageFileRead *source, StorageFileWrite *destination);

/***********************************************************************************************************************************
storageExists
***********************************************************************************************************************************/
typedef struct StorageExistsParam
{
    double timeout;
} StorageExistsParam;

#define storageExistsP(this, pathExp, ...)                                                                                         \
    storageExists(this, pathExp, (StorageExistsParam){__VA_ARGS__})
#define storageExistsNP(this, pathExp)                                                                                             \
    storageExists(this, pathExp, (StorageExistsParam){0})

bool storageExists(const Storage *this, const String *pathExp, StorageExistsParam param);

/***********************************************************************************************************************************
storageGet
***********************************************************************************************************************************/
typedef struct StorageGetParam
{
    size_t exactSize;
} StorageGetParam;

#define storageGetP(file, ...)                                                                                               \
    storageGet(file, (StorageGetParam){__VA_ARGS__})
#define storageGetNP(file)                                                                                                         \
    storageGet(file, (StorageGetParam){0})

Buffer *storageGet(StorageFileRead *file, StorageGetParam param);

/***********************************************************************************************************************************
storageInfo
***********************************************************************************************************************************/
typedef struct StorageInfoParam
{
    bool ignoreMissing;
} StorageInfoParam;

#define storageInfoP(this, fileExp, ...)                                                                                           \
    storageInfo(this, fileExp, (StorageInfoParam){__VA_ARGS__})
#define storageInfoNP(this, fileExp)                                                                                               \
    storageInfo(this, fileExp, (StorageInfoParam){0})

StorageInfo storageInfo(const Storage *this, const String *fileExp, StorageInfoParam param);

/***********************************************************************************************************************************
storageList
***********************************************************************************************************************************/
typedef struct StorageListParam
{
    bool errorOnMissing;
    const String *expression;
} StorageListParam;

#define storageListP(this, pathExp, ...)                                                                                           \
    storageList(this, pathExp, (StorageListParam){__VA_ARGS__})
#define storageListNP(this, pathExp)                                                                                               \
    storageList(this, pathExp, (StorageListParam){0})

StringList *storageList(const Storage *this, const String *pathExp, StorageListParam param);

/***********************************************************************************************************************************
storageMove
***********************************************************************************************************************************/
#define storageMoveNP(source, destination)                                                                                         \
    storageMove(source, destination)

void storageMove(StorageFileRead *source, StorageFileWrite *destination);

/***********************************************************************************************************************************
storageNewRead
***********************************************************************************************************************************/
typedef struct StorageNewReadParam
{
    bool ignoreMissing;
    IoFilterGroup *filterGroup;
} StorageNewReadParam;

#define storageNewReadP(this, pathExp, ...)                                                                                        \
    storageNewRead(this, pathExp, (StorageNewReadParam){__VA_ARGS__})
#define storageNewReadNP(this, pathExp)                                                                                            \
    storageNewRead(this, pathExp, (StorageNewReadParam){0})

StorageFileRead *storageNewRead(const Storage *this, const String *fileExp, StorageNewReadParam param);

/***********************************************************************************************************************************
storageNewWrite
***********************************************************************************************************************************/
typedef struct StorageNewWriteParam
{
    mode_t modeFile;
    mode_t modePath;
    bool noCreatePath;
    bool noSyncFile;
    bool noSyncPath;
    bool noAtomic;
    IoFilterGroup *filterGroup;
} StorageNewWriteParam;

#define storageNewWriteP(this, pathExp, ...)                                                                                       \
    storageNewWrite(this, pathExp, (StorageNewWriteParam){__VA_ARGS__})
#define storageNewWriteNP(this, pathExp)                                                                                           \
    storageNewWrite(this, pathExp, (StorageNewWriteParam){0})

StorageFileWrite *storageNewWrite(const Storage *this, const String *fileExp, StorageNewWriteParam param);

/***********************************************************************************************************************************
storagePath
***********************************************************************************************************************************/
#define storagePathNP(this, pathExp)                                                                                               \
    storagePath(this, pathExp)

String *storagePath(const Storage *this, const String *pathExp);

/***********************************************************************************************************************************
storagePathCreate
***********************************************************************************************************************************/
typedef struct StoragePathCreateParam
{
    bool errorOnExists;
    bool noParentCreate;
    mode_t mode;
} StoragePathCreateParam;

#define storagePathCreateP(this, pathExp, ...)                                                                                     \
    storagePathCreate(this, pathExp, (StoragePathCreateParam){__VA_ARGS__})
#define storagePathCreateNP(this, pathExp)                                                                                         \
    storagePathCreate(this, pathExp, (StoragePathCreateParam){0})

void storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param);

/***********************************************************************************************************************************
storagePathRemove
***********************************************************************************************************************************/
typedef struct StoragePathRemoveParam
{
    bool errorOnMissing;
    bool recurse;
} StoragePathRemoveParam;

#define storagePathRemoveP(this, pathExp, ...)                                                                                     \
    storagePathRemove(this, pathExp, (StoragePathRemoveParam){__VA_ARGS__})
#define storagePathRemoveNP(this, pathExp)                                                                                         \
    storagePathRemove(this, pathExp, (StoragePathRemoveParam){0})

void storagePathRemove(const Storage *this, const String *pathExp, StoragePathRemoveParam param);

/***********************************************************************************************************************************
storagePathSync
***********************************************************************************************************************************/
typedef struct StoragePathSync
{
    bool ignoreMissing;
} StoragePathSyncParam;

#define storagePathSyncP(this, pathExp, ...)                                                                                       \
    storagePathSync(this, pathExp, (StoragePathSyncParam){__VA_ARGS__})
#define storagePathSyncNP(this, pathExp)                                                                                           \
    storagePathSync(this, pathExp, (StoragePathSyncParam){0})

void storagePathSync(const Storage *this, const String *pathExp, StoragePathSyncParam param);

/***********************************************************************************************************************************
storagePut
***********************************************************************************************************************************/
#define storagePutNP(file, buffer)                                                                                                 \
    storagePut(file, buffer)

void storagePut(StorageFileWrite *file, const Buffer *buffer);

/***********************************************************************************************************************************
storageRemove
***********************************************************************************************************************************/
typedef struct StorageRemoveParam
{
    bool errorOnMissing;
} StorageRemoveParam;

#define storageRemoveP(this, fileExp, ...)                                                                                         \
    storageRemove(this, fileExp, (StorageRemoveParam){__VA_ARGS__})
#define storageRemoveNP(this, fileExp)                                                                                             \
    storageRemove(this, fileExp, (StorageRemoveParam){0})

void storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param);

/***********************************************************************************************************************************
storageFree
***********************************************************************************************************************************/
#define storageFreeNP(this)                                                                                                        \
    storageFree(this)

void storageFree(const Storage *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *storageToLog(const Storage *this);

#define FUNCTION_DEBUG_STORAGE_TYPE                                                                                                \
    Storage *
#define FUNCTION_DEBUG_STORAGE_FORMAT(value, buffer, bufferSize)                                                                   \
    FUNCTION_DEBUG_STRING_OBJECT_FORMAT(value, storageToLog, buffer, bufferSize)

#endif
