/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <sys/types.h>

#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Default buffer size

Generally buffer size should be pulled out of options but some storage is created without access to options.  In those cases we'll
assume they are not doing any heavy lifting and have a moderate default.
***********************************************************************************************************************************/
#define STORAGE_BUFFER_SIZE_DEFAULT                                 (64 * 1024)

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_FILE_MODE_DEFAULT                                   0640
#define STORAGE_PATH_MODE_DEFAULT                                   0750

/***********************************************************************************************************************************
Storage object
***********************************************************************************************************************************/
typedef struct Storage Storage;

#include "storage/file.h"

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
    size_t bufferSize;
    bool write;
    StoragePathExpressionCallback pathExpressionFunction;
} StorageNewParam;

#define storageNewP(path, ...)                                                                                                     \
    storageNew(path, (StorageNewParam){__VA_ARGS__})
#define storageNewNP(path)                                                                                                         \
    storageNew(path, (StorageNewParam){0})

Storage *storageNew(const String *path, StorageNewParam param);

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
#define storageGetNP(file)                                                                                                         \
    storageGet(file)

Buffer *storageGet(const StorageFile *file);

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
storageOpenRead
***********************************************************************************************************************************/
typedef struct StorageOpenReadParam
{
    bool ignoreMissing;
} StorageOpenReadParam;

#define storageOpenReadP(this, pathExp, ...)                                                                                       \
    storageOpenRead(this, pathExp, (StorageOpenReadParam){__VA_ARGS__})
#define storageOpenReadNP(this, pathExp)                                                                                           \
    storageOpenRead(this, pathExp, (StorageOpenReadParam){0})

StorageFile *storageOpenRead(const Storage *this, const String *fileExp, StorageOpenReadParam param);

/***********************************************************************************************************************************
storageOpenWrite
***********************************************************************************************************************************/
typedef struct StorageOpenWriteParam
{
    mode_t mode;
} StorageOpenWriteParam;

#define storageOpenWriteP(this, pathExp, ...)                                                                                      \
    storageOpenWrite(this, pathExp, (StorageOpenWriteParam){__VA_ARGS__})
#define storageOpenWriteNP(this, pathExp)                                                                                          \
    storageOpenWrite(this, pathExp, (StorageOpenWriteParam){0})

StorageFile *storageOpenWrite(const Storage *this, const String *fileExp, StorageOpenWriteParam param);

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
storagePut
***********************************************************************************************************************************/
#define storagePutNP(file, buffer)                                                                                                 \
    storagePut(file, buffer)

void storagePut(const StorageFile *file, const Buffer *buffer);

/***********************************************************************************************************************************
storageRemove
***********************************************************************************************************************************/
typedef struct StorageRemoveParam
{
    bool errorOnMissing;
} StorageRemoveParam;

#define storageRemoveP(this, pathExp, ...)                                                                                         \
    storageRemove(this, pathExp, (StorageRemoveParam){__VA_ARGS__})
#define storageRemoveNP(this, pathExp)                                                                                             \
    storageRemove(this, pathExp, (StorageRemoveParam){0})

void storageRemove(const Storage *this, const String *fileExp, StorageRemoveParam param);

/***********************************************************************************************************************************
storageStat
***********************************************************************************************************************************/
typedef struct StorageStat
{
    mode_t mode;
} StorageStat;

typedef struct StorageStatParam
{
    bool ignoreMissing;
} StorageStatParam;

#define storageStatP(this, pathExp, ...)                                                                                           \
    storageStat(this, pathExp, (StorageStatParam){__VA_ARGS__})
#define storageStatNP(this, pathExp)                                                                                               \
    storageStat(this, pathExp, (StorageStatParam){0})

StorageStat *storageStat(const Storage *this, const String *pathExp, StorageStatParam param);

/***********************************************************************************************************************************
storageFree
***********************************************************************************************************************************/
#define storageFreeNP(this)                                                                                                        \
    storageFree(this)

void storageFree(const Storage *this);

#endif
