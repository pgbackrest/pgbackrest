/***********************************************************************************************************************************
Store manifest data in memory
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(BACKUP_MANIFEST_FILE_STR,                             BACKUP_MANIFEST_FILE);

STRING_EXTERN(MANIFEST_TARGET_PGDATA_STR,                           MANIFEST_TARGET_PGDATA);
STRING_EXTERN(MANIFEST_TARGET_PGTBLSPC_STR,                         MANIFEST_TARGET_PGTBLSPC);

// All block incremental sizes must be divisible by this factor
#define BLOCK_INCR_SIZE_FACTOR                                      8192

/***********************************************************************************************************************************
// Add owner to the owner list if it is not there already and return the pointer. This saves a lot of space.
***********************************************************************************************************************************/
static const String *
manifestOwnerCache(Manifest *const this, const String *const owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, owner);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    if (owner != NULL)
        FUNCTION_TEST_RETURN_CONST(STRING, strLstAddIfMissing(this->ownerList, owner));

    FUNCTION_TEST_RETURN_CONST(STRING, NULL);
}

/***********************************************************************************************************************************
Db functions
***********************************************************************************************************************************/
static void
manifestDbAdd(Manifest *const this, const ManifestDb *const db)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_DB, db);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(db != NULL);
    ASSERT(db->name != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.dbList))
    {
        const ManifestDb dbAdd =
        {
            .id = db->id,
            .lastSystemId = db->lastSystemId,
            .name = strDup(db->name),
        };

        lstAdd(this->pub.dbList, &dbAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
File functions
***********************************************************************************************************************************/
// Base time used as a delta to reduce the size of packed timestamps. This will be set on the first call to manifestFilePack().
static time_t manifestPackBaseTime = -1;

// Flags used to reduce the size of packed data. They should be ordered from most to least likely and can be reordered at will.
typedef enum
{
    manifestFilePackFlagChecksum,
    manifestFilePackFlagChecksumRepo,
    manifestFilePackFlagReference,
    manifestFilePackFlagBundle,
    manifestFilePackFlagBlockIncr,
    manifestFilePackFlagCopy,
    manifestFilePackFlagDelta,
    manifestFilePackFlagResume,
    manifestFilePackFlagChecksumPage,
    manifestFilePackFlagChecksumPageError,
    manifestFilePackFlagChecksumPageErrorList,
    manifestFilePackFlagSizeOriginal,
    manifestFilePackFlagMode,
    manifestFilePackFlagUser,
    manifestFilePackFlagUserNull,
    manifestFilePackFlagGroup,
    manifestFilePackFlagGroupNull,
} ManifestFilePackFlag;

// Pack file into a compact format to save memory
static ManifestFilePack *
manifestFilePack(const Manifest *const manifest, const ManifestFile *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
    FUNCTION_TEST_END();

    ASSERT(manifest != NULL);
    ASSERT(file != NULL);

    uint8_t buffer[512];
    size_t bufferPos = 0;

    // Flags
    uint64_t flag = 0;

    if (file->checksumSha1 != NULL)
        flag |= 1 << manifestFilePackFlagChecksum;

    if (file->checksumRepoSha1 != NULL)
        flag |= 1 << manifestFilePackFlagChecksumRepo;

    if (file->copy)
        flag |= 1 << manifestFilePackFlagCopy;

    if (file->delta)
        flag |= 1 << manifestFilePackFlagDelta;

    if (file->resume)
        flag |= 1 << manifestFilePackFlagResume;

    if (file->checksumPage)
        flag |= 1 << manifestFilePackFlagChecksumPage;

    if (file->checksumPageError)
        flag |= 1 << manifestFilePackFlagChecksumPageError;

    if (file->checksumPageErrorList != NULL)
        flag |= 1 << manifestFilePackFlagChecksumPageErrorList;

    if (file->reference != NULL)
        flag |= 1 << manifestFilePackFlagReference;

    if (file->bundleId != 0)
        flag |= 1 << manifestFilePackFlagBundle;

    if (file->blockIncrSize != 0)
        flag |= 1 << manifestFilePackFlagBlockIncr;

    if (file->sizeOriginal != file->size)
        flag |= 1 << manifestFilePackFlagSizeOriginal;

    if (file->mode != manifest->fileModeDefault)
        flag |= 1 << manifestFilePackFlagMode;

    if (file->user == NULL)
        flag |= 1 << manifestFilePackFlagUserNull;
    else if (!strEq(file->user, manifest->fileUserDefault))
        flag |= 1 << manifestFilePackFlagUser;

    if (file->group == NULL)
        flag |= 1 << manifestFilePackFlagGroupNull;
    else if (!strEq(file->group, manifest->fileGroupDefault))
        flag |= 1 << manifestFilePackFlagGroup;

    cvtUInt64ToVarInt128(flag, buffer, &bufferPos, sizeof(buffer));

    // Size
    cvtUInt64ToVarInt128(file->size, buffer, &bufferPos, sizeof(buffer));

    // Original size
    if (flag & (1 << manifestFilePackFlagSizeOriginal))
        cvtUInt64ToVarInt128(file->sizeOriginal, buffer, &bufferPos, sizeof(buffer));

    // Use the first timestamp that appears as the base for all other timestamps. Ideally we would like a timestamp as close to the
    // middle as possible but it doesn't seem worth doing the calculation.
    if (manifestPackBaseTime == -1)
        manifestPackBaseTime = file->timestamp;

    // Timestamp
    cvtUInt64ToVarInt128(cvtInt64ToZigZag(manifestPackBaseTime - file->timestamp), buffer, &bufferPos, sizeof(buffer));

    // SHA1 checksum
    if (file->checksumSha1 != NULL)
    {
        memcpy((uint8_t *)buffer + bufferPos, file->checksumSha1, HASH_TYPE_SHA1_SIZE);
        bufferPos += HASH_TYPE_SHA1_SIZE;
    }

    // SHA1 repo checksum
    if (file->checksumRepoSha1 != NULL)
    {
        memcpy((uint8_t *)buffer + bufferPos, file->checksumRepoSha1, HASH_TYPE_SHA1_SIZE);
        bufferPos += HASH_TYPE_SHA1_SIZE;
    }

    // Reference
    if (file->reference != NULL)
    {
        cvtUInt64ToVarInt128(
            strLstFindIdxP(manifest->pub.referenceList, file->reference, .required = true), buffer, &bufferPos, sizeof(buffer));
    }

    // Mode
    if (flag & (1 << manifestFilePackFlagMode))
        cvtUInt64ToVarInt128(file->mode, buffer, &bufferPos, sizeof(buffer));

    // User/group
    if (flag & (1 << manifestFilePackFlagUser))
        cvtUInt64ToVarInt128((uintptr_t)file->user, buffer, &bufferPos, sizeof(buffer));

    if (flag & (1 << manifestFilePackFlagGroup))
        cvtUInt64ToVarInt128((uintptr_t)file->group, buffer, &bufferPos, sizeof(buffer));

    // Repo size
    cvtUInt64ToVarInt128(file->sizeRepo, buffer, &bufferPos, sizeof(buffer));

    // Bundle
    if (flag & (1 << manifestFilePackFlagBundle))
    {
        cvtUInt64ToVarInt128(file->bundleId, buffer, &bufferPos, sizeof(buffer));
        cvtUInt64ToVarInt128(file->bundleOffset, buffer, &bufferPos, sizeof(buffer));
    }

    // Block incremental
    if (flag & (1 << manifestFilePackFlagBlockIncr))
    {
        ASSERT(file->blockIncrSize % BLOCK_INCR_SIZE_FACTOR == 0);

        cvtUInt64ToVarInt128(file->blockIncrSize / BLOCK_INCR_SIZE_FACTOR, buffer, &bufferPos, sizeof(buffer));
        cvtUInt64ToVarInt128(file->blockIncrChecksumSize, buffer, &bufferPos, sizeof(buffer));
        cvtUInt64ToVarInt128(file->blockIncrMapSize, buffer, &bufferPos, sizeof(buffer));
    }

    // Allocate memory for the file pack
    const size_t nameSize = strSize(file->name) + 1;

    uint8_t *const result = memNew(
        sizeof(StringPub) + nameSize + bufferPos +
        (file->checksumPageErrorList != NULL ?
             ALIGN_OFFSET(StringPub, nameSize + bufferPos) + sizeof(StringPub) + strSize(file->checksumPageErrorList) + 1 : 0));

    // Create string object for the file name
    *(StringPub *)result = (StringPub){.size = (unsigned int)strSize(file->name), .buffer = (char *)result + sizeof(StringPub)};
    size_t resultPos = sizeof(StringPub);

    memcpy(result + resultPos, (const uint8_t *)strZ(file->name), nameSize);
    resultPos += nameSize;

    // Copy pack data
    memcpy(result + resultPos, buffer, bufferPos);

    // Create string object for the checksum error list
    if (file->checksumPageErrorList != NULL)
    {
        resultPos += bufferPos + ALIGN_OFFSET(StringPub, nameSize + bufferPos);

        *(StringPub *)(result + resultPos) = (StringPub)
        {
            .size = (unsigned int)strSize(file->checksumPageErrorList),
            .buffer = (char *)result + resultPos + sizeof(StringPub),
        };

        resultPos += sizeof(StringPub);
        memcpy(result + resultPos, (const uint8_t *)strZ(file->checksumPageErrorList), strSize(file->checksumPageErrorList) + 1);
    }

    FUNCTION_TEST_RETURN_TYPE_P(ManifestFilePack, (ManifestFilePack *)result);
}

FN_EXTERN ManifestFile
manifestFileUnpack(const Manifest *const manifest, const ManifestFilePack *const filePack)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM_P(VOID, filePack);
    FUNCTION_TEST_END();

    ASSERT(filePack != NULL);
    ASSERT(manifestPackBaseTime != -1);

    ManifestFile result = {0};
    size_t bufferPos = 0;

    // Name
    result.name = (const String *)filePack;
    bufferPos += sizeof(StringPub) + strSize(result.name) + 1;

    // Flags
    const uint64_t flag = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);

    result.copy = (flag >> manifestFilePackFlagCopy) & 1;
    result.delta = (flag >> manifestFilePackFlagDelta) & 1;
    result.resume = (flag >> manifestFilePackFlagResume) & 1;

    // Size
    result.size = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);

    // Original size
    if (flag & (1 << manifestFilePackFlagSizeOriginal))
        result.sizeOriginal = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    else
        result.sizeOriginal = result.size;

    // Timestamp
    result.timestamp =
        manifestPackBaseTime - (time_t)cvtInt64FromZigZag(cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX));

    // Checksum page
    result.checksumPage = (flag >> manifestFilePackFlagChecksumPage) & 1;

    // SHA1 checksum
    if (flag & (1 << manifestFilePackFlagChecksum))
    {
        result.checksumSha1 = (const uint8_t *)filePack + bufferPos;
        bufferPos += HASH_TYPE_SHA1_SIZE;
    }

    // SHA1 repo checksum
    if (flag & (1 << manifestFilePackFlagChecksumRepo))
    {
        result.checksumRepoSha1 = (const uint8_t *)filePack + bufferPos;
        bufferPos += HASH_TYPE_SHA1_SIZE;
    }

    // Reference
    if (flag & (1 << manifestFilePackFlagReference))
    {
        result.reference = strLstGet(
            manifest->pub.referenceList, (unsigned int)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX));
    }

    // Mode
    if (flag & (1 << manifestFilePackFlagMode))
        result.mode = (mode_t)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    else
        result.mode = manifest->fileModeDefault;

    // User/group
    if (flag & (1 << manifestFilePackFlagUser))
        result.user = (const String *)(uintptr_t)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    else if (!(flag & (1 << manifestFilePackFlagUserNull)))
        result.user = manifest->fileUserDefault;

    if (flag & (1 << manifestFilePackFlagGroup))
        result.group = (const String *)(uintptr_t)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    else if (!(flag & (1 << manifestFilePackFlagGroupNull)))
        result.group = manifest->fileGroupDefault;

    // Repo size
    result.sizeRepo = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);

    // Bundle
    if (flag & (1 << manifestFilePackFlagBundle))
    {
        result.bundleId = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
        result.bundleOffset = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    }

    // Block incremental
    if (flag & (1 << manifestFilePackFlagBlockIncr))
    {
        result.blockIncrSize =
            (size_t)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX) * BLOCK_INCR_SIZE_FACTOR;
        result.blockIncrChecksumSize = (size_t)cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
        result.blockIncrMapSize = cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX);
    }

    // Checksum page error
    result.checksumPageError = flag & (1 << manifestFilePackFlagChecksumPageError) ? true : false;

    if (flag & (1 << manifestFilePackFlagChecksumPageErrorList))
        result.checksumPageErrorList = (const String *)((const uint8_t *)filePack + bufferPos + ALIGN_OFFSET(StringPub, bufferPos));

    FUNCTION_TEST_RETURN_TYPE(ManifestFile, result);
}

