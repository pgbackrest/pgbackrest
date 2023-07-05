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
#include "common/type/json.h"
#include "common/user.h"
#include "common/wait.h"
#include "storage/posix/storage.h"
#include "storage/storage.intern.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Indicates a lock that was made by matching exec-id rather than holding an actual lock. This distinguishes it from -1, which is a
// general system error.
#define LOCK_ON_EXEC_ID                                             -2

#define LOCK_KEY_EXEC_ID                                            STRID6("execId", 0x12e0c56051)
#define LOCK_KEY_PERCENT_COMPLETE                                   STRID6("pctCplt", 0x14310a140d01)
#define LOCK_KEY_PROCESS_ID                                         STRID5("pid", 0x11300)

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
    const String *path;                                             // Lock path
    const String *execId;                                           // Process exec id
    const String *stanza;                                           // Stanza
    LockType type;                                                  // Lock type
    bool held;                                                      // Is the lock being held?

    struct
    {
        String *name;                                               // Name of lock file
        int fd;                                                     // File descriptor for lock file
    } file[lockTypeAll];
} lockLocal;

/**********************************************************************************************************************************/
FN_EXTERN void
lockInit(const String *const path, const String *const execId, const String *const stanza, const LockType type)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, execId);
        FUNCTION_LOG_PARAM(ENUM, type);
    FUNCTION_LOG_END();

    ASSERT(lockLocal.memContext == NULL);

    // Allocate a mem context to hold lock info
    MEM_CONTEXT_BEGIN(memContextTop())
    {
        MEM_CONTEXT_NEW_BEGIN(Lock, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            lockLocal.memContext = MEM_CONTEXT_NEW();
            lockLocal.path = strDup(path);
            lockLocal.execId = strDup(execId);
            lockLocal.stanza = strDup(stanza);
            lockLocal.type = type;
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN String *
lockFileName(const String *const stanza, const LockType lockType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(ENUM, lockType);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(STRING, strNewFmt("%s-%s" LOCK_FILE_EXT, strZ(stanza), lockTypeName[lockType]));
}

/***********************************************************************************************************************************
Read contents of lock file

If a seek is required to get to the beginning of the data, that must be done before calling this function.
***********************************************************************************************************************************/
// Size of initial buffer used to load lock file
#define LOCK_BUFFER_SIZE                                            128

// Helper to read data
static LockData
lockReadFileData(const String *const lockFile, const int fd)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(INT, fd);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(lockFile != NULL);
    ASSERT(fd != -1);

    LockData result = {0};

    TRY_BEGIN()
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Read contents of file
            Buffer *const buffer = bufNew(LOCK_BUFFER_SIZE);
            IoWrite *const write = ioBufferWriteNewOpen(buffer);

            ioCopyP(ioFdReadNewOpen(lockFile, fd, 0), write);
            ioWriteClose(write);

            JsonRead *const json = jsonReadNew(strNewBuf(buffer));
            jsonReadObjectBegin(json);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.execId = jsonReadStr(jsonReadKeyRequireStrId(json, LOCK_KEY_EXEC_ID));

                if (jsonReadKeyExpectStrId(json, LOCK_KEY_PERCENT_COMPLETE))
                    result.percentComplete = varNewUInt(jsonReadUInt(json));

                result.processId = jsonReadInt(jsonReadKeyRequireStrId(json, LOCK_KEY_PROCESS_ID));
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();
    }
    CATCH_ANY()
    {
        THROWP_FMT(errorType(), "unable to read lock file '%s': %s", strZ(lockFile), errorMessage());
    }
    TRY_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN LockReadResult
lockReadFile(const String *const lockFile, const LockReadFileParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, lockFile);
        FUNCTION_LOG_PARAM(BOOL, param.remove);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(lockFile != NULL);

    LockReadResult result = {.status = lockReadStatusValid};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If we cannot open the lock file for any reason then warn and continue to next file
        int fd = -1;

        if ((fd = open(strZ(lockFile), O_RDONLY, 0)) == -1)
        {
            result.status = lockReadStatusMissing;
        }
        else
        {
            // Attempt a lock on the file - if a lock can be acquired that means the original process died without removing the lock
            if (flock(fd, LOCK_EX | LOCK_NB) == 0)
            {
                result.status = lockReadStatusUnlocked;
            }
            // Else attempt to read the file
            else
            {
                TRY_BEGIN()
                {
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        result.data = lockReadFileData(lockFile, fd);
                    }
                    MEM_CONTEXT_PRIOR_END();
                }
                CATCH_ANY()
                {
                    result.status = lockReadStatusInvalid;
                }
                TRY_END();
            }

            // Remove lock file if requested but do not report failures
            if (param.remove)
                unlink(strZ(lockFile));

            // Close after unlinking to prevent a race condition where another process creates the file as we remove it
            close(fd);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN LockReadResult
lockRead(const String *const lockPath, const String *const stanza, const LockType lockType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, lockPath);
        FUNCTION_LOG_PARAM(STRING, stanza);
        FUNCTION_LOG_PARAM(ENUM, lockType);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    LockReadResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const lockFile = strNewFmt("%s/%s", strZ(lockPath), strZ(lockFileName(stanza, lockType)));

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = lockReadFileP(lockFile);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Write contents of lock file
***********************************************************************************************************************************/
FN_EXTERN void
lockWriteData(const LockType lockType, const LockWriteDataParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM(VARIANT, param.percentComplete);
    FUNCTION_LOG_END();

    ASSERT(lockType < lockTypeAll);
    ASSERT(lockLocal.file[lockType].name != NULL);
    ASSERT(lockLocal.file[lockType].fd != -1);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the json object
        JsonWrite *json = jsonWriteNewP();
        jsonWriteObjectBegin(json);

        jsonWriteStr(jsonWriteKeyStrId(json, LOCK_KEY_EXEC_ID), lockLocal.execId);

        if (param.percentComplete != NULL)
            jsonWriteUInt(jsonWriteKeyStrId(json, LOCK_KEY_PERCENT_COMPLETE), varUInt(param.percentComplete));

        jsonWriteInt(jsonWriteKeyStrId(json, LOCK_KEY_PROCESS_ID), getpid());

        jsonWriteObjectEnd(json);

        if (lockType == lockTypeBackup && lockLocal.held)
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

        ioCopyP(ioBufferReadNewOpen(BUFSTR(jsonWriteResult(json))), write);
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
                    storagePathCreateP(storagePosixNewP(FSLASH_STR, .write = true), strPath(lockFile));
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
                        execId = lockReadFileData(lockFile, result).execId;
                    }
                    CATCH_ANY()
                    {
                        // Any errors will be reported as unable to acquire a lock. If a process is trying to get a lock but is not
                        // synchronized with the process holding the actual lock, it is possible that it could see a short read or
                        // have some other problem reading. Reporting the error will likely be misleading when the actual problem is
                        // that another process owns the lock file.
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
            if (failOnNoLock || errNo != EWOULDBLOCK)
            {
                const String *errorHint = NULL;

                if (errNo == EWOULDBLOCK)
                    errorHint = strNewZ("\nHINT: is another " PROJECT_NAME " process running?");
                else if (errNo == EACCES)
                {
                    // Get information for the current user
                    userInit();

                    errorHint = strNewFmt(
                        "\nHINT: does '%s:%s' running " PROJECT_NAME " have permissions on the '%s' file?", strZ(userName()),
                        strZ(groupName()), strZ(lockFile));
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

FN_EXTERN bool
lockAcquire(const LockAcquireParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TIMEMSEC, param.timeout);
        FUNCTION_LOG_PARAM(BOOL, param.returnOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockLocal.memContext != NULL);

    bool result = true;

    // Don't allow failures when locking more than one file. This makes cleanup difficult and there are no known use cases.
    ASSERT(!param.returnOnNoLock || lockLocal.type != lockTypeAll);

    // Don't allow another lock if one is already held
    if (lockLocal.held)
        THROW(AssertError, "lock is already held by this process");

    // Lock files
    LockType lockMin = lockLocal.type == lockTypeAll ? lockTypeArchive : lockLocal.type;
    LockType lockMax = lockLocal.type == lockTypeAll ? (lockTypeAll - 1) : lockLocal.type;

    for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
    {
        MEM_CONTEXT_BEGIN(lockLocal.memContext)
        {
            strFree(lockLocal.file[lockIdx].name);
            lockLocal.file[lockIdx].name = strNewFmt("%s/%s", strZ(lockLocal.path), strZ(lockFileName(lockLocal.stanza, lockIdx)));
        }
        MEM_CONTEXT_END();

        lockLocal.file[lockIdx].fd = lockAcquireFile(lockLocal.file[lockIdx].name, param.timeout, !param.returnOnNoLock);

        if (lockLocal.file[lockIdx].fd == -1)
        {
            // Free the lock
            lockLocal.held = false;
            result = false;
            break;
        }
        // Else write lock data unless we locked an execId match
        else if (lockLocal.file[lockIdx].fd != LOCK_ON_EXEC_ID)
            lockWriteDataP(lockIdx);
    }

    if (result)
        lockLocal.held = true;

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

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Remove file first and then close it to release the lock. If we close it first then another process might grab the lock
        // right before the delete which means the file locked by the other process will get deleted.
        storageRemoveP(storagePosixNewP(FSLASH_STR, .write = true), lockFile);
        close(lockFd);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

FN_EXTERN bool
lockRelease(bool failOnNoLock)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, failOnNoLock);
    FUNCTION_LOG_END();

    bool result = false;

    if (!lockLocal.held)
    {
        if (failOnNoLock)
            THROW(AssertError, "no lock is held by this process");
    }
    else
    {
        // Release locks
        LockType lockMin = lockLocal.type == lockTypeAll ? lockTypeArchive : lockLocal.type;
        LockType lockMax = lockLocal.type == lockTypeAll ? (lockTypeAll - 1) : lockLocal.type;

        for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
        {
            if (lockLocal.file[lockIdx].fd != LOCK_ON_EXEC_ID)
                lockReleaseFile(lockLocal.file[lockIdx].fd, lockLocal.file[lockIdx].name);
        }

        // Free the lock context
        lockLocal.held = false;
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
