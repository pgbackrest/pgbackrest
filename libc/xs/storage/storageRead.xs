# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Read Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::StorageRead

####################################################################################################################################
pgBackRest::LibC::StorageRead
new(class, storage, file, ignoreMissing)
    const char *class
    pgBackRest::LibC::Storage storage
    const char *file
    bool ignoreMissing
CODE:
    CHECK(strcmp(class, PACKAGE_NAME_LIBC "::StorageRead") == 0);

    RETVAL = storageNewReadP(storage->pxPayload, STR(file), .ignoreMissing = ignoreMissing);
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::StorageRead self
CODE:
    storageReadFree(self);
