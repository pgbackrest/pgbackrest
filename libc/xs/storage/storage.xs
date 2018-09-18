# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
void
storageDriverPosixPathRemove(path, errorOnMissing, recurse)
    const char *path
    bool errorOnMissing
    bool recurse
CODE:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        storageDriverPosixPathRemove(storageDriverPosixNew(strNew("/"), 0640, 750, true, NULL), strNew(path), errorOnMissing, recurse);
    }
    MEM_CONTEXT_XS_TEMP_END();
