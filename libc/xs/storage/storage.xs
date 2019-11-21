# ----------------------------------------------------------------------------------------------------------------------------------
# Storage Exports
# ----------------------------------------------------------------------------------------------------------------------------------

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC::Storage

####################################################################################################################################
pgBackRest::LibC::Storage
new(class, type, path)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    const String *class = STR_NEW_SV($arg);
    const String *type = STR_NEW_SV($arg);
    const String *path = STR_NEW_SV($arg);
CODE:
    CHECK(strEqZ(class, PACKAGE_NAME_LIBC "::Storage"));

    if (strEqZ(type, "<LOCAL>"))
    {
        memContextSwitch(MEM_CONTEXT_XS_OLD());
        RETVAL = storagePosixNew(
            path == NULL ? STRDEF("/") : path, STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);
        storagePathEnforceSet((Storage *)RETVAL, false);
        memContextSwitch(MEM_CONTEXT_XS_TEMP());
    }
    else if (strEqZ(type, "<REPO>"))
    {
        CHECK(path == NULL);
        RETVAL = (Storage *)storageRepoWrite();
    }
    else if (strEqZ(type, "<DB>"))
    {
        CHECK(path == NULL);

        memContextSwitch(MEM_CONTEXT_XS_OLD());
        RETVAL = storagePosixNew(cfgOptionStr(cfgOptPgPath), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);
        storagePathEnforceSet((Storage *)RETVAL, false);
        memContextSwitch(MEM_CONTEXT_XS_TEMP());
    }
    else
        THROW_FMT(AssertError, "unexpected storage type '%s'", strPtr(type));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
bucketCreate(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
CODE:
    if (strEq(storageType(self), STORAGE_S3_TYPE_STR))
        storageS3Request((StorageS3 *)storageDriver(self), HTTP_VERB_PUT_STR, FSLASH_STR, NULL, NULL, true, false);
    else
        THROW_FMT(AssertError, "unable to create bucket on '%s' storage", strPtr(storageType(self)));
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
copy(self, source, destination)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead source
    pgBackRest::LibC::StorageWrite destination
CODE:
    RETVAL = storageCopyP(source, destination);
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
    RETVAL = storageExistsP(self, fileExp);
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
    Buffer *buffer = storageGetP(read);

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
info(self, pathExp, ignoreMissing)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
    bool ignoreMissing
CODE:
    RETVAL = NULL;

    StorageInfo info = storageInfoP(self, pathExp, .ignoreMissing = ignoreMissing);

    if (info.exists)
    {
        String *json = storageManifestXsInfo(NULL, &info);
        RETVAL = newSVpv((char *)strPtr(json), strSize(json));
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
        storageListP(self, pathExp, .errorOnMissing = storageFeature(self, storageFeaturePath) ? !ignoreMissing : false,
        .expression = expression), sortAsc ? sortOrderAsc : sortOrderDesc);

    const String *fileListJson = jsonFromVar(varNewVarLst(varLstNewStrLst(fileList)));

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
    StorageManifestXsCallbackData data = {.storage = self, .json = strNew("{"), .pathRoot = pathExp, .filter = filter};

    // If a path is specified
    StorageInfo info = storageInfoP(self, pathExp, .ignoreMissing = true);

    if (!info.exists || info.type == storageTypePath)
    {
        storageInfoListP(
            self, data.pathRoot, storageManifestXsCallback, &data,
            .errorOnMissing = storageFeature(self, storageFeaturePath) ? true : false);
    }
    // Else a file is specified
    else
    {
        info.name = strBase(storagePathP(self, pathExp));
        strCat(data.json, strPtr(storageManifestXsInfo(NULL, &info)));
    }

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
    if (storageFeature(self, storageFeaturePath))
        storagePathCreateP(
            self, pathExp, .mode = mode ? cvtZToMode(strPtr(mode)) : 0, .errorOnExists = !ignoreExists,
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
    RETVAL = true;

    if (storageFeature(self, storageFeaturePath))
        RETVAL = storagePathExistsP(self, pathExp);
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
    String *path = storagePathP(self, pathExp);
    RETVAL = newSVpv((char *)strPtr(path), strSize(path));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
pathRemove(self, pathExp, ignoreMissing, recurse)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
    bool ignoreMissing
    bool recurse
CODE:
    storagePathRemoveP(
        self, pathExp, .errorOnMissing = storageFeature(self, storageFeaturePath) ? !ignoreMissing : false, .recurse = recurse);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
pathSync(self, pathExp)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *pathExp = STR_NEW_SV($arg);
CODE:
    storagePathSyncP(self, pathExp);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
UV
put(self, write, buffer)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageWrite write
    const Buffer *buffer = BUF_CONST_SV($arg);
CODE:
    storagePutP(write, buffer);
    RETVAL = buffer ? bufUsed(buffer) : 0;
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
bool
readDrain(self, read)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::StorageRead read
CODE:
    RETVAL = false;

    // Read and discard all IO (this is useful for processing filters)
    if (ioReadOpen(storageReadIo(read)))
    {
        Buffer *buffer = bufNew(ioBufferSize());

        do
        {
            ioRead(storageReadIo(read), buffer);
            bufUsedZero(buffer);
        }
        while (!ioReadEof(storageReadIo(read)));

        ioReadClose(storageReadIo(read));
        RETVAL = true;
    }
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
void
remove(self, fileExp, ignoreMissing)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
    const String *fileExp = STR_NEW_SV($arg);
    bool ignoreMissing
CODE:
    storageRemoveP(self, fileExp, .errorOnMissing = storageFeature(self, storageFeaturePath) ? !ignoreMissing : false);
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
cipherType(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
CODE:
    if (cfgOptionStr(cfgOptRepoCipherType) == NULL || cipherType(cfgOptionStr(cfgOptRepoCipherType)) == cipherTypeNone)
        RETVAL = NULL;
    else
        RETVAL = strPtr(cfgOptionStr(cfgOptRepoCipherType));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
cipherPass(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
CODE:
    RETVAL = strPtr(cfgOptionStr(cfgOptRepoCipherPass));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

####################################################################################################################################
const char *
type(self)
PREINIT:
    MEM_CONTEXT_XS_TEMP_BEGIN()
    {
INPUT:
    pgBackRest::LibC::Storage self
CODE:
    RETVAL = strPtr(storageType(self));
OUTPUT:
    RETVAL
CLEANUP:
    }
    MEM_CONTEXT_XS_TEMP_END();

MODULE = pgBackRest::LibC PACKAGE = pgBackRest::LibC

####################################################################################################################################
void
storageRepoFree()
CODE:
    storageHelperFree();
