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
VARIANT_STRDEF_STATIC(PERCENT_COMPLETE_VAR,                         "percentComplete");

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
String *
lockFileName(const String *const stanza, const LockType lockType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(ENUM, lockType);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(strNewFmt("%s-%s" LOCK_FILE_EXT, strZ(stanza), lockTypeName[lockType]));
}

/***********************************************************************************************************************************
Read contents of lock file

If a seek is required to get to the beginning of the data, that must be done before calling this function.

??? This function should not be extern'd, but need to fix dependency in cmdStop().
***********************************************************************************************************************************/
// Size of initial buffer used to load lock file
#define LOCK_BUFFER_SIZE                                            128

// Helper to read data
LockData
lockReadDataFile(const String *const lockFile, const int fd)
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

        ioCopyP(ioFdReadNewOpen(lockFile, fd, 0), write);
        ioWriteClose(write);

        // Parse the file
        const StringList *const parse = strLstNewSplitZ(strNewBuf(buffer), LF_Z);

        // Parse process id
        if (!strEmpty(strTrim(strLstGet(parse, 0))))
            result.processId = cvtZToInt(strZ(strLstGet(parse, 0)));

        // Populate result if the data exists
        if (strLstSize(parse) == 3)
        {
            // Convert json to key value store
            KeyValue *kv = jsonToKv(strLstGet(parse, 1));

            // Get percentComplete if it exists
            const String *percentCompleteStr = varStr(kvGet(kv, PERCENT_COMPLETE_VAR));

            // Populate result
            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.execId = strDup(varStr(kvGet(kv, EXEC_ID_VAR)));

                if (percentCompleteStr != NULL)
                {
                    result.percentComplete = memNew(sizeof(double));
                    *result.percentComplete = cvtZToDouble(strZ(percentCompleteStr));
                }
            }
            MEM_CONTEXT_PRIOR_END();
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Write contents of lock file
***********************************************************************************************************************************/
void
lockWriteData(const LockType lockType, const LockWriteDataParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM_P(DOUBLE, param.percentComplete);
    FUNCTION_LOG_END();

    ASSERT(lockType < lockTypeAll);
    ASSERT(lockLocal.file[lockType].name != NULL);
    ASSERT(lockLocal.file[lockType].fd != -1);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build key value store for second line json
        KeyValue *keyValue = kvNew();
        kvPut(keyValue, EXEC_ID_VAR, varNewStr(lockLocal.execId));

        if (param.percentComplete != NULL)
            kvPut(keyValue, PERCENT_COMPLETE_VAR, varNewStr(strNewFmt("%.2f", *param.percentComplete)));

        if (lockType == lockTypeBackup && lockLocal.held != lockTypeNone)
        {
            // Seek to beginning of backup lock file
            THROW_ON_SYS_ERROR_FMT(
                lseek(lockLocal.file[lockType].fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0,
                strZ(lockLocal.file[lockType].name));

            // In case the current write is ever shorter than the previous one
            THROW_ON_SYS_ERROR_FMT(
                ftruncate(lockLocal.file[lockType].fd, 0) == -1, FileWriteError, "unable to truncate '%s'",
                strZ(lockLocal.file[lockType].name));
        }

        // Write lock file data
        IoWrite *const write = ioFdWriteNewOpen(lockLocal.file[lockType].name, lockLocal.file[lockType].fd, 0);

        ioCopyP(ioBufferReadNewOpen(BUFSTR(strNewFmt("%d" LF_Z "%s" LF_Z, getpid(), strZ(jsonFromKv(keyValue))))), write);
        ioWriteClose(write);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to acquire a file lock
static int
lockAcquireFile(const String *const lockFile, const TimeMSec lockTimeout, const bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(TIMEMSEC, lockTimeout);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockFile != NULL);

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

                    // Get execId from lock file and close it
                    const String *execId = NULL;

                    TRY_BEGIN()
                    {
                        execId = lockReadDataFile(lockFile, result).execId;
                    }
                    FINALLY()
                    {
                        close(result);
                    }
                    TRY_END();

                    // Even though we were unable to lock the file, it may be that it is already locked by another process with the
                    // same exec-id, i.e. spawned by the same original main process. If so, report the lock as successful.
                    if (strEq(execId, lockLocal.execId))
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
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}

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

    bool result = true;

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
    LockType lockMin = lockType == lockTypeAll ? lockTypeArchive : lockType;
    LockType lockMax = lockType == lockTypeAll ? (lockTypeAll - 1) : lockType;

    for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
    {
        MEM_CONTEXT_BEGIN(lockLocal.memContext)
        {
            lockLocal.file[lockIdx].name = strNewFmt("%s/%s", strZ(lockPath), strZ(lockFileName(stanza, lockIdx)));
        }
        MEM_CONTEXT_END();

        lockLocal.file[lockIdx].fd = lockAcquireFile(lockLocal.file[lockIdx].name, lockTimeout, failOnNoLock);

        if (lockLocal.file[lockIdx].fd == -1)
        {
            // Free the lock context and reset lock data
            memContextFree(lockLocal.memContext);
            lockLocal = (struct LockLocal){.held = lockTypeNone};

            result = false;
            break;
        }
        // Else write lock data unless we locked an execId match
        else if (lockLocal.file[lockIdx].fd != LOCK_ON_EXEC_ID)
            lockWriteDataP(lockIdx);
    }

    if (result)
        lockLocal.held = lockType;

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
// Helper to release a file lock
static void
lockReleaseFile(const int lockFd, const String *const lockFile)
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
        }

        // Free the lock context and reset lock data
        memContextFree(lockLocal.memContext);
        lockLocal = (struct LockLocal){.held = lockTypeNone};

        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
