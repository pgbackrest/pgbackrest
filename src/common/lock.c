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
#define LOCK_KEY_SIZE_COMPLETE                                      STRID6("szCplt", 0x50c4286931)
#define LOCK_KEY_SIZE                                               STRID5("sz", 0x3530)

/***********************************************************************************************************************************
Mem context and local variables
***********************************************************************************************************************************/
typedef struct LockFile
{
    String *name;                                                   // Name of lock file
    int fd;                                                         // File descriptor for lock file
    bool written;                                                   // Has the lock file been written?
} LockFile;

static struct LockLocal
{
    MemContext *memContext;                                         // Mem context for locks
    const String *path;                                             // Lock path
    const String *execId;                                           // Process exec id
    List *lockList;                                                 // List of locks held
    const Storage *storage;                                         // Storage object for lock path
} lockLocal;

/**********************************************************************************************************************************/
FN_EXTERN void
lockInit(const String *const path, const String *const execId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(STRING, execId);
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
            lockLocal.lockList = lstNewP(sizeof(LockFile), .comparator = lstComparatorStr);
            lockLocal.storage = storagePosixNewP(lockLocal.path, .write = true);
        }
        MEM_CONTEXT_NEW_END();
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Read contents of lock file
***********************************************************************************************************************************/
// Size of initial buffer used to load lock file
#define LOCK_BUFFER_SIZE                                            128

// Helper to read data
static LockData
lockReadData(const String *const lockFileName, const int fd)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFileName);
        FUNCTION_LOG_PARAM(INT, fd);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(lockFileName != NULL);
    ASSERT(fd != -1);

    LockData result = {0};

    TRY_BEGIN()
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Read contents of file
            Buffer *const buffer = bufNew(LOCK_BUFFER_SIZE);
            IoWrite *const write = ioBufferWriteNewOpen(buffer);

            ioCopyP(ioFdReadNewOpen(lockFileName, fd, 0), write);
            ioWriteClose(write);

            JsonRead *const json = jsonReadNew(strNewBuf(buffer));
            jsonReadObjectBegin(json);

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                result.execId = jsonReadStr(jsonReadKeyRequireStrId(json, LOCK_KEY_EXEC_ID));

                if (jsonReadKeyExpectStrId(json, LOCK_KEY_PERCENT_COMPLETE))
                    result.percentComplete = varNewUInt(jsonReadUInt(json));

                result.processId = jsonReadInt(jsonReadKeyRequireStrId(json, LOCK_KEY_PROCESS_ID));

                if (jsonReadKeyExpectStrId(json, LOCK_KEY_SIZE))
                    result.size = varNewUInt64(jsonReadUInt64(json));

                if (jsonReadKeyExpectStrId(json, LOCK_KEY_SIZE_COMPLETE))
                    result.sizeComplete = varNewUInt64(jsonReadUInt64(json));
            }
            MEM_CONTEXT_PRIOR_END();
        }
        MEM_CONTEXT_TEMP_END();
    }
    CATCH_ANY()
    {
        THROWP_FMT(errorType(), "unable to read lock file '%s': %s", strZ(lockFileName), errorMessage());
    }
    TRY_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
