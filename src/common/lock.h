/***********************************************************************************************************************************
Lock Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOCK_H
#define COMMON_LOCK_H

/***********************************************************************************************************************************
Lock types
***********************************************************************************************************************************/
typedef enum
{
    lockTypeArchive,
    lockTypeBackup,
    lockTypeAll,
    lockTypeNone,
} LockType;

#include "common/type/string.h"

// Lock data
typedef struct LockData
{
    pid_t processId;                                                // Process holding the lock
    const String *execId;                                           // Exec id of process holding the lock
    double *percentComplete;                                        // Percentage of backup complete (when not NULL)
} LockData;

#include "common/time.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define LOCK_FILE_EXT                                               ".lock"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Acquire a lock type. This will involve locking one or more files on disk depending on the lock type.  Most operations only take a
// single lock (archive or backup), but the stanza commands all need to lock both.
bool lockAcquire(
    const String *lockPath, const String *stanza, const String *execId, LockType lockType, TimeMSec lockTimeout, bool failOnNoLock);

// Release a lock
bool lockRelease(bool failOnNoLock);

// Build lock file name
String *lockFileName(const String *stanza, LockType lockType);

// Write data to a lock file
typedef struct LockWriteDataParam
{
    VAR_PARAM_HEADER;
    double *percentComplete;                                        // Percentage of backup complete
} LockWriteDataParam;

#define lockWriteDataP(lockType, ...)                                                                                              \
    lockWriteData(lockType, (LockWriteDataParam) {VAR_PARAM_INIT, __VA_ARGS__})

void lockWriteData(LockType lockType, LockWriteDataParam param);
// Read a lock file held by another process to get information about what the process is doing. This is a lower-level version to use
// when the lock file name is already known and the lock file may need to be removed.
typedef enum
{
    lockReadStatusMissing,                                          // File is missing
    lockReadStatusUnlocked,                                         // File is not locked
    lockReadStatusInvalid,                                          // File contents are invalid
    lockReadStatusValid,                                            // File is locked and contexts are valid
} LockReadStatus;

typedef struct LockReadResult
{
    LockReadStatus status;                                          // Status of file read
    LockData data;                                                  // Lock data
} LockReadResult;

typedef struct LockReadFileParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove the lock file after locking/reading
} LockReadFileParam;

#define lockReadFileP(lockFile, ...)                                                                                               \
    lockReadFile(lockFile, (LockReadFileParam){VAR_PARAM_INIT, __VA_ARGS__})

LockReadResult lockReadFile(const String *lockFile, LockReadFileParam param);

// Wrapper that generates the lock filename before calling lockReadFile()
LockReadResult lockRead(const String *lockPath, const String *stanza, LockType lockType);

#endif
