/***********************************************************************************************************************************
Command Lock Handler
***********************************************************************************************************************************/
#ifndef COMMAND_LOCK_H
#define COMMAND_LOCK_H

#include "common/lock.h"
#include "config/config.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get list of required locks
FN_EXTERN StringList *cmdLockList(void);

// Acquire a command lock. This will involve locking one or more files on disk depending on the lock type. Most operations only
// acquire a single lock type (archive or backup), but the stanza commands all need to lock both.
#define cmdLockAcquireP(...)                                                                                                       \
    cmdLockAcquire((LockAcquireParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool cmdLockAcquire(LockAcquireParam param);

// Write data to command lock file
#define cmdLockWriteP(...)                                                                                                         \
    cmdLockWrite((LockWriteParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN void cmdLockWrite(LockWriteParam param);

// Read a command lock file held by another process to get information about what the process is doing
FN_EXTERN LockReadResult cmdLockRead(LockType lockType, const String *stanza, unsigned int repoKey);

// Is a command lock held by this process's own exec id, i.e. is our own async process running? Async archive-get uses this to
// decide whether to wait for its own async process or fetch the segment synchronously (when the lock is free or held by a foreign
// process).
FN_EXTERN bool cmdLockOwn(void);

// Release command lock(s)
#define cmdLockReleaseP(...)                                                                                                       \
    cmdLockRelease((LockReleaseParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN bool cmdLockRelease(LockReleaseParam param);

#endif
