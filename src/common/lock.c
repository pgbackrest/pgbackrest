/***********************************************************************************************************************************
Lock Handler
***********************************************************************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "common/assert.h"
#include "common/debug.h"
#include "common/io/handle.h"
#include "common/lock.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"
#include "version.h"

/***********************************************************************************************************************************
Lock type names
***********************************************************************************************************************************/
static const char *lockTypeName[] =
{
    "archive",                                                      // lockTypeArchive
    "backup",                                                       // lockTypeBackup
};

/***********************************************************************************************************************************
Mem context and local variables
***********************************************************************************************************************************/
static MemContext *lockMemContext = NULL;
static String *lockFile[lockTypeAll];
static int lockHandle[lockTypeAll];
static LockType lockTypeHeld = lockTypeNone;

/***********************************************************************************************************************************
Acquire a lock using a file on the local filesystem
***********************************************************************************************************************************/
static int
lockAcquireFile(const String *lockFile, TimeMSec lockTimeout, bool failOnNoLock)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(STRING, lockFile);
        FUNCTION_DEBUG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_DEBUG_PARAM(BOOL, failOnNoLock);
    FUNCTION_DEBUG_END();

    int result = -1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *wait = lockTimeout != 0 ? waitNew(lockTimeout) : NULL;
        bool retry = false;
        int errNo = 0;

        do
        {
            // Attempt to open the file
            if ((result = open(strPtr(lockFile), O_WRONLY | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1)
            {
                // Save the error for reporting outside the loop
                errNo = errno;

                // If the path does not exist then create it
                if (errNo == ENOENT)
                {
                    storagePathCreateNP(storageLocalWrite(), strPath(lockFile));
                    retry = true;
                }
            }
            else
            {
                // Attempt to lock the file
                if (flock(result, LOCK_EX | LOCK_NB) == -1)
                {
                    // Save the error for reporting outside the loop
                    errNo = errno;

                    // Close the file and reset the handle
                    close(result);
                    result = -1;
                }
            }
        }
        while (result == -1 && ((wait != NULL && waitMore(wait)) || retry));

        waitFree(wait);

        // If the lock was not successful
        if (result == -1)
        {
            // Error when requested
            if (failOnNoLock)
            {
                const String *errorHint = NULL;

                if (errNo == EWOULDBLOCK)
                    errorHint = STRING_CONST("\nHINT: is another " PROJECT_NAME " process running?");
                else if (errNo == EACCES)
                {
                    errorHint = strNewFmt(
                        "\nHINT: does the user running " PROJECT_NAME " have permissions on the '%s' file?",
                        strPtr(lockFile));
                }

                THROW_FMT(
                    LockAcquireError, "unable to acquire lock on file '%s': %s%s",
                    strPtr(lockFile), strerror(errNo), errorHint == NULL ? "" : strPtr(errorHint));
            }
        }
        else
        {
            // Write pid of the current process
            ioHandleWriteOneStr(result, strNewFmt("%d\n", getpid()));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_DEBUG_RESULT(INT, result);
}

/***********************************************************************************************************************************
Release the current lock
***********************************************************************************************************************************/
static void
lockReleaseFile(int lockHandle, const String *lockFile)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(INT, lockHandle);
        FUNCTION_DEBUG_PARAM(STRING, lockFile);
    FUNCTION_DEBUG_END();

    // Can't release lock if there isn't one
    ASSERT_DEBUG(lockHandle != -1);

    // Remove file first and then close it to release the lock.  If we close it first then another process might grab the lock
    // right before the delete which means the file locked by the other process will get deleted.
    storageRemoveNP(storageLocalWrite(), lockFile);
    close(lockHandle);

    FUNCTION_DEBUG_RESULT_VOID();
}

/***********************************************************************************************************************************
Acquire a lock type

This will involve locking one or more files on disk depending on the lock type.  Most operations only take a single lock (archive or
backup), but the stanza commands all need to lock both.
***********************************************************************************************************************************/
bool
lockAcquire(const String *lockPath, const String *stanza, LockType lockType, TimeMSec lockTimeout, bool failOnNoLock)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(STRING, lockPath);
        FUNCTION_DEBUG_PARAM(STRING, stanza);
        FUNCTION_DEBUG_PARAM(ENUM, lockType);
        FUNCTION_DEBUG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_DEBUG_PARAM(BOOL, failOnNoLock);
    FUNCTION_DEBUG_END();

    bool result = false;

    // Don't allow failures when locking more than one file.  This makes cleanup difficult and there are no known use cases.
    ASSERT_DEBUG(failOnNoLock || lockType != lockTypeAll);

    // Don't allow another lock if one is already held
    if (lockTypeHeld != lockTypeNone)
        THROW(AssertError, "lock is already held by this process");

    // Allocate a mem context to hold lock filenames if one does not already exist
    if (lockMemContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            lockMemContext = memContextNew("Lock");
        }
        MEM_CONTEXT_END();
    }

    // Lock files
    MEM_CONTEXT_BEGIN(lockMemContext)
    {
        LockType lockMin = lockType == lockTypeAll ? lockTypeArchive : lockType;
        LockType lockMax = lockType == lockTypeAll ? (lockTypeAll - 1) : lockType;
        bool error = false;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
        {
            lockFile[lockIdx] = strNewFmt("%s/%s-%s.lock", strPtr(lockPath), strPtr(stanza), lockTypeName[lockIdx]);

            lockHandle[lockIdx] = lockAcquireFile(lockFile[lockIdx], lockTimeout, failOnNoLock);

            if (lockHandle[lockIdx] == -1)
            {
                error = true;
                break;
            }
        }

        if (!error)
        {
            lockTypeHeld = lockType;
            result = true;
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Clear the lock without releasing it.  This is used by a master process after it has spawned a child so the child can keep the lock
and the master process won't try to free it.
***********************************************************************************************************************************/
bool
lockClear(bool failOnNoLock)
{
    FUNCTION_DEBUG_BEGIN(logLevelTrace);
        FUNCTION_DEBUG_PARAM(BOOL, failOnNoLock);
    FUNCTION_DEBUG_END();

    bool result = false;

    if (lockTypeHeld == lockTypeNone)
    {
        if (failOnNoLock)
            THROW(AssertError, "no lock is held by this process");
    }
    else
    {
        // Clear locks
        LockType lockMin = lockTypeHeld == lockTypeAll ? lockTypeArchive : lockTypeHeld;
        LockType lockMax = lockTypeHeld == lockTypeAll ? (lockTypeAll - 1) : lockTypeHeld;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
            strFree(lockFile[lockIdx]);

        lockTypeHeld = lockTypeNone;
        result = true;
    }

    FUNCTION_DEBUG_RESULT(BOOL, result);
}

/***********************************************************************************************************************************
Release a lock type
***********************************************************************************************************************************/
bool
lockRelease(bool failOnNoLock)
{
    FUNCTION_DEBUG_BEGIN(logLevelDebug);
        FUNCTION_DEBUG_PARAM(BOOL, failOnNoLock);
    FUNCTION_DEBUG_END();

    bool result = false;

    if (lockTypeHeld == lockTypeNone)
    {
        if (failOnNoLock)
            THROW(AssertError, "no lock is held by this process");
    }
    else
    {
        // Release locks
        LockType lockMin = lockTypeHeld == lockTypeAll ? lockTypeArchive : lockTypeHeld;
        LockType lockMax = lockTypeHeld == lockTypeAll ? (lockTypeAll - 1) : lockTypeHeld;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
        {
            lockReleaseFile(lockHandle[lockIdx], lockFile[lockIdx]);
            strFree(lockFile[lockIdx]);
        }

        lockTypeHeld = lockTypeNone;
        result = true;
    }

    FUNCTION_DEBUG_RESULT(BOOL, result);
}
