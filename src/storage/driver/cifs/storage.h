/***********************************************************************************************************************************
CIFS Storage Driver
***********************************************************************************************************************************/
#ifndef STORAGE_DRIVER_CIFS_STORAGE_H
#define STORAGE_DRIVER_CIFS_STORAGE_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageDriverCifs StorageDriverCifs;

#include <sys/types.h>

#include "common/type/buffer.h"
#include "common/type/stringList.h"
#include "storage/driver/posix/fileRead.h"
#include "storage/driver/posix/fileWrite.h"
#include "storage/info.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Driver type constant
***********************************************************************************************************************************/
#define STORAGE_DRIVER_CIFS_TYPE                                    "cifs"
    STRING_DECLARE(STORAGE_DRIVER_CIFS_TYPE_STR);

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
StorageDriverCifs *storageDriverCifsNew(
    const String *path, mode_t modeFile, mode_t modePath, bool write, StoragePathExpressionCallback pathExpressionFunction);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
StorageFileWrite *storageDriverCifsNewWrite(
    StorageDriverCifs *this, const String *file, mode_t modeFile, mode_t modePath, const String *user, const String *group,
    time_t timeModified, bool createPath, bool syncFile, bool syncPath, bool atomic);
void storageDriverCifsPathSync(StorageDriverCifs *this, const String *path, bool ignoreMissing);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
Storage *storageDriverCifsInterface(const StorageDriverCifs *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void storageDriverCifsFree(StorageDriverCifs *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_DRIVER_CIFS_TYPE                                                                                      \
    StorageDriverCifs *
#define FUNCTION_LOG_STORAGE_DRIVER_CIFS_FORMAT(value, buffer, bufferSize)                                                         \
    objToLog(value, "StorageDriverCifs", buffer, bufferSize)

#endif
