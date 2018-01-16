/***********************************************************************************************************************************
Storage Manager
***********************************************************************************************************************************/
#ifndef STORAGE_LOCAL_H
#define STORAGE_LOCAL_H

#include "common/type/buffer.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Default file and path modes
***********************************************************************************************************************************/
#define STORAGE_FILE_MODE_DEFAULT                                   0640
#define STORAGE_PATH_MODE_DEFAULT                                   0750

/***********************************************************************************************************************************
Storage object
***********************************************************************************************************************************/
typedef struct Storage Storage;

/***********************************************************************************************************************************
Path expression callback function type - used to modify paths base on expressions enclosed in <>
***********************************************************************************************************************************/
typedef String *(*StoragePathExpressionCallback)(const String *expression, const String *path);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Storage *storageNew(const String *path, int mode, size_t bufferSize, StoragePathExpressionCallback pathExpressionFunction);

Buffer *storageGet(const Storage *storage, const String *fileExp, bool ignoreMissing);
StringList *storageList(const Storage *storage, const String *pathExp, const String *expression, bool ignoreMissing);
String *storagePath(const Storage *storage, const String *pathExp);
void storagePut(const Storage *storage, const String *fileExp, const Buffer *buffer);

#endif
