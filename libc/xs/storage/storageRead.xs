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
    StorageRead *read = storageNewReadP(storage->pxPayload, STR(file), .ignoreMissing = ignoreMissing);

    if (ioReadOpen(storageReadIo(read)))
    {
        MEM_CONTEXT_XS_NEW_BEGIN("StorageReadXs")
        {
            RETVAL = memNew(sizeof(StorageReadXs));
            RETVAL->memContext = MEM_COMTEXT_XS();

            RETVAL->storage = storage->pxPayload;
            RETVAL->read = read;
        }
        MEM_CONTEXT_XS_NEW_END();
    }
    else
    {
        storageReadFree(read);
    }
OUTPUT:
    RETVAL

####################################################################################################################################
U32
read(self, buffer, size)
    pgBackRest::LibC::StorageRead self
    SV *buffer
    U32 size
CODE:
    RETVAL = 0;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        buffer = NEWSV(0, ioBufferSize());
        SvPOK_only(buffer);

        Buffer *tempBuffer = bufNewUseC(SvPV_nolen(buffer), size);
        RETVAL = (uint32_t)ioRead(storageReadIo(self->read), tempBuffer);

        SvCUR_set(buffer, RETVAL);
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
