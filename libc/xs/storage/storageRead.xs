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
