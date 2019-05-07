# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
void
storagePosixPathRemove(path, errorOnMissing, recurse)
    const char *path
    bool errorOnMissing
    bool recurse
CODE:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        storagePathRemoveP(
            storagePosixNew(strNew("/"), 0640, 750, true, NULL), strNew(path), .errorOnMissing = errorOnMissing,
            .recurse = recurse);
    }
    MEM_CONTEXT_XS_TEMP_END();
