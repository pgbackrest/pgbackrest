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
        else if (strcmp(type, "<REPO>") == 0)
            RETVAL->pxPayload = storageRepoWrite();
        else
            THROW_FMT(AssertError, "unexpected storage type '%s'", type);
    }
    MEM_CONTEXT_XS_NEW_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
list(self, pathExp, ignoreMissing, expression)
    pgBackRest::LibC::Storage self
    const char *pathExp
    bool ignoreMissing
    const char *expression
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
            self->pxPayload, STR(pathExp), .mode = strlen(mode) == 0 ? 0 : cvtZToIntBase(mode, 8), .errorOnExists = !ignoreExists,
            .noParentCreate = !createParent);
    }
    MEM_CONTEXT_XS_END();

####################################################################################################################################
bool
pathExists(self, pathExp)
    pgBackRest::LibC::Storage self
    const char *pathExp
CODE:
    RETVAL = false;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        RETVAL = storagePathExistsNP(self->pxPayload, STR(pathExp));
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
pathGet(self, pathExp)
    pgBackRest::LibC::Storage self
    const char *pathExp
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_BEGIN(self->memContext)
    {
        String *path = storagePathNP(self->pxPayload, STR(pathExp));

        RETVAL = NEWSV(0, strSize(path));
        SvPOK_only(RETVAL);
        memcpy(SvPV_nolen(RETVAL), strPtr(path), strSize(path));
        SvCUR_set(RETVAL, strSize(path));
    }
    MEM_CONTEXT_XS_END();
OUTPUT:
    RETVAL

####################################################################################################################################
void
DESTROY(self)
    pgBackRest::LibC::Storage self
CODE:
    MEM_CONTEXT_XS_DESTROY(self->memContext);
