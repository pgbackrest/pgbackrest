####################################################################################################################################
# Block Cipher Perl Exports
#
# XS wrapper for functions in cipher/block.c.
####################################################################################################################################

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::Cipher::Block

####################################################################################################################################
pgBackRest::LibC::Cipher::Block
new(class, mode, type, key, keySize, digest = NULL)
    const char *class
    U32 mode
    const char *type
    unsigned char *key
    I32 keySize
    const char *digest
CODE:
    RETVAL = NULL;

    CHECK(type != NULL);
    CHECK(key != NULL);
    CHECK(keySize != 0);

    // Not much point to this but it keeps the var from being unused
    if (strcmp(class, PACKAGE_NAME_LIBC "::Cipher::Block") != 0)
        croak("unexpected class name '%s'", class);

    MEM_CONTEXT_XS_NEW_BEGIN("cipherBlockXs")
    {
        RETVAL = memNew(sizeof(CipherBlockXs));
        RETVAL->memContext = MEM_COMTEXT_XS();

        RETVAL->pxPayload = cipherBlockNew(mode, cipherType(STR(type)), BUF(key, keySize), digest == NULL ? NULL : STR(digest));
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
process(self, source)
    pgBackRest::LibC::Cipher::Block self
    SV *source
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        STRLEN tSize;
        const unsigned char *sourcePtr = (const unsigned char *)SvPV(source, tSize);

        RETVAL = NEWSV(0, ioBufferSize());
        SvPOK_only(RETVAL);

        if (tSize > 0)
        {
            size_t outBufferUsed = 0;

            do
            {
                SvGROW(RETVAL, outBufferUsed + ioBufferSize());
                Buffer *outBuffer = bufNewUseC((unsigned char *)SvPV_nolen(RETVAL) + outBufferUsed, ioBufferSize());

                ioFilterProcessInOut(self->pxPayload, BUF(sourcePtr, tSize), outBuffer);
                outBufferUsed += bufUsed(outBuffer);
            }
            while (ioFilterInputSame(self->pxPayload));

            SvCUR_set(RETVAL, outBufferUsed);
        }
        else
            SvCUR_set(RETVAL, 0);
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
flush(self)
    pgBackRest::LibC::Cipher::Block self
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        RETVAL = NEWSV(0, ioBufferSize());
        SvPOK_only(RETVAL);

        size_t outBufferUsed = 0;

        do
        {
            SvGROW(RETVAL, outBufferUsed + ioBufferSize());
            Buffer *outBuffer = bufNewUseC((unsigned char *)SvPV_nolen(RETVAL) + outBufferUsed, ioBufferSize());

            ioFilterProcessInOut(self->pxPayload, NULL, outBuffer);
            outBufferUsed += bufUsed(outBuffer);
        }
        while (!ioFilterDone(self->pxPayload));

        SvCUR_set(RETVAL, outBufferUsed);
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::Cipher::Block self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);
