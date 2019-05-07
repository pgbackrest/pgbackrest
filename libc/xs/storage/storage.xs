# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::Storage

####################################################################################################################################
pgBackRest::LibC::Storage
new(class, type)
    const char *class
    const char *type
CODE:
    CHECK(strcmp(class, PACKAGE_NAME_LIBC "::Storage") == 0);

    RETVAL = NULL;

    MEM_CONTEXT_XS_NEW_BEGIN("StorageXs")
    {
        RETVAL = memNew(sizeof(StorageXs));
        RETVAL->memContext = MEM_COMTEXT_XS();

        if (strcmp(type, "<LOCAL>") == 0)
            RETVAL->pxPayload = storageLocalWrite();
        else
            croak("unexpected storage type '%s'", type);
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
pathCreate(self, pathExp, mode, ignoreExists, createParent)
    pgBackRest::LibC::Storage self
    const char *pathExp
    const char *mode
    bool ignoreExists
    bool createParent
CODE:
    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        storagePathCreateP(
            self->pxPayload, STR(pathExp), .mode = cvtZToIntBase(mode, 8), .errorOnExists = !ignoreExists,
            .noParentCreate = !createParent);
    }
    MEM_CONTEXT_XS_END();

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::Storage self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);

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
