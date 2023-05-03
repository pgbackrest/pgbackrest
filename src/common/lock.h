/***********************************************************************************************************************************
Lock Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOCK_H
#define COMMON_LOCK_H

#include <sys/types.h>

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

#include "common/type/variant.h"

// Lock data
typedef struct LockData
{
    pid_t processId;                                                // Process holding the lock
    const String *execId;                                           // Exec id of process holding the lock
    Variant *percentComplete;                                       // Percentage of backup complete * 100 (when not NULL)
} LockData;

#include "common/time.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define LOCK_FILE_EXT                                               ".lock"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Initialize lock module
FN_EXTERN void lockInit(const String *path, const String *execId, const String *stanza, LockType type);

// Acquire a lock type. This will involve locking one or more files on disk depending on the lock type. Most operations only take a
// single lock (archive or backup), but the stanza commands all need to lock both.
typedef struct LockAcquireParam
{
    VAR_PARAM_HEADER;
    TimeMSec timeout;                                               // Lock timeout
    bool returnOnNoLock;                                            // Return when no lock acquired (rather than throw an error)
} LockAcquireParam;

#define lockAcquireP(...)                                                                                                          \
    lockAcquire((LockAcquireParam) {VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool lockAcquire(LockAcquireParam param);

// Release a lock
FN_EXTERN bool lockRelease(bool failOnNoLock);

// Build lock file name
FN_EXTERN String *lockFileName(const String *stanza, LockType lockType);

// Write data to a lock file
typedef struct LockWriteDataParam
{
    VAR_PARAM_HEADER;
    const Variant *percentComplete;                                 // Percentage of backup complete * 100 (when not NULL)
} LockWriteDataParam;

#define lockWriteDataP(lockType, ...)                                                                                              \
    lockWriteData(lockType, (LockWriteDataParam) {VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void lockWriteData(LockType lockType, LockWriteDataParam param);

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

FN_EXTERN LockReadResult lockReadFile(const String *lockFile, LockReadFileParam param);

// Wrapper that generates the lock filename before calling lockReadFile()
FN_EXTERN LockReadResult lockRead(const String *lockPath, const String *stanza, LockType lockType);

#endif
