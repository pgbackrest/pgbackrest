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
    int percentComplete;                                            // Percentage of backup complete
} LockData;

#include "common/time.h"
#include "common/type/keyValue.h"

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

typedef struct LockWriteDataParam
{
    VAR_PARAM_HEADER;
    int percentComplete;                                             // Percentage of backup complete
} LockWriteDataParam;

#define lockWriteDataP(lockType, ...)                                                                                              \
    lockWriteData(lockType, (LockWriteDataParam) {VAR_PARAM_INIT, __VA_ARGS__})

void lockWriteData(LockType lockType, LockWriteDataParam param);

#endif
