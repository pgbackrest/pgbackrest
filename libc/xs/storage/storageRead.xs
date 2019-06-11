# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Read Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::StorageRead

####################################################################################################################################
pgBackRest::LibC::StorageRead
new(class, storage, file, ignoreMissing)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    const String *class = STR_NEW_SV($arg);
    pgBackRest::LibC::Storage storage
    const String *file = STR_NEW_SV($arg);
    bool ignoreMissing
CODE:
    CHECK(strEqZ(class, PACKAGE_NAME_LIBC "::StorageRead"));

    RETVAL = storageReadMove(storageNewReadP(storage, file, .ignoreMissing = ignoreMissing), MEM_CONTEXT_XS_OLD());
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
    pgBackRest::LibC::StorageRead self
    const String *filter = STR_NEW_SV($arg);
    const String *param = STR_NEW_SV($arg);
CODE:
    IoFilterGroup *filterGroup = ioReadFilterGroup(storageReadIo(self));

    if (filterGroup == NULL)
    {
        filterGroup = ioFilterGroupNew();
        ioReadFilterGroupSet(storageReadIo(self), filterGroup);
    }

    storageFilterXsAdd(filterGroup, filter, param);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
open(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead self
CODE:
    RETVAL = ioReadOpen(storageReadIo(self));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
SV *
read(self, bufferSize)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead self
    U32 bufferSize
CODE:
    RETVAL = NEWSV(0, bufferSize);
    SvPOK_only(RETVAL);

    Buffer *bufferRead = bufNewUseC((unsigned char *)SvPV_nolen(RETVAL), bufferSize);
    ioRead(storageReadIo(self), bufferRead);

    SvCUR_set(RETVAL, bufUsed(bufferRead));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
eof(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead self
CODE:
    RETVAL = ioReadEof(storageReadIo(self));
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
    pgBackRest::LibC::StorageRead self
CODE:
    ioReadClose(storageReadIo(self));
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
    pgBackRest::LibC::StorageRead self
    const String *filter = STR_NEW_SV($arg);
CODE:
    RETVAL = strPtr(storageFilterXsResult(ioReadFilterGroup(storageReadIo(self)), filter));
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
    pgBackRest::LibC::StorageRead self
CODE:
    storageReadFree(self);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();
