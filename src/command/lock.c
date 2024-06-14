/***********************************************************************************************************************************
Command Lock Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/lock.h"
#include "common/debug.h"
#include "common/log.h"
#include "config/config.h"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct CmdLockLocal
{
    bool held;                                                      // Is the lock being held?
} cmdLockLocal;

/***********************************************************************************************************************************
Lock type names
***********************************************************************************************************************************/
static const char *const lockTypeName[] =
{
    "archive",                                                      // lockTypeArchive
    "backup",                                                       // lockTypeBackup
};

/**********************************************************************************************************************************/
static String *
cmdLockFileName(const String *const stanza, const LockType lockType)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, stanza);
        FUNCTION_TEST_PARAM(ENUM, lockType);
    FUNCTION_TEST_END();

    ASSERT(stanza != NULL);
    ASSERT(lockType < lockTypeAll);

    FUNCTION_TEST_RETURN(STRING, strNewFmt("%s-%s" LOCK_FILE_EXT, strZ(stanza), lockTypeName[lockType]));
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cmdLockAcquire(const LockAcquireParam param)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BOOL, param.returnOnNoLock);
    FUNCTION_LOG_END();

    bool result = true;

    // Don't allow return on no lock when locking more than one file. This makes cleanup difficult and there are no known use cases.
    const LockType lockType = cfgLockType();
    ASSERT(!param.returnOnNoLock || lockType != lockTypeAll);

    // Don't allow another lock if one is already held
    if (cmdLockLocal.held)
        THROW(AssertError, "lock is already held by this process");

    // Lock files
    const LockType lockMin = lockType == lockTypeAll ? lockTypeArchive : lockType;
    const LockType lockMax = lockType == lockTypeAll ? (lockTypeAll - 1) : lockType;

    for (LockType lockIdx = lockMin; lockIdx <= lockMax; lockIdx++)
    {
        String *const lockFileName = cmdLockFileName(cfgOptionStr(cfgOptStanza), lockIdx);

        result = lockAcquire(lockFileName, param);
        strFree(lockFileName);
    }

    // Set lock held flag
    cmdLockLocal.held = result;

    FUNCTION_LOG_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdLockWrite(const LockWriteParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(VARIANT, param.percentComplete);
        FUNCTION_LOG_PARAM(VARIANT, param.sizeComplete);
        FUNCTION_LOG_PARAM(VARIANT, param.size);
    FUNCTION_LOG_END();

    String *const lockFileName = cmdLockFileName(cfgOptionStr(cfgOptStanza), cfgLockType());

    lockWrite(lockFileName, param);
    strFree(lockFileName);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN LockReadResult
cmdLockRead(const LockType lockType, const String *const stanza)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, lockType);
        FUNCTION_LOG_PARAM(STRING, stanza);
    FUNCTION_LOG_END();

    ASSERT(lockType == lockTypeBackup);
    ASSERT(stanza != NULL);

    FUNCTION_AUDIT_STRUCT();

    LockReadResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const lockFileName = cmdLockFileName(stanza, lockType);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = lockReadP(lockFileName);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
cmdLockRelease(const LockReleaseParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, param.returnOnNoLock);
    FUNCTION_LOG_END();

    bool result = true;

    // Release lock(s)
    if (cmdLockLocal.held)
    {
        result = lockRelease(param);
        cmdLockLocal.held = false;
    }
    // Else error when requested
    else if (!param.returnOnNoLock)
        THROW(AssertError, "no lock is held by this process");

    FUNCTION_LOG_RETURN(BOOL, result);
}
