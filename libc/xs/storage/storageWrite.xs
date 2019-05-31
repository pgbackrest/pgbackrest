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
        RETVAL->memContext = MEM_CONTEXT_XS();

        RETVAL->storage = storage->pxPayload;
        RETVAL->write = storageNewWriteP(
            RETVAL->storage, STR(file), .modeFile = mode, .user = strlen(user) == 0 ? NULL : STR(user),
            .group = strlen(group) == 0 ? NULL : STR(group), .timeModified = (time_t)timeModified, .noCreatePath = !pathCreate,
            .noSyncPath = !atomic, .noAtomic = !atomic);

        ioWriteOpen(storageWriteIo(RETVAL->write));
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
U32
write(self, buffer)
    pgBackRest::LibC::StorageWrite self
    SV *buffer
CODE:
    RETVAL = 0;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        STRLEN bufferSize;
        const void *bufferPtr;

        if (SvROK(buffer))
            bufferPtr = SvPV(SvRV(buffer), bufferSize);
        else
            bufferPtr = SvPV(buffer, bufferSize);

        ioWrite(storageWriteIo(self->write), BUF(bufferPtr, bufferSize));

        RETVAL = bufferSize;
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
close(self)
    pgBackRest::LibC::StorageWrite self
CODE:
    ioWriteClose(storageWriteIo(self->write));

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::StorageWrite self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);
