# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Write Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::StorageWrite

####################################################################################################################################
pgBackRest::LibC::StorageWrite
new(class, storage, file, mode, user, group, timeModified, atomic, pathCreate)
    const char *class
    pgBackRest::LibC::Storage storage
    const char *file
    U32 mode
    const char *user
    const char *group
    U8 timeModified
    bool atomic
    bool pathCreate
CODE:
    CHECK(strcmp(class, PACKAGE_NAME_LIBC "::StorageWrite") == 0);

    RETVAL = NULL;

    MEM_CONTEXT_XS_NEW_BEGIN("StorageWriteXs")
    {
        RETVAL = memNew(sizeof(StorageWriteXs));
        RETVAL->memContext = MEM_COMTEXT_XS();

        RETVAL->storage = storage->pxPayload;
        RETVAL->write = storageNewWriteP(
            RETVAL->storage, STR(file), .modeFile = mode, .user = STR(user), .group = STR(group),
            .timeModified = (time_t)timeModified, .noCreatePath = !pathCreate, .noSyncPath = !atomic, .noAtomic = !atomic);
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::StorageWrite self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);
