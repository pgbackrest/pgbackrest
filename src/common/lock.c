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
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "common/io/fdRead.h"
#include "common/io/fdWrite.h"
#include "common/io/io.h"
#include "common/lock.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/wait.h"
#include "storage/helper.h"
#include "storage/storage.intern.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Indicates a lock that was made by matching exec-id rather than holding an actual lock. This disguishes it from -1, which is a
// general system error.
#define LOCK_ON_EXEC_ID                                             -2

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
static struct LockLocal
{
    MemContext *memContext;                                         // Mem context for locks
    LockType held;                                                  // Current lock type held
    const String *execId;                                           // Process exec id

    struct
    {
        String *name;                                               // Name of lock file
        int fd;                                                     // File descriptor for lock file
    } file[lockTypeAll];
} lockLocal =
{
    .held = lockTypeNone,
};

/**********************************************************************************************************************************/
// Size of initial buffer used to load lock file
#define LOCK_BUFFER_SIZE                                            128

LockData
lockReadData(const String *const lockFile, const int fd)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(INT, fd);
    FUNCTION_LOG_END();

    ASSERT(lockFile != NULL);
    ASSERT(fd != -1);

    LockData result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read contents of file
        Buffer *const buffer = bufNew(LOCK_BUFFER_SIZE);
        IoWrite *const write = ioBufferWriteNewOpen(buffer);

        ioCopy(ioFdReadNewOpen(lockFile, fd, 0), write);
        ioWriteClose(write);

        // Parse the file
        const StringList *const parse = strLstNewSplitZ(strNewBuf(buffer), LF_Z);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            if (!strEmpty(strTrim(strLstGet(parse, 0))))
                result.processId = cvtZToInt(strZ(strLstGet(parse, 0)));

            if (strLstSize(parse) == 3)
                result.execId = strDup(strLstGet(parse, 1));
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Acquire a lock using a file on the local filesystem
***********************************************************************************************************************************/
static void
lockWriteData(const LockType lockType)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, lockType);
    FUNCTION_LOG_END();

    ASSERT(lockType < lockTypeAll);
    ASSERT(lockLocal.file[lockType].name != NULL);
    ASSERT(lockLocal.file[lockType].fd != -1);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        IoWrite *const write = ioFdWriteNewOpen(lockLocal.file[lockType].name, lockLocal.file[lockType].fd, 0);

        ioCopy(ioBufferReadNewOpen(BUFSTR(strNewFmt("%d" LF_Z "%s" LF_Z, getpid(), strZ(lockLocal.execId)))), write);
        ioWriteClose(write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Acquire a lock using a file on the local filesystem
***********************************************************************************************************************************/
static int
lockAcquireFile(const LockType lockType, const TimeMSec lockTimeout, const bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockType < lockTypeAll);
    ASSERT(lockLocal.file[lockType].name != NULL);

    int result = -1;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *wait = waitNew(lockTimeout);
        bool retry;
        int errNo = 0;

        do
        {
            // Assume there will be no retry
            retry = false;

            // Attempt to open the file
            if ((result = open(strZ(lockLocal.file[lockType].name), O_RDWR | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1)
            {
                // Save the error for reporting outside the loop
                errNo = errno;

                // If the path does not exist then create it
                if (errNo == ENOENT)
                {
                    storagePathCreateP(storageLocalWrite(), strPath(lockLocal.file[lockType].name));
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

                    // Even though we were unable to lock the file, it may be that it is already locked by another process with the
                    // same exec-id, i.e. spawned by the same original main process. If so, report the lock as successful.
                    TRY_BEGIN()
                    {
                        if (strEq(lockReadData(lockLocal.file[lockType].name, result).execId, lockLocal.execId))
                            result = LOCK_ON_EXEC_ID;
                        else
                            result = -1;
                    }
                    FINALLY()
                    {
                        // Close the file
                        close(result);
                    }
                    TRY_END();
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
                    errorHint = strNewZ("\nHINT: is another " PROJECT_NAME " process running?");
                else if (errNo == EACCES)
                {
                    errorHint = strNewFmt(
                        "\nHINT: does the user running " PROJECT_NAME " have permissions on the '%s' file?",
                        strZ(lockLocal.file[lockType].name));
                }

                THROW_FMT(
                    LockAcquireError, "unable to acquire lock on file '%s': %s%s", strZ(lockLocal.file[lockType].name),
                    strerror(errNo), errorHint == NULL ? "" : strZ(errorHint));
            }
        }
        // Else write lock data unless we locked an execId match
        else if (result != LOCK_ON_EXEC_ID)
        {
            lockLocal.file[lockType].fd = result;
            lockWriteData(lockType);
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
    ASSERT(lockFd >= 0);

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

    ASSERT(lockPath != NULL);
    ASSERT(stanza != NULL);
    ASSERT(execId != NULL);

    bool result = false;

    // Don't allow failures when locking more than one file.  This makes cleanup difficult and there are no known use cases.
    ASSERT(failOnNoLock || lockType != lockTypeAll);

    // Don't allow another lock if one is already held
    if (lockLocal.held != lockTypeNone)
        THROW(AssertError, "lock is already held by this process");

    // Allocate a mem context to hold lock filenames if one does not already exist
    if (lockLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN("Lock")
            {
                lockLocal.memContext = MEM_CONTEXT_NEW();
                lockLocal.execId = strDup(execId);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    // Exec id should never change
    ASSERT(strEq(execId, lockLocal.execId));

    // Lock files
    MEM_CONTEXT_BEGIN(lockLocal.memContext)
    {
        LockType lockMin = lockType == lockTypeAll ? lockTypeArchive : lockType;
        LockType lockMax = lockType == lockTypeAll ? (lockTypeAll - 1) : lockType;
        bool error = false;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
        {
            lockLocal.file[lockIdx].name = strNewFmt("%s/%s-%s" LOCK_FILE_EXT, strZ(lockPath), strZ(stanza), lockTypeName[lockIdx]);
            lockLocal.file[lockIdx].fd = lockAcquireFile(lockIdx, lockTimeout, failOnNoLock);

            if (lockLocal.file[lockIdx].fd == -1)
            {
                error = true;
                break;
            }
        }

        if (!error)
        {
            lockLocal.held = lockType;
            result = true;
        }
    }
    MEM_CONTEXT_END();

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

    if (lockLocal.held == lockTypeNone)
    {
        if (failOnNoLock)
            THROW(AssertError, "no lock is held by this process");
    }
    else
    {
        // Release locks
        LockType lockMin = lockLocal.held == lockTypeAll ? lockTypeArchive : lockLocal.held;
        LockType lockMax = lockLocal.held == lockTypeAll ? (lockTypeAll - 1) : lockLocal.held;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
        {
            if (lockLocal.file[lockIdx].fd != LOCK_ON_EXEC_ID)
                lockReleaseFile(lockLocal.file[lockIdx].fd, lockLocal.file[lockIdx].name);

            strFree(lockLocal.file[lockIdx].name);
        }

        // Free the lock context and reset lock data
        memContextFree(lockLocal.memContext);
        lockLocal = (struct LockLocal){.held = lockTypeNone};

        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
