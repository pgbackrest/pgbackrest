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
#include "common/type/json.h"
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
VARIANT_STRDEF_STATIC(EXEC_ID_VAR,                                  "execId");
VARIANT_STRDEF_STATIC(PERCENTAGE_COMPLETE_VAR,                      "percentageComplete");

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
lockAcquireFile(const String *lockFile, const String *execId, TimeMSec lockTimeout, bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(STRING, execId);
        FUNCTION_LOG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockFile != NULL);
    ASSERT(execId != NULL);

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
            if ((result = open(strZ(lockFile), O_RDWR | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1)
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

                    // Even though we were unable to lock the file, it may be that it is already locked by another process with the
                    // same exec-id, i.e. spawned by the same original main process. If so, report the lock as successful.
                    char buffer[LOCK_BUFFER_SIZE];

                    // Read from file
                    ssize_t actualBytes = read(result, buffer, sizeof(buffer));

                    // Close the file
                    close(result);

                    // Make sure the read was successful. The file is already open and the chance of a failed read seems pretty
                    // remote so don't integrate this with the rest of the lock error handling.
                    THROW_ON_SYS_ERROR_FMT(actualBytes == -1, FileReadError, "unable to read '%s", strZ(lockFile));

                    // Parse the file and see if the exec id matches
                    const StringList *parse = strLstNewSplitZ(strNewZN(buffer, (size_t)actualBytes), LF_Z);

                    if (strLstSize(parse) == 3 && strEq(varStr(kvGet(jsonToKv(strLstGet(parse,1)), EXEC_ID_VAR)), execId))
                        result = LOCK_ON_EXEC_ID;
                    else
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
                    errorHint = strNewZ("\nHINT: is another " PROJECT_NAME " process running?");
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
        else if (result != LOCK_ON_EXEC_ID)
        {
            // Prepare kv store to write as json
            KeyValue *keyValue = kvNew();
            kvPut(keyValue, EXEC_ID_VAR, varNewStr(execId));

            // Write pid and execID of the current process
            ioFdWriteOneStr(result, strNewFmt("%d" LF_Z "%s" LF_Z, getpid(), strZ(jsonFromKv(keyValue))));
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

            lockFd[lockIdx] = lockAcquireFile(lockFile[lockIdx], execId, lockTimeout, failOnNoLock);

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
            if (lockFd[lockIdx] != LOCK_ON_EXEC_ID)
                lockReleaseFile(lockFd[lockIdx], lockFile[lockIdx]);

            strFree(lockFile[lockIdx]);
        }

        lockTypeHeld = lockTypeNone;
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
bool
writeLockPercentageComplete(const String *lockPath, const String *stanza, const String *execId, const double percentageComplete)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
    FUNCTION_LOG_PARAM(STRING, stanza);
    FUNCTION_LOG_PARAM(STRING, execId);
    FUNCTION_LOG_PARAM(DOUBLE, percentageComplete);
    FUNCTION_LOG_END();

    ASSERT(lockPath != NULL);
    ASSERT(stanza != NULL);
    ASSERT(execId != NULL);

    bool status = false;

    // This context should already exist, noop if it doesn't
    if (lockMemContext != NULL)
    {
        MEM_CONTEXT_BEGIN(lockMemContext)
        {
            // We are only interested in the backup lock file
            const String *const lockFPtr = strNewFmt(
                    "%s/%s-%s" LOCK_FILE_EXT, strZ(lockPath), strZ(stanza), lockTypeName[lockTypeBackup]);

            // If backup lock file exists, locate the file descriptor and update the content of the file.
            if (storageExistsP(storageLocal(), lockFPtr))
            {
                LockType lockMin = lockTypeHeld == lockTypeAll ? lockTypeArchive : lockTypeHeld;
                LockType lockMax = lockTypeHeld == lockTypeAll ? (lockTypeAll - 1) : lockTypeHeld;
                for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
                {
                    if (strEq(lockFile[lockIdx], lockFPtr))
                    {
                        // Build kv for second line json
                        KeyValue *keyValue = kvNew();
                        kvPut(keyValue, EXEC_ID_VAR, varNewStr(execId));
                        const String *const percentageCompleteStr = strNewFmt("%.2lf", percentageComplete);
                        kvPut(keyValue, PERCENTAGE_COMPLETE_VAR, varNewStr(percentageCompleteStr));

                        // Move to the beginning of the file, and overwrite the contents with the udpated data
                        lseek(lockFd[lockIdx], 0, SEEK_SET);
                        ioFdWriteOneStr(lockFd[lockIdx], strNewFmt("%d" LF_Z "%s" LF_Z, getpid(), strZ(jsonFromKv(keyValue))));

                        status = true;
                    }
                }
            }
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, status);
}
