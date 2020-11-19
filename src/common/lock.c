/***********************************************************************************************************************************
Lock Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "common/debug.h"
#include "common/io/fdWrite.h"
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
static const char *const lockTypeName[] =
{
    "archive",                                                      // lockTypeArchive
    "backup",                                                       // lockTypeBackup
};

/***********************************************************************************************************************************
Mem context and local variables
***********************************************************************************************************************************/
static MemContext *lockMemContext = NULL;
static String *lockFile[lockTypeAll];
static int lockFd[lockTypeAll];
static LockType lockTypeHeld = lockTypeNone;

/***********************************************************************************************************************************
Acquire a lock using a file on the local filesystem
***********************************************************************************************************************************/
static int
lockAcquireFile(const String *lockFile, TimeMSec lockTimeout, bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    int result = -1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *wait = waitNew(lockTimeout);
        bool retry = false;
        int errNo = 0;

        do
        {
            // Attempt to open the file
            if ((result = open(strZ(lockFile), O_WRONLY | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1)
            {
                // Save the error for reporting outside the loop
                errNo = errno;

                // If the path does not exist then create it
                if (errNo == ENOENT)
                {
                    storagePathCreateP(storageLocalWrite(), strPath(lockFile));
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

                    // Close the file and reset the file descriptor
                    close(result);
                    result = -1;
                }
            }
        }
        while (result == -1 && (waitMore(wait) || retry));

        // If the lock was not successful
        if (result == -1)
        {
            // Error when requested
            if (failOnNoLock)
            {
                const String *errorHint = NULL;

                if (errNo == EWOULDBLOCK)
                    errorHint = strNew("\nHINT: is another " PROJECT_NAME " process running?");
                else if (errNo == EACCES)
                {
                    errorHint = strNewFmt(
                        "\nHINT: does the user running " PROJECT_NAME " have permissions on the '%s' file?", strZ(lockFile));
                }

                THROW_FMT(
                    LockAcquireError, "unable to acquire lock on file '%s': %s%s", strZ(lockFile), strerror(errNo),
                    errorHint == NULL ? "" : strZ(errorHint));
            }
        }
        else
        {
            // Write pid of the current process
            ioFdWriteOneStr(result, strNewFmt("%d\n", getpid()));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

/***********************************************************************************************************************************
Release the current lock
***********************************************************************************************************************************/
static void
lockReleaseFile(int lockFd, const String *lockFile)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INT, lockFd);
        FUNCTION_LOG_PARAM(STRING, lockFile);
    FUNCTION_LOG_END();

    // Can't release lock if there isn't one
    ASSERT(lockFd != -1);

    // Remove file first and then close it to release the lock.  If we close it first then another process might grab the lock
    // right before the delete which means the file locked by the other process will get deleted.
    storageRemoveP(storageLocalWrite(), lockFile);
    close(lockFd);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
bool
lockAcquire(
    const String *lockPath, const String *stanza, const String *execId, LockType lockType, TimeMSec lockTimeout, bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, lockPath);
        FUNCTION_LOG_PARAM(STRING, stanza);
        FUNCTION_LOG_PARAM(STRING, execId);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    bool result = false;

    // Don't allow failures when locking more than one file.  This makes cleanup difficult and there are no known use cases.
    ASSERT(failOnNoLock || lockType != lockTypeAll);

    // Don't allow another lock if one is already held
    if (lockTypeHeld != lockTypeNone)
        THROW(AssertError, "lock is already held by this process");

    // Allocate a mem context to hold lock filenames if one does not already exist
    if (lockMemContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("Lock")
            {
                lockMemContext = MEM_CONTEXT_NEW();
            }
            MEM_CONTEXT_NEW_END();
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
            lockFile[lockIdx] = strNewFmt("%s/%s-%s" LOCK_FILE_EXT, strZ(lockPath), strZ(stanza), lockTypeName[lockIdx]);

            lockFd[lockIdx] = lockAcquireFile(lockFile[lockIdx], lockTimeout, failOnNoLock);

            if (lockFd[lockIdx] == -1)
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

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
bool
lockClear(bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

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

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
bool
lockRelease(bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

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
            lockReleaseFile(lockFd[lockIdx], lockFile[lockIdx]);
            strFree(lockFile[lockIdx]);
        }

        lockTypeHeld = lockTypeNone;
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
