/***********************************************************************************************************************************
Storage Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_STORAGE_INTERN_H
#define STORAGE_STORAGE_INTERN_H

#include "storage/fileRead.intern.h"
#include "storage/fileWrite.intern.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_MODE_FILE_DEFAULT                                   0640
#define STORAGE_MODE_PATH_DEFAULT                                   0750

/***********************************************************************************************************************************
Path expression callback function type - used to modify paths based on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct StorageInterface
{
    bool (*exists)(void *driver, const String *path);
    StorageInfo (*info)(void *driver, const String *path, bool ignoreMissing, bool followLink);
    bool (*infoList)(void *driver, const String *file, bool ignoreMissing, StorageInfoListCallback callback, void *callbackData);
    StringList *(*list)(void *driver, const String *path, bool errorOnMissing, const String *expression);
    bool (*move)(void *driver, StorageFileRead *source, StorageFileWrite *destination);
    StorageFileRead *(*newRead)(void *driver, const String *file, bool ignoreMissing);
    StorageFileWrite *(*newWrite)(
        void *driver, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group,
        time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);
    void (*pathCreate)(void *driver, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
    void (*pathRemove)(void *driver, const String *path, bool errorOnMissing, bool recurse);
    void (*pathSync)(void *driver, const String *path, bool ignoreMissing);
    void (*remove)(void *driver, const String *file, bool errorOnMissing);
} StorageInterface;

#define storageNewP(type, path, modeFile, modePath, write, pathExpressionFunction, driver, ...)                                    \
    storageNew(type, path, modeFile, modePath, write, pathExpressionFunction, driver, (StorageInterface){__VA_ARGS__})

Storage *storageNew(
    const String *type, const String *path, mode_t modeFile, mode_t modePath, bool write,
    StoragePathExpressionCallback pathExpressionFunction, void *driver, StorageInterface interface);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
void *storageDriver(const Storage *this);
StorageInterface storageInterface(const Storage *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_INTERFACE_TYPE                                                                                        \
    StorageInterface
#define FUNCTION_LOG_STORAGE_INTERFACE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(&value, "StorageInterface", buffer, bufferSize)

#endif
