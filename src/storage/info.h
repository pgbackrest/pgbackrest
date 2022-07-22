/***********************************************************************************************************************************
Storage Info
***********************************************************************************************************************************/
#ifndef STORAGE_INFO_H
#define STORAGE_INFO_H

#include <sys/types.h>

/***********************************************************************************************************************************
Specify the level of information required when calling functions that return StorageInfo

Each level includes the level below it, i.e. level storageInfoLevelBasic includes storageInfoLevelType which includes
storageInfoLevelExists.
***********************************************************************************************************************************/
typedef enum
{
    // The info type is determined by driver capabilities. This mimics the prior behavior where drivers would always return as much
    // information as they could.
    storageInfoLevelDefault = 0,

    // Only test for existence. All drivers must support this level.
    storageInfoLevelExists,

    // Include file type, e.g. storageTypeFile. All drivers must support this level.
    storageInfoLevelType,

    // Basic information. All drivers support this level.
    storageInfoLevelBasic,

    // Detailed information that is generally only available from filesystems such as Posix
    storageInfoLevelDetail,
} StorageInfoLevel;

/***********************************************************************************************************************************
Storage types. The values are used in the remote protocol so must not be changed.
***********************************************************************************************************************************/
typedef enum
{
    storageTypeFile     = 0,
    storageTypePath     = 1,
    storageTypeLink     = 2,
    storageTypeSpecial  = 3,
} StorageType;

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageInfo
{
    // Set when info type >= storageInfoLevelExists
    const String *name;                                             // Name of path/file/link
    StorageInfoLevel level;                                         // Level of information provided
    bool exists;                                                    // Does the path/file/link exist?

    // Set when info type >= storageInfoLevelType (undefined at lower levels)
    StorageType type;                                               // Type file/path/link)

    // Set when info type >= storageInfoLevelBasic (undefined at lower levels)
    uint64_t size;                                                  // Size (path/link is 0)
    time_t timeModified;                                            // Time file was last modified

    // Set when info type >= storageInfoLevelDetail (undefined at lower levels)
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
