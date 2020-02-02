# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Write Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::StorageWrite

####################################################################################################################################
pgBackRest::LibC::StorageWrite
new(class, storage, file, mode, user, group, timeModified, atomic, pathCreate)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    const String *class = STR_NEW_SV($arg);
    pgBackRest::LibC::Storage storage
    const String *file = STR_NEW_SV($arg);
    U32 mode
    const String *user = STR_NEW_SV($arg);
    const String *group = STR_NEW_SV($arg);
    IV timeModified
    bool atomic
    bool pathCreate
CODE:
    CHECK(strEqZ(class, PACKAGE_NAME_LIBC "::StorageWrite"));

    RETVAL = storageWriteMove(
        storageNewWriteP(
            storage, file, .modeFile = mode, .user = user, .group = group, .timeModified = (time_t)timeModified,
            .noCreatePath = storageFeature(storage, storageFeaturePath) ? !pathCreate : false, .noSyncPath = !atomic,
            .noAtomic = !atomic),
        memContextPrior());
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
filterAdd(self, filter, param)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
    const String *filter = STR_NEW_SV($arg);
    const String *param = STR_NEW_SV($arg);
CODE:
    IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(self));
    storageFilterXsAdd(filterGroup, filter, param);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
open(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
CODE:
    ioWriteOpen(storageWriteIo(self));
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
UV
write(self, buffer)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
    const Buffer *buffer = BUF_CONST_SV($arg);
CODE:
    ioWrite(storageWriteIo(self), buffer);
    RETVAL = bufUsed(buffer);
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
close(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
CODE:
    ioWriteClose(storageWriteIo(self));
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
result(self, filter)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
    const String *filter = STR_NEW_SV($arg);
CODE:
    RETVAL = strPtr(storageFilterXsResult(ioWriteFilterGroup(storageWriteIo(self)), filter));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
resultAll(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
CODE:
    RETVAL = strPtr(storageFilterXsResultAll(ioWriteFilterGroup(storageWriteIo(self))));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
DESTROY(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite self
CODE:
    storageWriteFree(self);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();
