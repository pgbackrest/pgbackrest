/***********************************************************************************************************************************
Storage Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_INTERN_H
#define STORAGE_STORAGE_INTERN_H

#include "storage/storage.h"

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_MODE_FILE_DEFAULT                                   0640
#define STORAGE_MODE_PATH_DEFAULT                                   0750

/***********************************************************************************************************************************
Interface
***********************************************************************************************************************************/
typedef bool (*StorageExistsFn)(const void *driver, const String *path);
typedef StorageInfo (*StorageInfoFn)(const void *driver, const String *file, bool ignoreMissing);
typedef StringList *(*StorageListFn)(const void *driver, const String *path, bool errorOnMissing, const String *expression);
typedef bool (*StorageMoveFn)(const void *driver, void *source, void *destination);
typedef StorageFileRead *(*StorageNewReadFn)(const void *driver, const String *file, bool ignoreMissing);
typedef StorageFileWrite *(*StorageNewWriteFn)(
    const void *driver, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath,
    bool atomic);
typedef void (*StoragePathCreateFn)(const void *driver, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
typedef void (*StoragePathRemoveFn)(const void *driver, const String *path, bool errorOnMissing, bool recurse);
typedef void (*StoragePathSyncFn)(const void *driver, const String *path, bool ignoreMissing);
typedef void (*StorageRemoveFn)(const void *driver, const String *file, bool errorOnMissing);

typedef struct StorageInterface
{
    StorageExistsFn exists;
    StorageInfoFn info;
    StorageListFn list;
    StorageMoveFn move;
    StorageNewReadFn newRead;
    StorageNewWriteFn newWrite;
    StoragePathCreateFn pathCreate;
    StoragePathRemoveFn pathRemove;
    StoragePathSyncFn pathSync;
    StorageRemoveFn remove;
} StorageInterface;

/***********************************************************************************************************************************
Path expression callback function type - used to modify paths based on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
Storage *storageNew(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, const void *driver, const StorageInterface *interface);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_STORAGE_INTERFACE_TYPE                                                                                      \
    StorageInterface *
#define FUNCTION_DEBUG_STORAGE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageInterface", buffer, bufferSize)

#endif
