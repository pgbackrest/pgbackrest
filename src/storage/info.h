/***********************************************************************************************************************************
Storage Info
***********************************************************************************************************************************/
#ifndef STORAGE_INFO_H
#define STORAGE_INFO_H

#include <sys/types.h>

/***********************************************************************************************************************************
Storage type
***********************************************************************************************************************************/
typedef enum
{
    storageTypeFile,
    storageTypePath,
    storageTypeLink,
    storageTypeSpecial,
} StorageType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageInfo
{
    const String *name;                                             // Name of path/file/link
    bool exists;                                                    // Does the path/file/link exist?
    StorageType type;                                               // Type file/path/link)
    uint64_t size;                                                  // Size (path/link is 0)
    time_t timeModified;                                            // Time file was last modified
    mode_t mode;                                                    // Mode of path/file/link
    uid_t userId;                                                   // User that owns the file
    uid_t groupId;                                                  // Group that owns the file
    const String *user;                                             // Name of user that owns the file
    const String *group;                                            // Name of group that owns the file
    const String *linkDestination;                                  // Destination if this is a link
} StorageInfo;

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_INFO_TYPE                                                                                             \
    StorageInfo
#define FUNCTION_LOG_STORAGE_INFO_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(&value, "StorageInfo", buffer, bufferSize)

#endif
