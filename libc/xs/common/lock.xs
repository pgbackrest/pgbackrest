# ----------------------------------------------------------------------------------------------------------------------------------
# Lock Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
bool
lockAcquire(lockPath, command, stanza, lockTimeout, failOnNoLock)
    const char *lockPath
    const char *command
    const char *stanza
    double lockTimeout
    bool failOnNoLock
CODE:
    RETVAL = false;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        // Set the command so we can get the correct lock type to use
        cfgCommandSet(cfgCommandId(command, true));

        // Attempt to acquire the lock
        if (cfgLockRequired())
            RETVAL = lockAcquire(strNew(lockPath), strNew(stanza), cfgLockType(), (TimeMSec)(lockTimeout * 1000), failOnNoLock);
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
bool
lockRelease(failOnNoLock)
    bool failOnNoLock
CODE:
    RETVAL = false;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        RETVAL = lockRelease(failOnNoLock);
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL
