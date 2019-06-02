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

    // logInit(logLevelDebug, logLevelOff, logLevelOff, false, 999);

    if (strcmp(type, "<LOCAL>") == 0)
        RETVAL = (Storage *)storageLocalWrite();
    else if (strcmp(type, "<REPO>") == 0)
        RETVAL = (Storage *)storageRepoWrite();
    else
        THROW_FMT(AssertError, "unexpected storage type '%s'", type);
OUTPUT:
    RETVAL

####################################################################################################################################
bool
exists(self, fileExp)
    pgBackRest::LibC::Storage self
    SV *fileExp
CODE:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        RETVAL = storageExistsNP(self, STR_NEW_SV(fileExp));
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
get(self, read)
    pgBackRest::LibC::Storage self
    pgBackRest::LibC::StorageRead read
CODE:
    (void)self;
    RETVAL = NULL;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        Buffer *buffer = storageGetNP(read);

        if (buffer != NULL)
        {
            if (bufUsed(buffer) == 0)
                RETVAL = newSVpv("", 0);
            else
                RETVAL = newSVpv((char *)bufPtr(buffer), bufUsed(buffer));
        }
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
list(self, pathExp, ignoreMissing, sortAsc, expression)
    pgBackRest::LibC::Storage self
    SV *pathExp
    bool ignoreMissing
    bool sortAsc
    SV *expression = NULL_SV_OK($arg);
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        StringList *fileList = strLstSort(
            storageListP(
                self, STR_NEW_SV(pathExp), .errorOnMissing = !ignoreMissing,
                .expression = expression == NULL ? NULL : STR_NEW_SV(expression)),
            sortAsc ? sortOrderAsc : sortOrderDesc);

        const String *fileListJson = jsonFromVar(varNewVarLst(varLstNewStrLst(fileList)), 0);

        RETVAL = newSVpv(strPtr(fileListJson), strSize(fileListJson));
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
manifest(self, pathExp, filter=NULL)
    pgBackRest::LibC::Storage self
    SV *pathExp
    SV *filter = NULL_SV_OK($arg);
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        StorageManifestXsCallbackData data = {.storage = self, .json = strNew("{"), .pathRoot = STR_NEW_SV(pathExp)};

        storageInfoListP(self, data.pathRoot, storageManifestXsCallback, &data, .errorOnMissing = true);
        strCat(data.json, "}");
        (void)filter;

        RETVAL = newSVpv((char *)strPtr(data.json), strSize(data.json));
    }
    MEM_CONTEXT_XS_TEMP_END();
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
    storagePathCreateP(
        self, STR(pathExp), .mode = strlen(mode) == 0 ? 0 : cvtZToIntBase(mode, 8), .errorOnExists = !ignoreExists,
        .noParentCreate = !createParent);

####################################################################################################################################
bool
pathExists(self, pathExp)
    pgBackRest::LibC::Storage self
    const char *pathExp
CODE:
    RETVAL = storagePathExistsNP(self, STR(pathExp));
OUTPUT:
    RETVAL

####################################################################################################################################
SV *
pathGet(self, pathExp)
    pgBackRest::LibC::Storage self
    const char *pathExp
CODE:
    RETVAL = NULL;

    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
        String *path = storagePathNP(self, STR(pathExp));

        RETVAL = NEWSV(0, strSize(path));
        SvPOK_only(RETVAL);
        memcpy(SvPV_nolen(RETVAL), strPtr(path), strSize(path));
        SvCUR_set(RETVAL, strSize(path));
    }
    MEM_CONTEXT_XS_TEMP_END();
OUTPUT:
    RETVAL

####################################################################################################################################
U8
put(self, write, buffer)
    pgBackRest::LibC::Storage self
    pgBackRest::LibC::StorageWrite write
    const Buffer *buffer = BUF_CONST_SV($arg);
CODE:
    (void)self;
    storagePutNP(write, buffer);

    RETVAL = buffer ? bufUsed(buffer) : 0;
OUTPUT:
    RETVAL

####################################################################################################################################
const char *
type(self)
    pgBackRest::LibC::Storage self
CODE:
    RETVAL = strPtr(storageType(self));
OUTPUT:
    RETVAL
