/***********************************************************************************************************************************
Command Lock Handler
***********************************************************************************************************************************/
#ifndef COMMAND_LOCK_H
#define COMMAND_LOCK_H

#include "common/lock.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Acquire a command lock. This will involve locking one or more files on disk depending on the lock type. Most operations only
// acquire a single lock type (archive or backup), but the stanza commands all need to lock both.
#define cmdLockAcquireP(...)                                                                                                       \
    cmdLockAcquire((LockAcquireParam) {VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool cmdLockAcquire(LockAcquireParam param);

// Read a command lock file held by another process to get information about what the process is doing
FN_EXTERN LockReadResult cmdLockRead(LockType lockType, const String *stanza);

// Write data to command lock file
#define cmdLockWriteP(...)                                                                                                         \
    cmdLockWrite((LockWriteDataParam) {VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void cmdLockWrite(LockWriteDataParam param);

// Release command locks
FN_EXTERN bool cmdLockRelease(bool failOnNoLock);

#endif