FN_EXTERN void
manifestFileAdd(Manifest *const this, ManifestFile *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(file->name != NULL);

    file->user = manifestOwnerCache(this, file->user);
    file->group = manifestOwnerCache(this, file->group);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.fileList))
    {
        const ManifestFilePack *const filePack = manifestFilePack(this, file);
        lstAdd(this->pub.fileList, &filePack);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

// Update file pack by creating a new one and then freeing the old one
static void
manifestFilePackUpdate(Manifest *const this, ManifestFilePack **const filePack, const ManifestFile *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM_P(VOID, filePack);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(filePack != NULL);
    ASSERT(file != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.fileList))
    {
        ManifestFilePack *const filePackOld = *filePack;
        *filePack = manifestFilePack(this, file);
        memFree(filePackOld);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

static ManifestFilePack **
manifestFilePackFindInternal(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    ManifestFilePack **const filePack = lstFind(this->pub.fileList, &name);

    if (filePack == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest file list", strZ(name));

    FUNCTION_TEST_RETURN_TYPE_PP(ManifestFilePack, filePack);
}

const ManifestFilePack *
manifestFilePackFind(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN_TYPE_P(ManifestFilePack, *manifestFilePackFindInternal(this, name));
}

FN_EXTERN void
manifestFileRemove(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    if (!lstRemove(this->pub.fileList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest file list", strZ(name));

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
manifestFileUpdate(Manifest *const this, const ManifestFile *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(
        (!file->checksumPage && !file->checksumPageError && file->checksumPageErrorList == NULL) ||
        (file->checksumPage && !file->checksumPageError && file->checksumPageErrorList == NULL) ||
        (file->checksumPage && file->checksumPageError));
    ASSERT(file->size != 0 || (file->bundleId == 0 && file->bundleOffset == 0));

    ManifestFilePack **const filePack = manifestFilePackFindInternal(this, file->name);
    manifestFilePackUpdate(this, filePack, file);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Link functions
***********************************************************************************************************************************/
FN_EXTERN void
manifestLinkAdd(Manifest *const this, const ManifestLink *const link)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_LINK, link);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(link != NULL);
    ASSERT(link->name != NULL);
    ASSERT(link->destination != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.linkList))
    {
        const ManifestLink linkAdd =
        {
            .destination = strDup(link->destination),
            .name = strDup(link->name),
            .group = manifestOwnerCache(this, link->group),
            .user = manifestOwnerCache(this, link->user),
        };

        lstAdd(this->pub.linkList, &linkAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN ManifestLink *
manifestLinkFind(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    ManifestLink *const result = lstFind(this->pub.linkList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest link list", strZ(name));

    FUNCTION_TEST_RETURN(MANIFEST_LINK, result);
}

FN_EXTERN void
manifestLinkRemove(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    if (!lstRemove(this->pub.linkList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest link list", strZ(name));

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
manifestLinkUpdate(const Manifest *const this, const String *const name, const String *const destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(STRING, destination);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);
    ASSERT(destination != NULL);

    ManifestLink *const link = (ManifestLink *const)manifestLinkFind(this, name);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.linkList))
    {
        if (!strEq(link->destination, destination))
            link->destination = strDup(destination);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Path functions
***********************************************************************************************************************************/
static void
manifestPathAdd(Manifest *const this, const ManifestPath *const path)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_PATH, path);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(path != NULL);
    ASSERT(path->name != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.pathList))
    {
        const ManifestPath pathAdd =
        {
            .mode = path->mode,
            .name = strDup(path->name),
            .group = manifestOwnerCache(this, path->group),
            .user = manifestOwnerCache(this, path->user),
        };

        lstAdd(this->pub.pathList, &pathAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN const ManifestPath *
manifestPathFind(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestPath *const result = lstFind(this->pub.pathList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest path list", strZ(name));

    FUNCTION_TEST_RETURN_CONST(MANIFEST_PATH, result);
}

/***********************************************************************************************************************************
Manifest functions
***********************************************************************************************************************************/
FN_EXTERN void
manifestTargetAdd(Manifest *const this, const ManifestTarget *const target)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, target);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(target != NULL);
    ASSERT(target->path != NULL);
    ASSERT(target->name != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.targetList))
    {
        const ManifestTarget targetAdd =
        {
            .file = strDup(target->file),
            .name = strDup(target->name),
            .path = strDup(target->path),
            .tablespaceId = target->tablespaceId,
            .tablespaceName = strDup(target->tablespaceName),
            .type = target->type,
        };

        lstAdd(this->pub.targetList, &targetAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN ManifestTarget *
manifestTargetFind(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    ManifestTarget *const result = lstFind(this->pub.targetList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest target list", strZ(name));

    FUNCTION_TEST_RETURN(MANIFEST_TARGET, result);
}

FN_EXTERN String *
manifestTargetPath(const Manifest *const this, const ManifestTarget *const target)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, target);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(target != NULL);

    // If the target path is already absolute then just return it
    if (strBeginsWith(target->path, FSLASH_STR))
        FUNCTION_TEST_RETURN(STRING, strDup(target->path));

    // Construct it from the base pg path and a relative path
    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const pgPath = strCat(strNew(), strPath(manifestPathPg(target->name)));

        if (strSize(pgPath) != 0)
            strCatZ(pgPath, "/");

        strCat(pgPath, target->path);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strPathAbsolute(pgPath, manifestTargetBase(this)->path);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(STRING, result);
}

FN_EXTERN void
manifestTargetRemove(const Manifest *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    if (!lstRemove(this->pub.targetList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest target list", strZ(name));

    FUNCTION_TEST_RETURN_VOID();
}

FN_EXTERN void
manifestTargetUpdate(const Manifest *const this, const String *const name, const String *const path, const String *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(STRING, path);
        FUNCTION_TEST_PARAM(STRING, file);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);
    ASSERT(path != NULL);

    ManifestTarget *const target = (ManifestTarget *)manifestTargetFind(this, name);

    ASSERT((target->file == NULL && file == NULL) || (target->file != NULL && file != NULL));

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.targetList))
    {
        if (!strEq(target->path, path))
            target->path = strDup(path);

        if (!strEq(target->file, file))
            target->file = strDup(file);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}