lockAcquire(const String *const lockFileName, const LockAcquireParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFileName);
        FUNCTION_LOG_PARAM(BOOL, param.returnOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockFileName != NULL);
    ASSERT(lockLocal.memContext != NULL);

    bool result = false;

    if (lstFind(lockLocal.lockList, &lockFileName) != NULL)
        THROW_FMT(AssertError, "lock on file '%s' already held", strZ(lockFileName));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const lockFilePath = storagePathP(lockLocal.storage, lockFileName);
        bool retry;
        int errNo = 0;
        int fd = -1;

        do
        {
            // Assume there will be no retry
            retry = false;

            // Attempt to open the file
            if ((fd = open(strZ(lockFilePath), O_RDWR | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1)
            {
                // Save the error for reporting outside the loop
                errNo = errno;

                // If the path does not exist then create it
                if (errNo == ENOENT)
                {
                    storagePathCreateP(storagePosixNewP(FSLASH_STR, .write = true), strPath(lockFilePath));
                    retry = true;
                }
            }
            else
            {
                // Attempt to lock the file
                if (flock(fd, LOCK_EX | LOCK_NB) == -1)
                {
                    // Save the error for reporting outside the loop
                    errNo = errno;

                    // Get execId from lock file and close it
                    const String *execId = NULL;

                    TRY_BEGIN()
                    {
                        execId = lockReadData(lockFileName, fd).execId;
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
                        close(fd);
                    }
                    TRY_END();

                    // Even though we were unable to lock the file, it may be that it is already locked by another process with the
                    // same exec-id, i.e. spawned by the same original main process. If so, report the lock as successful.
                    if (strEq(execId, lockLocal.execId))
                        fd = LOCK_ON_EXEC_ID;
                    else
                        fd = -1;
                }
            }
        }
        while (fd == -1 && retry);

        // If the lock was not successful
        if (fd == -1)
        {
            // Error when requested
            if (!param.returnOnNoLock || errNo != EWOULDBLOCK)
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
                        strZ(groupName()), strZ(lockFilePath));
                }

                THROW_FMT(
                    LockAcquireError, "unable to acquire lock on file '%s': %s%s", strZ(lockFilePath), strerror(errNo),
                    errorHint == NULL ? "" : strZ(errorHint));
            }
        }
        // Else add lock to list
        else
        {
            MEM_CONTEXT_OBJ_BEGIN(lockLocal.lockList)
            {
                lstAdd(lockLocal.lockList, &(LockFile){.name = strDup(lockFileName), .fd = fd});
            }
            MEM_CONTEXT_OBJ_END();

            // Write lock data unless lock was acquired by matching execId
            if (fd != LOCK_ON_EXEC_ID)
                lockWriteP(lockFileName);

            // Success
            result = true;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN LockReadResult
lockRead(const String *const lockFileName, const LockReadParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, lockFileName);
        FUNCTION_LOG_PARAM(BOOL, param.remove);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_STRUCT();

    ASSERT(lockLocal.memContext != NULL);
    ASSERT(lockFileName != NULL);

    LockReadResult result = {.status = lockReadStatusValid};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const lockFilePath = storagePathP(lockLocal.storage, lockFileName);

        // If we cannot open the lock file for any reason then warn
        int fd = -1;

        if ((fd = open(strZ(lockFilePath), O_RDONLY, 0)) == -1)
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
                        result.data = lockReadData(lockFilePath, fd);
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
                unlink(strZ(lockFilePath));

            // Close after unlinking to prevent a race condition where another process creates the file as we remove it
            close(fd);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
lockWrite(const String *const lockFileName, const LockWriteParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, lockFileName);
        FUNCTION_LOG_PARAM(VARIANT, param.percentComplete);
        FUNCTION_LOG_PARAM(VARIANT, param.sizeComplete);
        FUNCTION_LOG_PARAM(VARIANT, param.size);
    FUNCTION_LOG_END();

    ASSERT(lockFileName != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the json object
        JsonWrite *const json = jsonWriteNewP();
        jsonWriteObjectBegin(json);

        jsonWriteStr(jsonWriteKeyStrId(json, LOCK_KEY_EXEC_ID), lockLocal.execId);

        if (param.percentComplete != NULL)
            jsonWriteUInt(jsonWriteKeyStrId(json, LOCK_KEY_PERCENT_COMPLETE), varUInt(param.percentComplete));

        jsonWriteInt(jsonWriteKeyStrId(json, LOCK_KEY_PROCESS_ID), getpid());

        if (param.size != NULL)
            jsonWriteUInt64(jsonWriteKeyStrId(json, LOCK_KEY_SIZE), varUInt64(param.size));

        if (param.sizeComplete != NULL)
            jsonWriteUInt64(jsonWriteKeyStrId(json, LOCK_KEY_SIZE_COMPLETE), varUInt64(param.sizeComplete));

        jsonWriteObjectEnd(json);

        // Write to lock file
        LockFile *const lockFile = lstFind(lockLocal.lockList, &lockFileName);

        if (lockFile == NULL)
            THROW_FMT(AssertError, "lock file '%s' not found", strZ(lockFileName));

        const String *const lockFilePath = storagePathP(lockLocal.storage, lockFileName);

        if (lockFile->written)
        {
            // Seek to beginning of lock file
            THROW_ON_SYS_ERROR_FMT(
                lseek(lockFile->fd, 0, SEEK_SET) == -1, FileOpenError, STORAGE_ERROR_READ_SEEK, (uint64_t)0, strZ(lockFilePath));

            // In case the current write is shorter than before
            THROW_ON_SYS_ERROR_FMT(ftruncate(lockFile->fd, 0) == -1, FileWriteError, "unable to truncate '%s'", strZ(lockFilePath));
        }

        // Write lock file data
        IoWrite *const write = ioFdWriteNewOpen(lockFilePath, lockFile->fd, 0);

        ioCopyP(ioBufferReadNewOpen(BUFSTR(jsonWriteResult(json))), write);
        ioWriteClose(write);

        lockFile->written = true;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN bool
lockRelease(const LockReleaseParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, param.returnOnNoLock);
    FUNCTION_LOG_END();

    ASSERT(lockLocal.memContext != NULL);

    bool result = false;

    // Nothing to do if lock list is empty
    if (lstEmpty(lockLocal.lockList))
    {
        // Fail if requested
        if (!param.returnOnNoLock)
            THROW(AssertError, "no lock is held by this process");
    }
    // Else release all locks
    else
    {
        // Release until list is empty
        while (!lstEmpty(lockLocal.lockList))
        {
            LockFile *const lockFile = lstGet(lockLocal.lockList, 0);

            // Remove lock file if this lock was not acquired by matching execId
            if (lockFile->fd != LOCK_ON_EXEC_ID)
            {
                storageRemoveP(lockLocal.storage, lockFile->name);
                close(lockFile->fd);
            }

            strFree(lockFile->name);
            lstRemoveIdx(lockLocal.lockList, 0);
        }

        // Success
        result = true;
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
