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

    RETVAL = NULL;

    MEM_CONTEXT_XS_NEW_BEGIN("StorageReadXs")
    {
        RETVAL = memNew(sizeof(StorageReadXs));
        RETVAL->memContext = MEM_COMTEXT_XS();

        RETVAL->storage = storage->pxPayload;
        RETVAL->read = storageNewReadP(RETVAL->storage, STR(file), .ignoreMissing = ignoreMissing);
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
U32
read(self, buffer, size)
    pgBackRest::LibC::StorageRead self
    SV *buffer
    U32 size
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        StringList *fileList = storageListP(
            self->pxPayload, STR(pathExp), .errorOnMissing = !ignoreMissing,
            .expression = strlen(expression) == 0 ? NULL : STR(expression));

        if (fileList != NULL)
        {
            const String *fileListJson = jsonFromVar(varNewVarLst(varLstNewStrLst(fileList)), 0);

            RETVAL = NEWSV(0, strSize(fileListJson));
            SvPOK_only(RETVAL);
            memcpy(SvPV_nolen(RETVAL), strPtr(fileListJson), strSize(fileListJson));
            SvCUR_set(RETVAL, strSize(fileListJson));
        }
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::StorageRead self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);
