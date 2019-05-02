/***********************************************************************************************************************************
Storage File Write Internal
***********************************************************************************************************************************/
#ifndef STORAGE_FILEWRITE_INTERN_H
#define STORAGE_FILEWRITE_INTERN_H

#include "common/io/write.intern.h"
#include "storage/fileWrite.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PROJECT_BIN ".tmp"

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct StorageFileWriteInterface
{
    const String *type;
    const String *name;

    bool atomic;
    bool createPath;
    const String *group;                                            // Group that owns the file
    mode_t modeFile;
    mode_t modePath;
    bool syncFile;
    bool syncPath;
    time_t timeModified;                                            // Time file was last modified
    const String *user;                                             // User that owns the file

    IoWriteInterface ioInterface;
} StorageFileWriteInterface;

StorageFileWrite *storageFileWriteNew(void *driver, const StorageFileWriteInterface *interface);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void *storageFileWriteFileDriver(const StorageFileWrite *this);

#endif
