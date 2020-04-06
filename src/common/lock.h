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
bool lockAcquire(const String *lockPath, const String *stanza, LockType lockType, TimeMSec lockTimeout, bool failOnNoLock);

// Clear the lock without releasing it.  This is used by a master process after it has spawned a child so the child can keep the
// lock and the master process won't try to free it.
bool lockClear(bool failOnNoLock);

// Release a lock
bool lockRelease(bool failOnNoLock);

#endif
