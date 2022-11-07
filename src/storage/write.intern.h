/***********************************************************************************************************************************
Storage Write Interface Internal
***********************************************************************************************************************************/
#ifndef STORAGE_WRITE_INTERN_H
#define STORAGE_WRITE_INTERN_H

#include "common/io/write.h"
#include "version.h"

/***********************************************************************************************************************************
Temporary file extension
***********************************************************************************************************************************/
#define STORAGE_FILE_TEMP_EXT                                       PROJECT_BIN ".tmp"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct StorageWriteInterface
{
    StringId type;                                                  // Storage type
    const String *name;

    bool atomic;
    bool truncate;                                                  // Truncate file if it exists
    bool createPath;
    bool compressible;                                              // Is this file compressible?
    unsigned int compressLevel;                                     // Level to use for compression
    const String *group;                                            // Group that owns the file
    mode_t modeFile;
    mode_t modePath;
    bool syncFile;
    bool syncPath;
    time_t timeModified;                                            // Time file was last modified
    const String *user;                                             // User that owns the file

    IoWriteInterface ioInterface;
} StorageWriteInterface;

StorageWrite *storageWriteNew(void *driver, const StorageWriteInterface *interface);

#endif
