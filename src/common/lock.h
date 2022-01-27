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
#include "common/type/keyValue.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define LOCK_FILE_EXT                                               ".lock"

// Size of buffer used to load lock file
#define LOCK_BUFFER_SIZE                                            128

/***********************************************************************************************************************************
Data Types and Structures
***********************************************************************************************************************************/
// Structure representing data contained in the lock file's json
typedef struct LockJsonData
{
    const String *execId;                                           // Process execId
    const String *percentComplete;                                  // Backup percent complete
} LockJsonData;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Acquire a lock type. This will involve locking one or more files on disk depending on the lock type.  Most operations only take a
// single lock (archive or backup), but the stanza commands all need to lock both.
bool lockAcquire(
    const String *lockPath, const String *stanza, const String *execId, LockType lockType, TimeMSec lockTimeout, bool failOnNoLock);

// Release a lock
bool lockRelease(bool failOnNoLock);

// Write the backup percentage complete value into the backup lock file
void lockWritePercentComplete(const String *execId, double percentComplete);

// Return json data from the lock file
LockJsonData lockReadJson(void);

#endif
