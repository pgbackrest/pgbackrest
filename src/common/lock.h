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

#endif
