# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Write Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::StorageWrite

####################################################################################################################################
pgBackRest::LibC::StorageWrite
new(class, storage, file, mode, user, group, timeModified, atomic, pathCreate)
    const char *class
    pgBackRest::LibC::Storage storage
    SV *file
    U32 mode
    const char *user
    const char *group
    U8 timeModified
    bool atomic
    bool pathCreate
CODE:
    CHECK(strcmp(class, PACKAGE_NAME_LIBC "::StorageWrite") == 0);

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        RETVAL = storageNewWriteP(
            storage, STR_NEW_SV(file), .modeFile = mode, .user = strlen(user) == 0 ? NULL : STR(user),
            .group = strlen(group) == 0 ? NULL : STR(group), .timeModified = (time_t)timeModified, .noCreatePath = !pathCreate,
            .noSyncPath = !atomic, .noAtomic = !atomic);

        storageWriteMove(RETVAL, MEM_CONTEXT_XS_OLD());
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::StorageWrite self
CODE:
    storageWriteFree(self);
