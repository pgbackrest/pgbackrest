/***********************************************************************************************************************************
Storage Info
***********************************************************************************************************************************/
#ifndef STORAGE_INFO_H
#define STORAGE_INFO_H

#include <sys/types.h>

/***********************************************************************************************************************************
Specify the amount of information required when calling functions that return StorageInfo
***********************************************************************************************************************************/
typedef enum
{
    // The info type is determined by driver capabilities.  This mimics the prior behavior where drivers would always return as
    // much information as they could.
    storageInfoTypeDefault = 0,

    // Only test for existence.  All drivers support this type.
    storageInfoTypeExists,

    // Basic information.  All drivers support this type.
    storageInfoTypeBasic,

    // Detailed information that is generally only available from filesystems such as Posix
    storageInfoTypeDetail,
} StorageInfoType;

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
    // Set when info type >= storageInfoTypeExists
    const String *name;                                             // Name of path/file/link
    bool exists;                                                    // Does the path/file/link exist?

    // Set when info type >= storageInfoTypeBasic
    StorageType type;                                               // Type file/path/link)
    uint64_t size;                                                  // Size (path/link is 0)
    time_t timeModified;                                            // Time file was last modified

    // Set when info type >= storageInfoTypeDetail
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
