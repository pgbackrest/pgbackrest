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
Path expression callback function type - used to modify paths based on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef bool (*StorageInterfaceExists)(void *driver, const String *path);
typedef StorageInfo (*StorageInterfaceInfo)(void *driver, const String *file, bool ignoreMissing);
typedef StringList *(*StorageInterfaceList)(void *driver, const String *path, bool errorOnMissing, const String *expression);
typedef bool (*StorageInterfaceMove)(void *driver, void *source, void *destination);
typedef StorageFileRead *(*StorageInterfaceNewRead)(void *driver, const String *file, bool ignoreMissing);
typedef StorageFileWrite *(*StorageInterfaceNewWrite)(
    void *driver, const String *file, mode_t modeFile, mode_t modePath, bool createPath, bool syncFile, bool syncPath, bool atomic);
typedef void (*StorageInterfacePathCreate)(
    void *driver, const String *path, bool errorOnExists, bool noParentCreate, mode_t mode);
typedef void (*StorageInterfacePathRemove)(void *driver, const String *path, bool errorOnMissing, bool recurse);
typedef void (*StorageInterfacePathSync)(void *driver, const String *path, bool ignoreMissing);
typedef void (*StorageInterfaceRemove)(void *driver, const String *file, bool errorOnMissing);

typedef struct StorageInterface
{
    StorageInterfaceExists exists;
    StorageInterfaceInfo info;
    StorageInterfaceList list;
    StorageInterfaceMove move;
    StorageInterfaceNewRead newRead;
    StorageInterfaceNewWrite newWrite;
    StorageInterfacePathCreate pathCreate;
    StorageInterfacePathRemove pathRemove;
    StorageInterfacePathSync pathSync;
    StorageInterfaceRemove remove;
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
