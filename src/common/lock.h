/***********************************************************************************************************************************
Lock Handler
***********************************************************************************************************************************/
#ifndef COMMON_LOCK_H
#define COMMON_LOCK_H

#include <sys/types.h>

/***********************************************************************************************************************************
Lock data
***********************************************************************************************************************************/
#include "common/type/variant.h"

// Lock data
typedef struct LockData
{
    pid_t processId;                                                // Process holding the lock
    const String *execId;                                           // Exec id of process holding the lock
    const Variant *percentComplete;                                 // Percentage of backup complete * 100 (when not NULL)
    const Variant *sizeComplete;                                    // Completed size of the backup in bytes
    const Variant *size;                                            // Total size of the backup in bytes
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
FN_EXTERN void lockInit(const String *path, const String *execId);

// Acquire a lock
typedef struct LockAcquireParam
{
    VAR_PARAM_HEADER;
    bool returnOnNoLock;                                            // Return when no lock acquired (rather than throw an error)
} LockAcquireParam;

#define lockAcquireP(lockFileName, ...)                                                                                            \
    lockAcquire(lockFileName, (LockAcquireParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool lockAcquire(const String *lockFileName, LockAcquireParam param);

// Write data to a lock file
typedef struct LockWriteParam
{
    VAR_PARAM_HEADER;
    const Variant *percentComplete;                                 // Percentage of backup complete * 100 (when not NULL)
    const Variant *sizeComplete;                                    // Completed size of the backup in bytes
    const Variant *size;                                            // Total size of the backup in bytes
} LockWriteParam;

#define lockWriteP(lockFileName, ...)                                                                                              \
    lockWrite(lockFileName, (LockWriteParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void lockWrite(const String *lockFileName, LockWriteParam param);

// Read a lock file held by another process to get information about what the process is doing
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

typedef struct LockReadParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove the lock file after locking/reading
} LockReadParam;

#define lockReadP(lockFileName, ...)                                                                                               \
    lockRead(lockFileName, (LockReadParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN LockReadResult lockRead(const String *lockFileName, LockReadParam param);

// Release lock(s)
typedef struct LockReleaseParam
{
    VAR_PARAM_HEADER;
    bool returnOnNoLock;                                            // Return when no lock is held (rather than throw an error)
} LockReleaseParam;

#define lockReleaseP(...)                                                                                                          \
    lockRelease((LockReleaseParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool lockRelease(LockReleaseParam param);

#endif
