# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::Storage

####################################################################################################################################
pgBackRest::LibC::Storage
new(class, type)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    const String *class = STR_NEW_SV($arg);
    const String *type = STR_NEW_SV($arg);
CODE:
    CHECK(strEqZ(class, PACKAGE_NAME_LIBC "::Storage"));

    // logInit(logLevelDebug, logLevelOff, logLevelOff, false, 999);

    if (strEqZ(type, "<LOCAL>"))
        RETVAL = (Storage *)storageLocalWrite();
    else if (strEqZ(type, "<REPO>"))
        RETVAL = (Storage *)storageRepoWrite();
    else
        THROW_FMT(AssertError, "unexpected storage type '%s'", strPtr(type));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
exists(self, fileExp)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *fileExp = STR_NEW_SV($arg);
CODE:
    RETVAL = storageExistsNP(self, fileExp);
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
SV *
get(self, read)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead read
CODE:
    RETVAL = NULL;
    Buffer *buffer = storageGetNP(read);

    if (buffer != NULL)
    {
        if (bufUsed(buffer) == 0)
            RETVAL = newSVpv("", 0);
        else
            RETVAL = newSVpv((char *)bufPtr(buffer), bufUsed(buffer));
    }
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
SV *
list(self, pathExp, ignoreMissing, sortAsc, expression)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
    bool ignoreMissing
    bool sortAsc
    const String *expression = STR_NEW_SV($arg);
CODE:
    StringList *fileList = strLstSort(
        storageListP(self, pathExp, .errorOnMissing = !ignoreMissing, .expression = expression),
        sortAsc ? sortOrderAsc : sortOrderDesc);

    const String *fileListJson = jsonFromVar(varNewVarLst(varLstNewStrLst(fileList)), 0);

    RETVAL = newSVpv(strPtr(fileListJson), strSize(fileListJson));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
SV *
manifest(self, pathExp, filter=NULL)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
    const String *filter = STR_NEW_SV($arg);
CODE:
    CHECK(filter == NULL); // !!! NOT YET IMPLEMENTED

    StorageManifestXsCallbackData data = {.storage = self, .json = strNew("{"), .pathRoot = pathExp};
    storageInfoListP(self, data.pathRoot, storageManifestXsCallback, &data, .errorOnMissing = true);
    strCat(data.json, "}");

    RETVAL = newSVpv((char *)strPtr(data.json), strSize(data.json));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
pathCreate(self, pathExp, mode, ignoreExists, createParent)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
    const String *mode = STR_NEW_SV($arg);
    bool ignoreExists
    bool createParent
CODE:
    storagePathCreateP(
        self, pathExp, .mode = strSize(mode) == 0 ? 0 : cvtZToIntBase(strPtr(mode), 8), .errorOnExists = !ignoreExists,
        .noParentCreate = !createParent);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
pathExists(self, pathExp)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
CODE:
    RETVAL = storagePathExistsNP(self, pathExp);
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
SV *
pathGet(self, pathExp)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
CODE:
    String *path = storagePathNP(self, pathExp);
    RETVAL = newSVpv((char *)strPtr(path), strSize(path));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
U8
put(self, write, buffer)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite write
    const Buffer *buffer = BUF_CONST_SV($arg);
CODE:
    storagePutNP(write, buffer);
    RETVAL = buffer ? bufUsed(buffer) : 0;
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
type(self)
    pgBackRest::LibC::Storage self
CODE:
    RETVAL = strPtr(storageType(self));
OUTPUT:
    RETVAL
