/***********************************************************************************************************************************
Backup Manifest Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/storage.h"
#include "version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(BACKUP_MANIFEST_FILE_STR,                             BACKUP_MANIFEST_FILE);

STRING_EXTERN(MANIFEST_TARGET_PGDATA_STR,                           MANIFEST_TARGET_PGDATA);
STRING_EXTERN(MANIFEST_TARGET_PGTBLSPC_STR,                         MANIFEST_TARGET_PGTBLSPC);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Manifest
{
    ManifestPub pub;                                                // Publicly accessible variables
    StringList *ownerList;                                          // List of users/groups

    const String *fileUserDefault;                                  // Default file user name
    const String *fileGroupDefault;                                 // Default file group name
    mode_t fileModeDefault;                                         // Default file mode
};

/***********************************************************************************************************************************
Internal functions to add types to their lists
***********************************************************************************************************************************/
// Helper to add owner to the owner list if it is not there already and return the pointer.  This saves a lot of space.
static const String *
manifestOwnerCache(Manifest *this, const String *owner)
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

static void
manifestDbAdd(Manifest *this, const ManifestDb *db)
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
        ManifestDb dbAdd =
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

// Base time used as a delta to reduce the size of packed timestamps. This will be set on the first call to manifestFilePack().
static time_t manifestPackBaseTime = -1;

// Flags used to reduce the size of packed data. They should be ordered from most to least likely and can be reordered at will.
typedef enum
{
    manifestFilePackFlagReference,
    manifestFilePackFlagBundle,
    manifestFilePackFlagCopy,
    manifestFilePackFlagDelta,
    manifestFilePackFlagResume,
    manifestFilePackFlagChecksumPage,
    manifestFilePackFlagChecksumPageError,
    manifestFilePackFlagChecksumPageErrorList,
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

    // Use the first timestamp that appears as the base for all other timestamps. Ideally we would like a timestamp as close to the
    // middle as possible but it doesn't seem worth doing the calculation.
    if (manifestPackBaseTime == -1)
        manifestPackBaseTime = file->timestamp;

    // Timestamp
    cvtUInt64ToVarInt128(cvtInt64ToZigZag(manifestPackBaseTime - file->timestamp), buffer, &bufferPos, sizeof(buffer));

    // SHA1 checksum
    strcpy((char *)buffer + bufferPos, file->checksumSha1);
    bufferPos += HASH_TYPE_SHA1_SIZE_HEX + 1;

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

    // Allocate memory for the file pack
    const size_t nameSize = strSize(file->name) + 1;

    uint8_t *const result = memNew(
        sizeof(StringPub) + nameSize + bufferPos + (file->checksumPageErrorList != NULL ?
            ALIGN_OFFSET(StringPub, nameSize + bufferPos) + sizeof(StringPub) + strSize(file->checksumPageErrorList) + 1 : 0));

    // Create string object for the file name
    *(StringPub *)result = (StringPub){.size = (unsigned int)strSize(file->name), .buffer = (char *)result + sizeof(StringPub)};
    size_t resultPos = sizeof(StringPub);

    memcpy(result + resultPos, (uint8_t *)strZ(file->name), nameSize);
    resultPos += nameSize;

    // Copy pack data
    memcpy(result + resultPos, buffer, bufferPos);

    // Create string object for the checksum error list
    if (file->checksumPageErrorList != NULL)
    {
        resultPos += bufferPos + ALIGN_OFFSET(StringPub, nameSize + bufferPos);

        *(StringPub *)(result + resultPos) = (StringPub)
            {.size = (unsigned int)strSize(file->checksumPageErrorList), .buffer = (char *)result + resultPos + sizeof(StringPub)};
        resultPos += sizeof(StringPub);

        memcpy(result + resultPos, (uint8_t *)strZ(file->checksumPageErrorList), strSize(file->checksumPageErrorList) + 1);
    }

    FUNCTION_TEST_RETURN_TYPE_P(ManifestFilePack, (ManifestFilePack *)result);
}

ManifestFile
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

    // Timestamp
    result.timestamp =
        manifestPackBaseTime - (time_t)cvtInt64FromZigZag(cvtUInt64FromVarInt128((const uint8_t *)filePack, &bufferPos, UINT_MAX));

    // Checksum page
    result.checksumPage = (flag >> manifestFilePackFlagChecksumPage) & 1;

    // SHA1 checksum
    memcpy(result.checksumSha1, (const uint8_t *)filePack + bufferPos, HASH_TYPE_SHA1_SIZE_HEX + 1);
    bufferPos += HASH_TYPE_SHA1_SIZE_HEX + 1;

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

    // Checksum page error
    result.checksumPageError = flag & (1 << manifestFilePackFlagChecksumPageError) ? true : false;

    if (flag & (1 << manifestFilePackFlagChecksumPageErrorList))
        result.checksumPageErrorList = (const String *)((const uint8_t *)filePack + bufferPos + ALIGN_OFFSET(StringPub, bufferPos));

    FUNCTION_TEST_RETURN_TYPE(ManifestFile, result);
}

void
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

void
manifestLinkAdd(Manifest *this, const ManifestLink *link)
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
        ManifestLink linkAdd =
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

static void
manifestPathAdd(Manifest *this, const ManifestPath *path)
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
        ManifestPath pathAdd =
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

void
manifestTargetAdd(Manifest *this, const ManifestTarget *target)
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
        ManifestTarget targetAdd =
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

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static Manifest *
manifestNewInternal(void)
{
    FUNCTION_TEST_VOID();

    Manifest *this = OBJ_NEW_ALLOC();

    *this = (Manifest)
    {
        .pub =
        {
            .memContext = memContextCurrent(),
            .dbList = lstNewP(sizeof(ManifestDb), .comparator = lstComparatorStr),
            .fileList = lstNewP(sizeof(ManifestFilePack *), .comparator = lstComparatorStr),
            .linkList = lstNewP(sizeof(ManifestLink), .comparator = lstComparatorStr),
            .pathList = lstNewP(sizeof(ManifestPath), .comparator = lstComparatorStr),
            .targetList = lstNewP(sizeof(ManifestTarget), .comparator = lstComparatorStr),
            .referenceList = strLstNew(),
        },
        .ownerList = strLstNew(),
    };

    FUNCTION_TEST_RETURN(MANIFEST, this);
}

/***********************************************************************************************************************************
Ensure that symlinks do not point to the same file, directory, or subdirectory of another link

There are two implementations: manifestLinkCheck(), which is externed, and manifestLinkCheckOne(), which is intended to be
used internally during processing. manifestLinkCheck() works simply by calling manifestLinkCheckOne() for every link in the target
list. manifestLinkCheckOne() is optimized to work quickly on a single link.
***********************************************************************************************************************************/
// Data needed in link list
typedef struct ManifestLinkCheckItem
{
    const String *path;                                             // Link destination path terminated with /
    const String *file;                                             // Link file if a file link
    unsigned int targetIdx;                                         // Index of target used for error messages
} ManifestLinkCheckItem;

// Persistent data needed during processing of manifestLinkCheck/One()
typedef struct ManifestLinkCheck
{
    const String *basePath;                                         // Base data path (initialized on first call)
    List *linkList;                                                 // Current list of link destination paths
} ManifestLinkCheck;

// Helper to initialize the link data
static ManifestLinkCheck
manifestLinkCheckInit(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_TEST_RETURN_TYPE(
        ManifestLinkCheck, (ManifestLinkCheck){.linkList = lstNewP(sizeof(ManifestLinkCheckItem), .comparator = lstComparatorStr)});
}

// Helper to check a single link specified by targetIdx
static void
manifestLinkCheckOne(const Manifest *this, ManifestLinkCheck *linkCheck, unsigned int targetIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM_P(VOID, linkCheck);
        FUNCTION_LOG_PARAM(UINT, targetIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(linkCheck != NULL);
    ASSERT(linkCheck->linkList != NULL);
    ASSERT(targetIdx < manifestTargetTotal(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const ManifestTarget *target1 = manifestTarget(this, targetIdx);

        // Only check link targets
        if (target1->type == manifestTargetTypeLink)
        {
            // Create link destination path for comparison with other paths. It must end in / so subpaths can be detected without
            // matching valid partial path names at the end of the path.
            const String *path = NULL;

            MEM_CONTEXT_BEGIN(lstMemContext(linkCheck->linkList))
            {
                path = strNewFmt("%s/", strZ(manifestTargetPath(this, target1)));

                // Get base bath
                if (linkCheck->basePath == NULL)
                {
                    linkCheck->basePath = strNewFmt(
                        "%s/", strZ(manifestTargetPath(this, manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR))));
                }
            }
            MEM_CONTEXT_END();

            // Check that link destination is not in base data path
            if (strBeginsWith(path, linkCheck->basePath))
            {
                THROW_FMT(
                    LinkDestinationError,
                    "link '%s' destination '%s' is in PGDATA",
                    strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)));
            }

            // Check if the link destination path already exists
            const ManifestLinkCheckItem *const link = lstFind(linkCheck->linkList, &path);

            if (link != NULL)
            {
                // If both links are files make sure they don't link to the same file
                if (target1->file != NULL && link->file != NULL)
                {
                    if (strEq(target1->file, link->file))
                    {
                        const ManifestTarget *const target2 = manifestTarget(this, link->targetIdx);

                        THROW_FMT(
                            LinkDestinationError,
                            "link '%s' (%s/%s) destination is the same file as link '%s' (%s/%s)",
                            strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)), strZ(target1->file),
                            strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)), strZ(target2->file));
                    }
                }
                // Else error because one of the links is a path and cannot link to the same path as another file/path link
                else
                {
                    const ManifestTarget *const target2 = manifestTarget(this, link->targetIdx);

                    THROW_FMT(
                        LinkDestinationError,
                        "link '%s' (%s) destination is the same directory as link '%s' (%s)",
                        strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)),
                        strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)));
                }
            }
            // Else add to the link list and check against other links
            else
            {
                // Add the link destination path and sort
                lstAdd(linkCheck->linkList, &(ManifestLinkCheckItem){.path = path, .file = target1->file, .targetIdx = targetIdx});
                lstSort(linkCheck->linkList, sortOrderAsc);

                // Find the path in the sorted list
                unsigned int linkIdx = lstFindIdx(linkCheck->linkList, &path);
                ASSERT(linkIdx != LIST_NOT_FOUND);

                // Check path links against other links (file links have already been checked)
                if (target1->file == NULL)
                {
                    // Check the link destination path to be sure it is not a subpath of a prior link destination path
                    for (unsigned int priorLinkIdx = linkIdx - 1; (int)priorLinkIdx >= 0; priorLinkIdx--)
                    {
                        const ManifestLinkCheckItem *const priorLink = lstGet(linkCheck->linkList, priorLinkIdx);

                        // Skip file links since they are allowed to be in the same path with each other and in the parent path of a
                        // linked destination path.
                        if (priorLink->file != NULL)
                            continue;

                        if (strBeginsWith(path, priorLink->path))
                        {
                            const ManifestTarget *const target2 = manifestTarget(this, priorLink->targetIdx);

                            THROW_FMT(
                                LinkDestinationError,
                                "link '%s' (%s) destination is a subdirectory of link '%s' (%s)",
                                strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)),
                                strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)));
                        }

                        // Stop once the first prior path link has been checked since it must be a parent (if there is one)
                        break;
                    }

                    // Check the link destination path to be sure it is not a parent path of a subsequent link destination path
                    for (unsigned int nextLinkIdx = linkIdx + 1; nextLinkIdx < lstSize(linkCheck->linkList); nextLinkIdx++)
                    {
                        const ManifestLinkCheckItem *const nextLink = lstGet(linkCheck->linkList, nextLinkIdx);

                        // Skip file links since they are allowed to be in the same path with each other and in the parent path of a
                        // linked destination path.
                        if (nextLink->file != NULL)
                            continue;

                        if (strBeginsWith(nextLink->path, path))
                        {
                            const ManifestTarget *const target2 = manifestTarget(this, nextLink->targetIdx);

                            THROW_FMT(
                                LinkDestinationError,
                                "link '%s' (%s) destination is a subdirectory of link '%s' (%s)",
                                strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)),
                                strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)));
                        }

                        // Stop once the first next path link has been checked since it must be a subpath (if there is one)
                        break;
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

void
manifestLinkCheck(const Manifest *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check all links
        ManifestLinkCheck linkCheck = manifestLinkCheckInit();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(this); targetIdx++)
            manifestLinkCheckOne(this, &linkCheck, targetIdx);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct ManifestBuildData
{
    Manifest *manifest;                                             // Manifest being build
    const Storage *storagePg;                                       // PostgreSQL storage
    const String *tablespaceId;                                     // Tablespace id if PostgreSQL version has one
    bool online;                                                    // Is this an online backup?
    bool checksumPage;                                              // Are page checksums being checked?
    const String *manifestWalName;                                  // Wal manifest name for this version of PostgreSQL
    RegExp *dbPathExp;                                              // Identify paths containing relations
    RegExp *tempRelationExp;                                        // Identify temp relations
    const Pack *tablespaceList;                                     // List of tablespaces in the database
    ManifestLinkCheck *linkCheck;                                   // List of links found during build (used for prefix check)
    StringList *excludeContent;                                     // Exclude contents of directories
    StringList *excludeSingle;                                      // Exclude a single file/link/path
} ManifestBuildData;

// Process files/links/paths and add them to the manifest
static void
manifestBuildInfo(
    ManifestBuildData *const buildData, const String *manifestParentName, const String *pgPath, const bool dbPath,
    const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buildData);
        FUNCTION_TEST_PARAM(STRING, manifestParentName);
        FUNCTION_TEST_PARAM(STRING, pgPath);
        FUNCTION_TEST_PARAM(BOOL, dbPath);
        FUNCTION_TEST_PARAM(STORAGE_INFO, *info);
    FUNCTION_TEST_END();

    ASSERT(buildData != NULL);
    ASSERT(manifestParentName != NULL);
    ASSERT(pgPath != NULL);
    ASSERT(info != NULL);

    // Skip any path/file/link that begins with pgsql_tmp.  The files are removed when the server is restarted and the directories
    // are recreated.
    if (strBeginsWithZ(info->name, PG_PREFIX_PGSQLTMP))
        FUNCTION_TEST_RETURN_VOID();

    // Get build data
    unsigned int pgVersion = buildData->manifest->pub.data.pgVersion;

    // Construct the name used to identify this file/link/path in the manifest
    const String *manifestName = strNewFmt("%s/%s", strZ(manifestParentName), strZ(info->name));

    // Skip excluded files/links/paths
    if (buildData->excludeSingle != NULL && strLstExists(buildData->excludeSingle, manifestName))
    {
        LOG_INFO_FMT(
            "exclude '%s/%s' from backup using '%s' exclusion", strZ(pgPath), strZ(info->name),
            strZ(strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA))));

        FUNCTION_TEST_RETURN_VOID();
    }

    // Process storage types
    switch (info->type)
    {
        // Add paths
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypePath:
        {
            // There should not be any paths in pg_tblspc
            if (strEqZ(manifestParentName, MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC))
            {
                THROW_FMT(
                    LinkExpectedError, "'%s' is not a symlink - " MANIFEST_TARGET_PGTBLSPC " should contain only symlinks",
                    strZ(manifestName));
            }

            // Add path to manifest
            ManifestPath path =
            {
                .name = manifestName,
                .mode = info->mode,
                .user = info->user,
                .group = info->group,
            };

            manifestPathAdd(buildData->manifest, &path);

            // Skip excluded path content
            if (buildData->excludeContent != NULL && strLstExists(buildData->excludeContent, manifestName))
            {
                LOG_INFO_FMT(
                    "exclude contents of '%s/%s' from backup using '%s/' exclusion", strZ(pgPath), strZ(info->name),
                    strZ(strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA))));

                FUNCTION_TEST_RETURN_VOID();
            }

            // Skip the contents of these paths if they exist in the base path since they won't be reused after recovery
            if (strEq(manifestParentName, MANIFEST_TARGET_PGDATA_STR))
            {
                // Skip pg_dynshmem/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGDYNSHMEM) && pgVersion >= PG_VERSION_94)
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_notify/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGNOTIFY))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_replslot/* since these files are generally not useful after a restore
                if (strEqZ(info->name, PG_PATH_PGREPLSLOT) && pgVersion >= PG_VERSION_94)
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_serial/* since these files are reset
                if (strEqZ(info->name, PG_PATH_PGSERIAL) && pgVersion >= PG_VERSION_91)
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_snapshots/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGSNAPSHOTS) && pgVersion >= PG_VERSION_92)
                    FUNCTION_TEST_RETURN_VOID();

                // Skip temporary statistics in pg_stat_tmp even when stats_temp_directory is set because PGSS_TEXT_FILE is always
                // created there in PostgreSQL < 15. PostgreSQL >= 15 no longer uses this directory, but it may be used by
                // extensions such as pg_stat_statements so it should still be excluded.
                if (strEqZ(info->name, PG_PATH_PGSTATTMP))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_subtrans/* since these files are reset
                if (strEqZ(info->name, PG_PATH_PGSUBTRANS))
                    FUNCTION_TEST_RETURN_VOID();
            }

            // Skip the contents of archive_status when online
            if (buildData->online && strEq(manifestParentName, buildData->manifestWalName) &&
                strEqZ(info->name, PG_PATH_ARCHIVE_STATUS))
            {
                FUNCTION_TEST_RETURN_VOID();
            }

            // Recurse into the path
            const String *const pgPathSub = strNewFmt("%s/%s", strZ(pgPath), strZ(info->name));
            const bool dbPathSub = regExpMatch(buildData->dbPathExp, manifestName);
            StorageIterator *const storageItr = storageNewItrP(buildData->storagePg, pgPathSub, .sortOrder = sortOrderAsc);

            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                while (storageItrMore(storageItr))
                {
                    const StorageInfo info = storageItrNext(storageItr);

                    manifestBuildInfo(buildData, manifestName, pgPathSub, dbPathSub, &info);

                    // Reset the memory context occasionally so we don't use too much memory or slow down processing
                    MEM_CONTEXT_TEMP_RESET(1000);
                }
            }
            MEM_CONTEXT_TEMP_END();

            break;
        }

        // Add files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeFile:
        {
            // There should not be any files in pg_tblspc
            if (strEqZ(manifestParentName, MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC))
            {
                THROW_FMT(
                    LinkExpectedError, "'%s' is not a symlink - " MANIFEST_TARGET_PGTBLSPC " should contain only symlinks",
                    strZ(manifestName));
            }

            // Skip pg_internal.init since it is recreated on startup.  It's also possible, (though unlikely) that a temp file with
            // the creating process id as the extension can exist so skip that as well.  This seems to be a bug in PostgreSQL since
            // the temp file should be removed on startup.  Use regExpMatchOne() here instead of preparing a regexp in advance since
            // the likelihood of needing the regexp should be very small.
            if (dbPath && strBeginsWithZ(info->name, PG_FILE_PGINTERNALINIT) &&
                (strSize(info->name) == sizeof(PG_FILE_PGINTERNALINIT) - 1 ||
                    regExpMatchOne(STRDEF("\\.[0-9]+"), strSub(info->name, sizeof(PG_FILE_PGINTERNALINIT) - 1))))
            {
                FUNCTION_TEST_RETURN_VOID();
            }

            // Skip files in the root data path
            if (strEq(manifestParentName, MANIFEST_TARGET_PGDATA_STR))
            {
                // Skip recovery files
                if (((strEqZ(info->name, PG_FILE_RECOVERYSIGNAL) || strEqZ(info->name, PG_FILE_STANDBYSIGNAL)) &&
                        pgVersion >= PG_VERSION_12) ||
                    ((strEqZ(info->name, PG_FILE_RECOVERYCONF) || strEqZ(info->name, PG_FILE_RECOVERYDONE)) &&
                        pgVersion < PG_VERSION_12) ||
                    // Skip temp file for safely writing postgresql.auto.conf
                    (strEqZ(info->name, PG_FILE_POSTGRESQLAUTOCONFTMP) && pgVersion >= PG_VERSION_94) ||
                    // Skip backup_label in versions where non-exclusive backup is supported
                    (strEqZ(info->name, PG_FILE_BACKUPLABEL) && pgVersion >= PG_VERSION_96) ||
                    // Skip old backup labels
                    strEqZ(info->name, PG_FILE_BACKUPLABELOLD) ||
                    // Skip backup_manifest/tmp in versions where it is created
                    ((strEqZ(info->name, PG_FILE_BACKUPMANIFEST) || strEqZ(info->name, PG_FILE_BACKUPMANIFEST_TMP)) &&
                        pgVersion >= PG_VERSION_13) ||
                    // Skip running process options
                    strEqZ(info->name, PG_FILE_POSTMTROPTS) ||
                    // Skip process id file to avoid confusing postgres after restore
                    strEqZ(info->name, PG_FILE_POSTMTRPID))
                {
                    FUNCTION_TEST_RETURN_VOID();
                }
            }

            // Skip the contents of the wal path when online. WAL will be restored from the archive or stored in the wal directory
            // at the end of the backup if the archive-copy option is set.
            if (buildData->online && strEq(manifestParentName, buildData->manifestWalName))
                FUNCTION_TEST_RETURN_VOID();

            // Skip temp relations in db paths
            if (dbPath && regExpMatch(buildData->tempRelationExp, info->name))
                FUNCTION_TEST_RETURN_VOID();

            // Add file to manifest
            ManifestFile file =
            {
                .name = manifestName,
                .copy = true,
                .mode = info->mode,
                .user = info->user,
                .group = info->group,
                .size = info->size,
                .sizeRepo = info->size,
                .timestamp = info->timeModified,
            };

            // When bundling zero-length files do not need to be copied
            if (info->size == 0 && buildData->manifest->pub.data.bundle)
            {
                file.copy = false;
                memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
            }

            // Determine if this file should be page checksummed
            if (dbPath && buildData->checksumPage)
            {
                file.checksumPage =
                    !strEndsWithZ(manifestName, "/" PG_FILE_PGFILENODEMAP) && !strEndsWithZ(manifestName, "/" PG_FILE_PGVERSION) &&
                    !strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);
            }

            manifestFileAdd(buildData->manifest, &file);
            break;
        }

        // Add links
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeLink:
        {
            // If the destination is another link then error.  In the future we'll allow this by following the link chain to the
            // eventual destination but for now we are trying to maintain compatibility during the migration.  To do this check we
            // need to read outside of the data directory but it is a read-only operation so is considered safe.
            const String *linkDestinationAbsolute = strPathAbsolute(info->linkDestination, pgPath);

            StorageInfo linkedCheck = storageInfoP(
                buildData->storagePg, linkDestinationAbsolute, .ignoreMissing = true, .noPathEnforce = true);

            if (linkedCheck.exists && linkedCheck.type == storageTypeLink)
            {
                THROW_FMT(
                    LinkDestinationError, "link '%s/%s' cannot reference another link '%s'", strZ(pgPath), strZ(info->name),
                    strZ(linkDestinationAbsolute));
            }

            // Initialize link and target
            ManifestLink link =
            {
                .name = manifestName,
                .user = info->user,
                .group = info->group,
                .destination = info->linkDestination,
            };

            ManifestTarget target =
            {
                .name = manifestName,
                .type = manifestTargetTypeLink,
            };

            // Make a copy of the link name because it will need to be modified when there are tablespace ids
            const String *linkName = info->name;

            // Is this a tablespace?
            if (strEq(manifestParentName, STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC)))
            {
                // Strip pg_data off the manifest name so it begins with pg_tblspc instead.  This reflects how the files are stored
                // in the backup directory.
                manifestName = strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA));

                // Identify this target as a tablespace
                target.name = manifestName;
                target.tablespaceId = cvtZToUInt(strZ(info->name));

                // Look for this tablespace in the provided list (list may be null for off-line backup)
                if (buildData->tablespaceList != NULL)
                {
                    // Search list
                    PackRead *const read = pckReadNew(buildData->tablespaceList);

                    while (!pckReadNullP(read))
                    {
                        pckReadArrayBeginP(read);

                        if (target.tablespaceId == pckReadU32P(read))
                            target.tablespaceName = pckReadStrP(read);

                        pckReadArrayEndP(read);
                    }

                    // Error if the tablespace could not be found.  ??? This seems excessive, perhaps just warn here?
                    if (target.tablespaceName == NULL)
                    {
                        THROW_FMT(
                            AssertError,
                            "tablespace with oid %u not found in tablespace map\n"
                            "HINT: was a tablespace created or dropped during the backup?",
                            target.tablespaceId);
                    }
                }

                // If no tablespace name was found then create one
                if (target.tablespaceName == NULL)
                    target.tablespaceName = strNewFmt("ts%s", strZ(info->name));

                // Add a dummy pg_tblspc path entry if it does not already exist.  This entry will be ignored by restore but it is
                // part of the original manifest format so we need to have it.
                lstSort(buildData->manifest->pub.pathList, sortOrderAsc);
                const ManifestPath *pathBase = manifestPathFind(buildData->manifest, MANIFEST_TARGET_PGDATA_STR);

                if (manifestPathFindDefault(buildData->manifest, MANIFEST_TARGET_PGTBLSPC_STR, NULL) == NULL)
                {
                    ManifestPath path =
                    {
                        .name = MANIFEST_TARGET_PGTBLSPC_STR,
                        .mode = pathBase->mode,
                        .user = pathBase->user,
                        .group = pathBase->group,
                    };

                    manifestPathAdd(buildData->manifest, &path);
                }

                // The tablespace link destination path is not the path where data will be stored so we can just store it as a dummy
                // path. This is because PostgreSQL creates a subpath with the version/catalog number so that multiple versions of
                // PostgreSQL can share a tablespace, which makes upgrades easier.
                const ManifestPath *pathTblSpc = manifestPathFind(
                    buildData->manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC));

                ManifestPath path =
                {
                    .name = manifestName,
                    .mode = pathTblSpc->mode,
                    .user = pathTblSpc->user,
                    .group = pathTblSpc->group,
                };

                manifestPathAdd(buildData->manifest, &path);

                // Update build structure to reflect the path added above and the tablespace id
                manifestParentName = manifestName;
                manifestName = strNewFmt("%s/%s", strZ(manifestName), strZ(buildData->tablespaceId));
                pgPath = strNewFmt("%s/%s", strZ(pgPath), strZ(info->name));
                linkName = buildData->tablespaceId;
            }

            // Add info about the linked file/path
            const String *linkPgPath = strNewFmt("%s/%s", strZ(pgPath), strZ(linkName));
            StorageInfo linkedInfo = storageInfoP(
                buildData->storagePg, linkPgPath, .followLink = true, .ignoreMissing = true);
            linkedInfo.name = linkName;

            // If the link destination exists then build the target
            if (linkedInfo.exists)
            {
                // If a path link then recurse
                if (linkedInfo.type == storageTypePath)
                {
                    target.path = info->linkDestination;
                }
                // Else it must be a file or special (since we have already checked if it is a link)
                else
                {
                    CHECK(FormatError, target.tablespaceId == 0, "tablespace links to a file");

                    // Identify target as a file
                    target.path = strPath(info->linkDestination);
                    target.file = strBase(info->linkDestination);
                }
            }
            // Else dummy up the target with a destination so manifestLinkCheck() can be run.  This is so errors about links with
            // destinations in PGDATA will take precedence over missing a destination.  We will probably simplify this once the
            // migration is done and it doesn't matter which error takes precedence.
            else
                target.path = info->linkDestination;

            // Add target and link
            manifestTargetAdd(buildData->manifest, &target);
            manifestLinkAdd(buildData->manifest, &link);

            // Make sure the link is valid
            manifestLinkCheckOne(buildData->manifest, buildData->linkCheck, manifestTargetTotal(buildData->manifest) - 1);

            // If the link check was successful but the destination does not exist then check it again to generate an error
            if (!linkedInfo.exists)
                storageInfoP(buildData->storagePg, linkPgPath, .followLink = true);

            // Recurse into the link destination
            manifestBuildInfo(buildData, manifestParentName, pgPath, dbPath, &linkedInfo);

            break;
        }

        // Skip special files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeSpecial:
            LOG_WARN_FMT("exclude special file '%s/%s' from backup", strZ(pgPath), strZ(info->name));
            break;
    }

    FUNCTION_TEST_RETURN_VOID();
}

// Regular expression constants
#define RELATION_EXP                                                "[0-9]+(_(fsm|vm)){0,1}(\\.[0-9]+){0,1}"
#define DB_PATH_EXP                                                                                                                \
    "(" MANIFEST_TARGET_PGDATA "/(" PG_PATH_GLOBAL "|" PG_PATH_BASE "/[0-9]+)|" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+)"

Manifest *
manifestNewBuild(
    const Storage *const storagePg, const unsigned int pgVersion, const unsigned int pgCatalogVersion, const bool online,
    const bool checksumPage, const bool bundle, const StringList *const excludeList, const Pack *const tablespaceList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT, pgCatalogVersion);
        FUNCTION_LOG_PARAM(BOOL, online);
        FUNCTION_LOG_PARAM(BOOL, checksumPage);
        FUNCTION_LOG_PARAM(BOOL, bundle);
        FUNCTION_LOG_PARAM(STRING_LIST, excludeList);
        FUNCTION_LOG_PARAM(PACK, tablespaceList);
    FUNCTION_LOG_END();

    ASSERT(storagePg != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(!checksumPage || pgVersion >= PG_VERSION_93);

    Manifest *this = NULL;

    OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = manifestNewInternal();
        this->pub.info = infoNew(NULL);
        this->pub.data.backrestVersion = strNewZ(PROJECT_VERSION);
        this->pub.data.pgVersion = pgVersion;
        this->pub.data.pgCatalogVersion = pgCatalogVersion;
        this->pub.data.backupType = backupTypeFull;
        this->pub.data.backupOptionOnline = online;
        this->pub.data.backupOptionChecksumPage = varNewBool(checksumPage);
        this->pub.data.bundle = bundle;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Data needed to build the manifest
            ManifestLinkCheck linkCheck = manifestLinkCheckInit();

            ManifestBuildData buildData =
            {
                .manifest = this,
                .storagePg = storagePg,
                .tablespaceId = pgTablespaceId(pgVersion, pgCatalogVersion),
                .online = online,
                .checksumPage = checksumPage,
                .tablespaceList = tablespaceList,
                .linkCheck = &linkCheck,
                .manifestWalName = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(pgWalPath(pgVersion))),
            };

            // Build expressions to identify databases paths and temp relations
            // ---------------------------------------------------------------------------------------------------------------------
            ASSERT(buildData.tablespaceId != NULL);

            // Expression to identify database paths
            buildData.dbPathExp = regExpNew(strNewFmt("^" DB_PATH_EXP "$", strZ(buildData.tablespaceId)));

            // Expression to find temp relations
            buildData.tempRelationExp = regExpNew(STRDEF("^t[0-9]+_" RELATION_EXP "$"));

            // Build list of exclusions
            // ---------------------------------------------------------------------------------------------------------------------
            if (excludeList != NULL)
            {
                for (unsigned int excludeIdx = 0; excludeIdx < strLstSize(excludeList); excludeIdx++)
                {
                    const String *exclude = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(strLstGet(excludeList, excludeIdx)));

                    // If the exclusions refers to the contents of a path
                    if (strEndsWithZ(exclude, "/"))
                    {
                        if (buildData.excludeContent == NULL)
                            buildData.excludeContent = strLstNew();

                        strLstAddSub(buildData.excludeContent, exclude, strSize(exclude) - 1);
                    }
                    // Otherwise exclude a single file/link/path
                    else
                    {
                        if (buildData.excludeSingle == NULL)
                            buildData.excludeSingle = strLstNew();

                        strLstAdd(buildData.excludeSingle, exclude);
                    }
                }
            }

            // Build manifest
            // ---------------------------------------------------------------------------------------------------------------------
            const String *const pgPath = storagePathP(storagePg, NULL);
            StorageInfo info = storageInfoP(storagePg, pgPath, .followLink = true);

            ManifestPath path =
            {
                .name = MANIFEST_TARGET_PGDATA_STR,
                .mode = info.mode,
                .user = info.user,
                .group = info.group,
            };

            manifestPathAdd(this, &path);

            // Generate file defaults from base path
            MEM_CONTEXT_BEGIN(this->pub.memContext)
            {
                this->fileUserDefault = strDup(path.user);
                this->fileGroupDefault = strDup(path.group);
                this->fileModeDefault = path.mode & (S_IRUSR | S_IWUSR | S_IRGRP);
            }
            MEM_CONTEXT_END();

            ManifestTarget target =
            {
                .name = MANIFEST_TARGET_PGDATA_STR,
                .path = pgPath,
                .type = manifestTargetTypePath,
            };

            manifestTargetAdd(this, &target);

            // Gather info for the rest of the files/links/paths
            StorageIterator *const storageItr = storageNewItrP(
                storagePg, pgPath, .errorOnMissing = true, .sortOrder = sortOrderAsc);

            MEM_CONTEXT_TEMP_RESET_BEGIN()
            {
                while (storageItrMore(storageItr))
                {
                    const StorageInfo info = storageItrNext(storageItr);

                    manifestBuildInfo(&buildData, MANIFEST_TARGET_PGDATA_STR, pgPath, false, &info);

                    // Reset the memory context occasionally so we don't use too much memory or slow down processing
                    MEM_CONTEXT_TEMP_RESET(1000);
                }
            }
            MEM_CONTEXT_TEMP_END();

            // These may not be in order even if the incoming data was sorted
            lstSort(this->pub.fileList, sortOrderAsc);
            lstSort(this->pub.linkList, sortOrderAsc);
            lstSort(this->pub.pathList, sortOrderAsc);
            lstSort(this->pub.targetList, sortOrderAsc);

            // Remove unlogged relations from the manifest.  This can't be done during the initial build because of the requirement
            // to check for _init files which will sort after the vast majority of the relation files.  We could check storage for
            // each _init file but that would be expensive.
            // -------------------------------------------------------------------------------------------------------------------------
            if (pgVersion >= PG_VERSION_91)
            {
                RegExp *relationExp = regExpNew(strNewFmt("^" DB_PATH_EXP "/" RELATION_EXP "$", strZ(buildData.tablespaceId)));
                unsigned int fileIdx = 0;
                char lastRelationFileId[21] = "";                   // Large enough for a 64-bit unsigned integer
                bool lastRelationFileIdUnlogged = false;

#ifdef DEBUG_MEM
                // Record the temp context size before the loop begins
                size_t sizeBegin = memContextSize(memContextCurrent());
#endif

                while (fileIdx < manifestFileTotal(this))
                {
                    // If this file looks like a relation.  Note that this never matches on _init forks.
                    const String *const filePathName = manifestFileNameGet(this, fileIdx);

                    if (regExpMatch(relationExp, filePathName))
                    {
                        // Get the filename (without path)
                        const char *fileName = strBaseZ(filePathName);
                        size_t fileNameSize = strlen(fileName);

                        // Strip off the numeric part of the relation
                        char relationFileId[sizeof(lastRelationFileId)];
                        unsigned int nameIdx = 0;

                        for (; nameIdx < fileNameSize; nameIdx++)
                        {
                            if (!isdigit(fileName[nameIdx]))
                                break;

                            relationFileId[nameIdx] = fileName[nameIdx];
                        }

                        relationFileId[nameIdx] = '\0';

                        // The filename must have characters
                        ASSERT(relationFileId[0] != '\0');

                        // Store the last relation so it does not need to be found everytime
                        if (strcmp(lastRelationFileId, relationFileId) != 0)
                        {
                            // Determine if the relation is unlogged
                            String *relationInit = strNewFmt(
                                "%.*s%s_init", (int)(strSize(filePathName) - fileNameSize), strZ(filePathName), relationFileId);
                            lastRelationFileIdUnlogged = manifestFileExists(this, relationInit);
                            strFree(relationInit);

                            // Save the file id so we don't need to do the lookup next time if it doesn't change
                            strcpy(lastRelationFileId, relationFileId);
                        }

                        // If relation is unlogged then remove it
                        if (lastRelationFileIdUnlogged)
                        {
                            manifestFileRemove(this, filePathName);
                            continue;
                        }
                    }

                    fileIdx++;
                }

#ifdef DEBUG_MEM
                // Make sure that the temp context did not grow too much during the loop
                ASSERT(memContextSize(memContextCurrent()) - sizeBegin < 256);
#endif
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(MANIFEST, this);
}

/**********************************************************************************************************************************/
void
manifestBuildValidate(Manifest *this, bool delta, time_t copyStart, CompressType compressType)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(BOOL, delta);
        FUNCTION_LOG_PARAM(TIME, copyStart);
        FUNCTION_LOG_PARAM(ENUM, compressType);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(copyStart > 0);

    MEM_CONTEXT_BEGIN(this->pub.memContext)
    {
        // Store the delta option.  If true we can skip checks that automatically enable delta.
        this->pub.data.backupOptionDelta = varNewBool(delta);

        // If online then add one second to the copy start time to allow for database updates during the last second that the
        // manifest was being built.  It's up to the caller to actually wait the remainder of the second, but for comparison
        // purposes we want the time when the waiting started.
        this->pub.data.backupTimestampCopyStart = copyStart + (this->pub.data.backupOptionOnline ? 1 : 0);

        // This value is not needed in this function, but it is needed for resumed manifests and this is last place to set it before
        // processing begins
        this->pub.data.backupOptionCompressType = compressType;
    }
    MEM_CONTEXT_END();

    // Check the manifest for timestamp anomalies that require a delta backup (if delta is not already specified)
    if (!varBool(this->pub.data.backupOptionDelta))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
            {
                const ManifestFile file = manifestFile(this, fileIdx);

                // Check for timestamp in the future
                if (file.timestamp > copyStart)
                {
                    LOG_WARN_FMT(
                        "file '%s' has timestamp (%" PRId64 ") in the future (relative to copy start %" PRId64 "), enabling delta"
                            " checksum", strZ(manifestPathPg(file.name)), (int64_t)file.timestamp, (int64_t)copyStart);

                    this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
                    break;
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
manifestBuildIncr(Manifest *this, const Manifest *manifestPrior, BackupType type, const String *archiveStart)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(MANIFEST, manifestPrior);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, archiveStart);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(manifestPrior != NULL);
    ASSERT(type == backupTypeDiff || type == backupTypeIncr);
    ASSERT(type != backupTypeDiff || manifestPrior->pub.data.backupType == backupTypeFull);
    ASSERT(archiveStart == NULL || strSize(archiveStart) == 24);

    MEM_CONTEXT_BEGIN(this->pub.memContext)
    {
        // Set prior backup label
        this->pub.data.backupLabelPrior = strDup(manifestPrior->pub.data.backupLabel);

        // Copy reference list
        this->pub.referenceList = strLstDup(manifestPrior->pub.referenceList);

        // Set diff/incr backup type
        this->pub.data.backupType = type;
    }
    MEM_CONTEXT_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Enable delta if timelines differ
        if (archiveStart != NULL && manifestData(manifestPrior)->archiveStop != NULL &&
            !strEq(strSubN(archiveStart, 0, 8), strSubN(manifestData(manifestPrior)->archiveStop, 0, 8)))
        {
            LOG_WARN_FMT(
                "a timeline switch has occurred since the %s backup, enabling delta checksum\n"
                "HINT: this is normal after restoring from backup or promoting a standby.",
                strZ(manifestData(manifestPrior)->backupLabel));

            this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
        }
        // Else enable delta if online differs
        else if (manifestData(manifestPrior)->backupOptionOnline != this->pub.data.backupOptionOnline)
        {
            LOG_WARN_FMT(
                "the online option has changed since the %s backup, enabling delta checksum",
                strZ(manifestData(manifestPrior)->backupLabel));

            this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
        }

        // Check for anomalies between manifests if delta is not already enabled.  This can't be combined with the main comparison
        // loop below because delta changes the behavior of that loop.
        if (!varBool(this->pub.data.backupOptionDelta))
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
            {
                const ManifestFile file = manifestFile(this, fileIdx);

                // If file was found in prior manifest then perform checks
                if (manifestFileExists(manifestPrior, file.name))
                {
                    const ManifestFile filePrior = manifestFileFind(manifestPrior, file.name);

                    // Check for timestamp earlier than the prior backup
                    if (file.timestamp < filePrior.timestamp)
                    {
                        LOG_WARN_FMT(
                            "file '%s' has timestamp earlier than prior backup (prior %" PRId64 ", current %" PRId64 "), enabling"
                                " delta checksum",
                            strZ(manifestPathPg(file.name)), (int64_t)filePrior.timestamp, (int64_t)file.timestamp);

                        this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
                        break;
                    }

                    // Check for size change with no timestamp change
                    if (file.size != filePrior.size && file.timestamp == filePrior.timestamp)
                    {
                        LOG_WARN_FMT(
                            "file '%s' has same timestamp (%" PRId64 ") as prior but different size (prior %" PRIu64 ", current"
                                " %" PRIu64 "), enabling delta checksum",
                            strZ(manifestPathPg(file.name)), (int64_t)file.timestamp, filePrior.size, file.size);

                        this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
                        break;
                    }
                }
            }
        }

        // Find files to reference in the prior manifest:
        // 1) that don't need to be copied because delta is disabled and the size and timestamp match or size matches and is zero
        // 2) where delta is enabled and size matches so checksum will be verified during backup and the file copied on mismatch
        bool delta = varBool(this->pub.data.backupOptionDelta);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->pub.fileList); fileIdx++)
        {
            ManifestFile file = manifestFile(this, fileIdx);

            // Check if prior file can be used
            if (manifestFileExists(manifestPrior, file.name))
            {
                const ManifestFile filePrior = manifestFileFind(manifestPrior, file.name);

                if (file.copy && file.size == filePrior.size && (delta || file.size == 0 || file.timestamp == filePrior.timestamp))
                {
                    file.sizeRepo = filePrior.sizeRepo;
                    memcpy(file.checksumSha1, filePrior.checksumSha1, HASH_TYPE_SHA1_SIZE_HEX + 1);
                    file.reference = filePrior.reference != NULL ? filePrior.reference : manifestPrior->pub.data.backupLabel;
                    file.checksumPage = filePrior.checksumPage;
                    file.checksumPageError = filePrior.checksumPageError;
                    file.checksumPageErrorList = filePrior.checksumPageErrorList;
                    file.bundleId = filePrior.bundleId;
                    file.bundleOffset = filePrior.bundleOffset;

                    // Perform delta if the file size is not zero
                    file.delta = delta && file.size != 0;

                    // Copy if the file has changed or delta
                    file.copy = (file.size != 0 && file.timestamp != filePrior.timestamp) || file.delta;

                    manifestFileUpdate(this, &file);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
manifestBuildComplete(
    Manifest *const this, const time_t timestampStart, const String *const lsnStart, const String *const archiveStart,
    const time_t timestampStop, const String *const lsnStop, const String *const archiveStop, const unsigned int pgId,
    const uint64_t pgSystemId, const Pack *const dbList, const bool optionArchiveCheck, const bool optionArchiveCopy,
    const size_t optionBufferSize, const unsigned int optionCompressLevel, const unsigned int optionCompressLevelNetwork,
    const bool optionHardLink, const unsigned int optionProcessMax, const bool optionStandby, const KeyValue *const annotation)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(TIME, timestampStart);
        FUNCTION_LOG_PARAM(STRING, lsnStart);
        FUNCTION_LOG_PARAM(STRING, archiveStart);
        FUNCTION_LOG_PARAM(TIME, timestampStop);
        FUNCTION_LOG_PARAM(STRING, lsnStop);
        FUNCTION_LOG_PARAM(STRING, archiveStop);
        FUNCTION_LOG_PARAM(UINT, pgId);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_LOG_PARAM(PACK, dbList);
        FUNCTION_LOG_PARAM(BOOL, optionArchiveCheck);
        FUNCTION_LOG_PARAM(BOOL, optionArchiveCopy);
        FUNCTION_LOG_PARAM(SIZE, optionBufferSize);
        FUNCTION_LOG_PARAM(UINT, optionCompressLevel);
        FUNCTION_LOG_PARAM(UINT, optionCompressLevelNetwork);
        FUNCTION_LOG_PARAM(BOOL, optionHardLink);
        FUNCTION_LOG_PARAM(UINT, optionProcessMax);
        FUNCTION_LOG_PARAM(BOOL, optionStandby);
        FUNCTION_LOG_PARAM(KEY_VALUE, annotation);
    FUNCTION_LOG_END();

    MEM_CONTEXT_BEGIN(this->pub.memContext)
    {
        // Save info
        this->pub.data.backupTimestampStart = timestampStart;
        this->pub.data.lsnStart = strDup(lsnStart);
        this->pub.data.archiveStart = strDup(archiveStart);
        this->pub.data.backupTimestampStop = timestampStop;
        this->pub.data.lsnStop = strDup(lsnStop);
        this->pub.data.archiveStop = strDup(archiveStop);
        this->pub.data.pgId = pgId;
        this->pub.data.pgSystemId = pgSystemId;

        // Save db list
        if (dbList != NULL)
        {
            PackRead *const read = pckReadNew(dbList);

            while (!pckReadNullP(read))
            {
                pckReadArrayBeginP(read);

                const unsigned int id = pckReadU32P(read);
                const String *const name = pckReadStrP(read);
                const unsigned int lastSystemId = pckReadU32P(read);

                pckReadArrayEndP(read);

                manifestDbAdd(this, &(ManifestDb){.id = id, .name = name, .lastSystemId = lastSystemId});

            }

            lstSort(this->pub.dbList, sortOrderAsc);
        }

        // Save annotations
        if (annotation != NULL)
        {
            this->pub.data.annotation = varNewKv(kvNew());

            KeyValue *const manifestAnnotationKv = varKv(this->pub.data.annotation);
            const VariantList *const annotationKeyList = kvKeyList(annotation);

            for (unsigned int keyIdx = 0; keyIdx < varLstSize(annotationKeyList); keyIdx++)
            {
                const Variant *const key = varLstGet(annotationKeyList, keyIdx);
                const Variant *const value = kvGet(annotation, key);

                // Skip empty values
                if (!strEmpty(varStr(value)))
                    kvPut(manifestAnnotationKv, key, value);
            }

            // Clean field if there are no annotations to save
            if (varLstSize(kvKeyList(manifestAnnotationKv)) == 0)
                this->pub.data.annotation = NULL;
        }

        // Save options
        this->pub.data.backupOptionArchiveCheck = optionArchiveCheck;
        this->pub.data.backupOptionArchiveCopy = optionArchiveCopy;
        this->pub.data.backupOptionBufferSize = varNewUInt64(optionBufferSize);
        this->pub.data.backupOptionCompressLevel = varNewUInt(optionCompressLevel);
        this->pub.data.backupOptionCompressLevelNetwork = varNewUInt(optionCompressLevelNetwork);
        this->pub.data.backupOptionHardLink = optionHardLink;
        this->pub.data.backupOptionProcessMax = varNewUInt(optionProcessMax);
        this->pub.data.backupOptionStandby = varNewBool(optionStandby);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
#define MANIFEST_TARGET_TYPE_LINK                                   "link"
#define MANIFEST_TARGET_TYPE_PATH                                   "path"

#define MANIFEST_SECTION_BACKUP                                     "backup"
#define MANIFEST_SECTION_BACKUP_DB                                  "backup:db"
#define MANIFEST_SECTION_BACKUP_OPTION                              "backup:option"
#define MANIFEST_SECTION_BACKUP_TARGET                              "backup:target"

#define MANIFEST_SECTION_DB                                         "db"
#define MANIFEST_SECTION_METADATA                                   "metadata"

#define MANIFEST_SECTION_TARGET_FILE                                "target:file"
#define MANIFEST_SECTION_TARGET_FILE_DEFAULT                        "target:file:default"
#define MANIFEST_SECTION_TARGET_LINK                                "target:link"
#define MANIFEST_SECTION_TARGET_LINK_DEFAULT                        "target:link:default"
#define MANIFEST_SECTION_TARGET_PATH                                "target:path"
#define MANIFEST_SECTION_TARGET_PATH_DEFAULT                        "target:path:default"

#define MANIFEST_KEY_ANNOTATION                                     "annotation"
#define MANIFEST_KEY_BACKUP_ARCHIVE_START                           "backup-archive-start"
#define MANIFEST_KEY_BACKUP_ARCHIVE_STOP                            "backup-archive-stop"
#define MANIFEST_KEY_BACKUP_BUNDLE                                  "backup-bundle"
#define MANIFEST_KEY_BACKUP_LABEL                                   "backup-label"
#define MANIFEST_KEY_BACKUP_LSN_START                               "backup-lsn-start"
#define MANIFEST_KEY_BACKUP_LSN_STOP                                "backup-lsn-stop"
#define MANIFEST_KEY_BACKUP_PRIOR                                   "backup-prior"
#define MANIFEST_KEY_BACKUP_REFERENCE                               "backup-reference"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START                    "backup-timestamp-copy-start"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_START                         "backup-timestamp-start"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                          "backup-timestamp-stop"
#define MANIFEST_KEY_BACKUP_TYPE                                    "backup-type"
#define MANIFEST_KEY_BUNDLE_ID                                      STRID5("bni", 0x25c20)
#define MANIFEST_KEY_BUNDLE_OFFSET                                  STRID5("bno", 0x3dc20)
#define MANIFEST_KEY_CHECKSUM                                       STRID5("checksum", 0x6d66b195030)
#define MANIFEST_KEY_CHECKSUM_PAGE                                  "checksum-page"
#define MANIFEST_KEY_CHECKSUM_PAGE_ERROR                            "checksum-page-error"
#define MANIFEST_KEY_DB_CATALOG_VERSION                             "db-catalog-version"
#define MANIFEST_KEY_DB_ID                                          "db-id"
#define MANIFEST_KEY_DB_LAST_SYSTEM_ID                              "db-last-system-id"
#define MANIFEST_KEY_DB_SYSTEM_ID                                   "db-system-id"
#define MANIFEST_KEY_DB_VERSION                                     "db-version"
#define MANIFEST_KEY_DESTINATION                                    STRID5("destination", 0x39e9a05c9a4ca40)
#define MANIFEST_KEY_FILE                                           STRID5("file", 0x2b1260)
#define MANIFEST_KEY_GROUP                                          "group"
#define MANIFEST_KEY_MODE                                           "mode"
#define MANIFEST_KEY_PATH                                           STRID5("path", 0x450300)
#define MANIFEST_KEY_REFERENCE                                      STRID5("reference", 0x51b8b2298b20)
#define MANIFEST_KEY_SIZE                                           STRID5("size", 0x2e9330)
#define MANIFEST_KEY_SIZE_REPO                                      STRID5("repo-size", 0x5d267b7c0b20)
#define MANIFEST_KEY_TABLESPACE_ID                                  "tablespace-id"
#define MANIFEST_KEY_TABLESPACE_NAME                                "tablespace-name"
#define MANIFEST_KEY_TIMESTAMP                                      STRID5("timestamp", 0x10686932b5340)
#define MANIFEST_KEY_TYPE                                           STRID5("type", 0x2c3340)
#define MANIFEST_KEY_USER                                           "user"

#define MANIFEST_KEY_OPTION_ARCHIVE_CHECK                           "option-archive-check"
#define MANIFEST_KEY_OPTION_ARCHIVE_COPY                            "option-archive-copy"
#define MANIFEST_KEY_OPTION_BACKUP_STANDBY                          "option-backup-standby"
#define MANIFEST_KEY_OPTION_BUFFER_SIZE                             "option-buffer-size"
#define MANIFEST_KEY_OPTION_CHECKSUM_PAGE                           "option-checksum-page"
#define MANIFEST_KEY_OPTION_COMPRESS                                "option-compress"
#define MANIFEST_KEY_OPTION_COMPRESS_TYPE                           "option-compress-type"
#define MANIFEST_KEY_OPTION_COMPRESS_LEVEL                          "option-compress-level"
#define MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK                  "option-compress-level-network"
#define MANIFEST_KEY_OPTION_DELTA                                   "option-delta"
#define MANIFEST_KEY_OPTION_HARDLINK                                "option-hardlink"
#define MANIFEST_KEY_OPTION_ONLINE                                  "option-online"
#define MANIFEST_KEY_OPTION_PROCESS_MAX                             "option-process-max"

// Keep track of which values were found during load and which need to be loaded from defaults. There is no point in having
// multiple structs since most of the fields are the same and the size shouldn't be more than 4/8 bytes.
typedef struct ManifestLoadFound
{
    bool group:1;
    bool mode:1;
    bool user:1;
} ManifestLoadFound;

typedef struct ManifestLoadData
{
    MemContext *memContext;                                         // Mem context for data needed only during load
    Manifest *manifest;                                             // Manifest info
    bool referenceListFound;                                        // Was a reference list found?

    List *linkFoundList;                                            // Values found in links
    const Variant *linkGroupDefault;                                // Link default group
    const Variant *linkUserDefault;                                 // Link default user

    List *pathFoundList;                                            // Values found in paths
    const Variant *pathGroupDefault;                                // Path default group
    mode_t pathModeDefault;                                         // Path default mode
    const Variant *pathUserDefault;                                 // Path default user
} ManifestLoadData;

// Helper to transform a variant that could be boolean or string into a string.  If the boolean is false return NULL else return
// the string.  The boolean cannot be true.
static const String *
manifestOwnerGet(const Variant *owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, owner);
    FUNCTION_TEST_END();

    ASSERT(owner != NULL);

    // If bool then it should be false.  This indicates that the owner could not be mapped to a name during the backup.
    if (varType(owner) == varTypeBool)
    {
        CHECK(FormatError, !varBool(owner), "owner bool must be false");
        FUNCTION_TEST_RETURN_CONST(STRING, NULL);
    }

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(owner));
}

// Helper to check the variant type of owner and duplicate (call in the containing context)
static const Variant *
manifestOwnerDefaultGet(const Variant *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, ownerDefault);
    FUNCTION_TEST_END();

    ASSERT(ownerDefault != NULL);

    // Bool = false means the owner was not mapped to a name
    if (varType(ownerDefault) == varTypeBool)
    {
        // Value must be false
        CHECK(FormatError, !varBool(ownerDefault), "owner bool must be false");
        FUNCTION_TEST_RETURN_CONST(VARIANT, BOOL_FALSE_VAR);
    }

    // Return a duplicate of the owner passed in
    FUNCTION_TEST_RETURN_CONST(VARIANT, varDup(ownerDefault));
}

static void
manifestLoadCallback(void *callbackData, const String *const section, const String *const key, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);

    ManifestLoadData *const loadData = (ManifestLoadData *)callbackData;
    Manifest *const manifest = loadData->manifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (strEqZ(section, MANIFEST_SECTION_TARGET_FILE))
    {
        ManifestFile file = {.name = key};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        // Bundle info
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_BUNDLE_ID))
        {
            file.bundleId = jsonReadUInt64(json);

            if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_BUNDLE_OFFSET))
                file.bundleOffset = jsonReadUInt64(json);
        }

        // The checksum might not exist if this is a partial save that was done during the backup to preserve checksums for already
        // backed up files
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_CHECKSUM))
            memcpy(file.checksumSha1, strZ(jsonReadStr(json)), HASH_TYPE_SHA1_SIZE_HEX + 1);

        // Page checksum errors
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_CHECKSUM_PAGE))
        {
            file.checksumPage = true;
            file.checksumPageError = !jsonReadBool(json);

            if (jsonReadKeyExpectZ(json, MANIFEST_KEY_CHECKSUM_PAGE_ERROR))
                file.checksumPageErrorList = jsonFromVar(jsonReadVar(json));
        }

        // Group
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_GROUP))
            file.group = manifestOwnerGet(jsonReadVar(json));
        else
            file.group = manifest->fileGroupDefault;

        // Mode
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_MODE))
            file.mode = cvtZToMode(strZ(jsonReadStr(json)));
        else
            file.mode = manifest->fileModeDefault;

        // Reference
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_REFERENCE))
        {
            file.reference = jsonReadStr(json);

            if (!loadData->referenceListFound)
                file.reference = strLstAddIfMissing(manifest->pub.referenceList, file.reference);
        }

        // If "repo-size" is not present in the manifest file, then it is the same as size (i.e. uncompressed) - to save space,
        // the repo-size is only stored in the manifest file if it is different than size.
        const bool sizeRepoExists = jsonReadKeyExpectStrId(json, MANIFEST_KEY_SIZE_REPO);

        if (sizeRepoExists)
            file.sizeRepo = jsonReadUInt64(json);

        // Size is required so error if it is not present. Older versions removed the size before the backup to ensure that the
        // manifest was updated during the backup, so size can be missing in partial manifests. This error will prevent older
        // partials from being resumed.
        if (!jsonReadKeyExpectStrId(json, MANIFEST_KEY_SIZE))
            THROW_FMT(FormatError, "missing size for file '%s'", strZ(key));

        file.size = jsonReadUInt64(json);

        // If repo size did not exist then
        if (!sizeRepoExists)
            file.sizeRepo = file.size;

        // If file size is zero then assign the static zero hash
        if (file.size == 0)
            memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);

        // Timestamp is required so error if it is not present
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_TIMESTAMP))
            file.timestamp = (time_t)jsonReadInt64(json);
        else
            THROW_FMT(FormatError, "missing timestamp for file '%s'", strZ(key));

        // User
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_USER))
            file.user = manifestOwnerGet(jsonReadVar(json));
        else
            file.user = manifest->fileUserDefault;

        manifestFileAdd(manifest, &file);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_PATH))
    {
        ManifestPath path = {.name = key};
        ManifestLoadFound valueFound = {0};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_GROUP))
        {
            valueFound.group = true;
            path.group = manifestOwnerGet(jsonReadVar(json));
        }

        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_MODE))
        {
            valueFound.mode = true;
            path.mode = cvtZToMode(strZ(jsonReadStr(json)));
        }

        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_USER))
        {
            valueFound.user = true;
            path.user = manifestOwnerGet(jsonReadVar(json));
        }

        lstAdd(loadData->pathFoundList, &valueFound);
        manifestPathAdd(manifest, &path);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_LINK))
    {
        ManifestLink link = {.name = key};
        ManifestLoadFound valueFound = {0};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        // Link destination
        link.destination = jsonReadStr(jsonReadKeyRequireStrId(json, MANIFEST_KEY_DESTINATION));

        // Group
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_GROUP))
        {
            valueFound.group = true;
            link.group = manifestOwnerGet(jsonReadVar(json));
        }

        // User
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_USER))
        {
            valueFound.user = true;
            link.user = manifestOwnerGet(jsonReadVar(json));
        }

        lstAdd(loadData->linkFoundList, &valueFound);
        manifestLinkAdd(manifest, &link);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_FILE_DEFAULT))
    {
        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_GROUP))
                manifest->fileGroupDefault = manifestOwnerGet(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_MODE))
                manifest->fileModeDefault = cvtZToMode(strZ(varStr(jsonToVar(value))));
            else if (strEqZ(key, MANIFEST_KEY_USER))
                manifest->fileUserDefault = strDup(manifestOwnerGet(jsonToVar(value)));
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_PATH_DEFAULT))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_GROUP))
                loadData->pathGroupDefault = manifestOwnerDefaultGet(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_MODE))
                loadData->pathModeDefault = cvtZToMode(strZ(varStr(jsonToVar(value))));
            else if (strEqZ(key, MANIFEST_KEY_USER))
                loadData->pathUserDefault = manifestOwnerDefaultGet(jsonToVar(value));
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_GROUP))
                loadData->linkGroupDefault = manifestOwnerDefaultGet(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_USER))
                loadData->linkUserDefault = manifestOwnerDefaultGet(jsonToVar(value));
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_TARGET))
    {
        ManifestTarget target = {.name = key};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        // File
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_FILE))
            target.file = jsonReadStr(json);

        // Path
        target.path = jsonReadStr(jsonReadKeyRequireStrId(json, MANIFEST_KEY_PATH));

        // Tablespace oid
        if (jsonReadKeyExpectZ(json, MANIFEST_KEY_TABLESPACE_ID))
        {
            target.tablespaceId = cvtZToUInt(strZ(jsonReadStr(json)));
            target.tablespaceName = jsonReadStr(jsonReadKeyRequireZ(json, MANIFEST_KEY_TABLESPACE_NAME));
        }

        // Tablespace type
        const String *const targetType = jsonReadStr(jsonReadKeyRequireStrId(json, MANIFEST_KEY_TYPE));
        ASSERT(strEqZ(targetType, MANIFEST_TARGET_TYPE_LINK) || strEqZ(targetType, MANIFEST_TARGET_TYPE_PATH));

        target.type = strEqZ(targetType, MANIFEST_TARGET_TYPE_PATH) ? manifestTargetTypePath : manifestTargetTypeLink;

        manifestTargetAdd(manifest, &target);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_DB))
    {
        ManifestDb db = {.name = key};

        JsonRead *const json = jsonReadNew(value);
        jsonReadObjectBegin(json);

        // Database oid
        db.id = jsonReadUInt(jsonReadKeyRequireZ(json, MANIFEST_KEY_DB_ID));

        // Last system oid
        db.lastSystemId = jsonReadUInt(jsonReadKeyRequireZ(json, MANIFEST_KEY_DB_LAST_SYSTEM_ID));

        manifestDbAdd(manifest, &db);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_METADATA))
    {
        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_ANNOTATION))
                manifest->pub.data.annotation = jsonToVar(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP))
    {
        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_BACKUP_ARCHIVE_START))
                manifest->pub.data.archiveStart = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_ARCHIVE_STOP))
                manifest->pub.data.archiveStop = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_BUNDLE))
                manifest->pub.data.bundle = varBool(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_LABEL))
                manifest->pub.data.backupLabel = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_LSN_START))
                manifest->pub.data.lsnStart = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_LSN_STOP))
                manifest->pub.data.lsnStop = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_PRIOR))
                manifest->pub.data.backupLabelPrior = varStr(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_REFERENCE))
            {
                manifest->pub.referenceList = strLstNewSplitZ(varStr(jsonToVar(value)), ",");
                loadData->referenceListFound = true;
            }
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START))
                manifest->pub.data.backupTimestampCopyStart = (time_t)varUInt64(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_START))
                manifest->pub.data.backupTimestampStart = (time_t)varUInt64(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_STOP))
                manifest->pub.data.backupTimestampStop = (time_t)varUInt64(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_BACKUP_TYPE))
            {
                manifest->pub.data.backupType = (BackupType)strIdFromStr(varStr(jsonToVar(value)));
                ASSERT(
                    manifest->pub.data.backupType == backupTypeFull || manifest->pub.data.backupType == backupTypeDiff ||
                    manifest->pub.data.backupType == backupTypeIncr);
            }
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_DB))
    {
        if (strEqZ(key, MANIFEST_KEY_DB_ID))
            manifest->pub.data.pgId = varUIntForce(jsonToVar(value));
        else if (strEqZ(key, MANIFEST_KEY_DB_SYSTEM_ID))
            manifest->pub.data.pgSystemId = varUInt64(jsonToVar(value));
        else if (strEqZ(key, MANIFEST_KEY_DB_CATALOG_VERSION))
            manifest->pub.data.pgCatalogVersion = varUIntForce(jsonToVar(value));
        else if (strEqZ(key, MANIFEST_KEY_DB_VERSION))
            manifest->pub.data.pgVersion = pgVersionFromStr(varStr(jsonToVar(value)));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_OPTION))
    {
        MEM_CONTEXT_BEGIN(manifest->pub.memContext)
        {
            // Required options
            if (strEqZ(key, MANIFEST_KEY_OPTION_ARCHIVE_CHECK))
                manifest->pub.data.backupOptionArchiveCheck = varBool(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_ARCHIVE_COPY))
                manifest->pub.data.backupOptionArchiveCopy = varBool(jsonToVar(value));
            // Historically this option meant to add gz compression
            else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS))
                manifest->pub.data.backupOptionCompressType = varBool(jsonToVar(value)) ? compressTypeGz : compressTypeNone;
            // This new option allows any type of compression to be specified.  It must be parsed after the option above so the
            // value does not get overwritten.  Since options are stored in alpha order this should always be true.
            else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_TYPE))
                manifest->pub.data.backupOptionCompressType = compressTypeEnum(strIdFromStr(varStr(jsonToVar(value))));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_HARDLINK))
                manifest->pub.data.backupOptionHardLink = varBool(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_ONLINE))
                manifest->pub.data.backupOptionOnline = varBool(jsonToVar(value));

            // Options that were added after v1.00 and may not be present in every manifest
            else if (strEqZ(key, MANIFEST_KEY_OPTION_BACKUP_STANDBY))
                manifest->pub.data.backupOptionStandby = varNewBool(varBool(jsonToVar(value)));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_BUFFER_SIZE))
                manifest->pub.data.backupOptionBufferSize = varNewUInt(varUIntForce(jsonToVar(value)));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_CHECKSUM_PAGE))
                manifest->pub.data.backupOptionChecksumPage = varDup(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL))
                manifest->pub.data.backupOptionCompressLevel = varNewUInt(varUIntForce(jsonToVar(value)));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK))
                manifest->pub.data.backupOptionCompressLevelNetwork = varNewUInt(varUIntForce(jsonToVar(value)));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_DELTA))
                manifest->pub.data.backupOptionDelta = varDup(jsonToVar(value));
            else if (strEqZ(key, MANIFEST_KEY_OPTION_PROCESS_MAX))
                manifest->pub.data.backupOptionProcessMax = varNewUInt(varUIntForce(jsonToVar(value)));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

Manifest *
manifestNewLoad(IoRead *read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    Manifest *this = NULL;

    OBJ_NEW_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = manifestNewInternal();

        // Load the manifest
        ManifestLoadData loadData =
        {
            .memContext = memContextNewP("load", .childQty = MEM_CONTEXT_QTY_MAX),
            .manifest = this,
        };

        // Set file defaults that will be updated when we know what the real defaults are. These need to be set to values that are
        // not valid for actual names or modes.
        this->fileUserDefault = STRDEF("@");
        this->fileGroupDefault = this->fileUserDefault;
        this->fileModeDefault = (mode_t)-1;

        MEM_CONTEXT_BEGIN(loadData.memContext)
        {
            loadData.linkFoundList = lstNewP(sizeof(ManifestLoadFound));
            loadData.pathFoundList = lstNewP(sizeof(ManifestLoadFound));
        }
        MEM_CONTEXT_END();

        this->pub.info = infoNewLoad(read, manifestLoadCallback, &loadData);
        this->pub.data.backrestVersion = infoBackrestVersion(this->pub.info);

        // Process link defaults
        for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(this); linkIdx++)
        {
            ManifestLink *link = lstGet(this->pub.linkList, linkIdx);
            ManifestLoadFound *found = lstGet(loadData.linkFoundList, linkIdx);

            if (!found->group)
                link->group = manifestOwnerCache(this, manifestOwnerGet(loadData.linkGroupDefault));

            if (!found->user)
                link->user = manifestOwnerCache(this, manifestOwnerGet(loadData.linkUserDefault));
        }

        // Process path defaults
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(this); pathIdx++)
        {
            ManifestPath *path = lstGet(this->pub.pathList, pathIdx);
            ManifestLoadFound *found = lstGet(loadData.pathFoundList, pathIdx);

            if (!found->group)
                path->group = manifestOwnerCache(this, manifestOwnerGet(loadData.pathGroupDefault));

            if (!found->mode)
                path->mode = loadData.pathModeDefault;

            if (!found->user)
                path->user = manifestOwnerCache(this, manifestOwnerGet(loadData.pathUserDefault));
        }

        // Sort the lists.  They should already be sorted in the file but it is possible that this system has a different collation
        // that renders that sort useless.
        //
        // This must happen *after* the default processing because found lists are in natural file order and it is not worth writing
        // comparator routines for them.
        lstSort(this->pub.dbList, sortOrderAsc);
        lstSort(this->pub.fileList, sortOrderAsc);
        lstSort(this->pub.linkList, sortOrderAsc);
        lstSort(this->pub.pathList, sortOrderAsc);
        lstSort(this->pub.targetList, sortOrderAsc);

        // Make sure the base path exists
        manifestTargetBase(this);

        // Discard the context holding temporary load data
        memContextDiscard();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(MANIFEST, this);
}

/**********************************************************************************************************************************/
typedef struct ManifestSaveData
{
    Manifest *manifest;                                             // Manifest object to be saved

    const Variant *userDefault;                                     // Default user
    const Variant *groupDefault;                                    // Default group
    mode_t fileModeDefault;                                         // File default mode
    mode_t pathModeDefault;                                         // Path default mode
} ManifestSaveData;

// Helper to convert the owner MCV to a default.  If the input is NULL boolean false should be returned, else the owner string.
static const Variant *
manifestOwnerVar(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(VARIANT, ownerDefault == NULL ? BOOL_FALSE_VAR : varNewStr(ownerDefault));
}

static void
manifestSaveCallback(void *const callbackData, const String *const sectionNext, InfoSave *const infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(infoSaveData != NULL);

    ManifestSaveData *const saveData = (ManifestSaveData *)callbackData;
    Manifest *const manifest = saveData->manifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP, sectionNext))
    {
        if (manifest->pub.data.archiveStart != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_ARCHIVE_START,
                jsonFromVar(VARSTR(manifest->pub.data.archiveStart)));
        }

        if (manifest->pub.data.archiveStop != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_ARCHIVE_STOP,
                jsonFromVar(VARSTR(manifest->pub.data.archiveStop)));
        }

        if (manifest->pub.data.bundle)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_BUNDLE, jsonFromVar(VARBOOL(manifest->pub.data.bundle)));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_LABEL, jsonFromVar(VARSTR(manifest->pub.data.backupLabel)));

        if (manifest->pub.data.lsnStart != NULL)
        {
            infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_LSN_START, jsonFromVar(VARSTR(manifest->pub.data.lsnStart)));
        }

        if (manifest->pub.data.lsnStop != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_LSN_STOP,
                jsonFromVar(VARSTR(manifest->pub.data.lsnStop)));
        }

        if (manifest->pub.data.backupLabelPrior != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_PRIOR,
                jsonFromVar(VARSTR(manifest->pub.data.backupLabelPrior)));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_REFERENCE,
            jsonFromVar(VARSTR(strLstJoin(manifest->pub.referenceList, ","))));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START,
            jsonFromVar(VARINT64(manifest->pub.data.backupTimestampCopyStart)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_TIMESTAMP_START,
            jsonFromVar(VARINT64(manifest->pub.data.backupTimestampStart)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_TIMESTAMP_STOP,
            jsonFromVar(VARINT64(manifest->pub.data.backupTimestampStop)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_TYPE,
            jsonFromVar(VARSTR(strIdToStr(manifest->pub.data.backupType))));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_DB, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_CATALOG_VERSION,
            jsonFromVar(VARUINT(manifest->pub.data.pgCatalogVersion)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB, "db-control-version",
            jsonFromVar(VARUINT(pgControlVersion(manifest->pub.data.pgVersion))));
        infoSaveValue(infoSaveData, MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_ID, jsonFromVar(VARUINT(manifest->pub.data.pgId)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_SYSTEM_ID,
            jsonFromVar(VARUINT64(manifest->pub.data.pgSystemId)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB, MANIFEST_KEY_DB_VERSION,
            jsonFromVar(VARSTR(pgVersionToStr(manifest->pub.data.pgVersion))));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_ARCHIVE_CHECK,
            jsonFromVar(VARBOOL(manifest->pub.data.backupOptionArchiveCheck)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_ARCHIVE_COPY,
            jsonFromVar(VARBOOL(manifest->pub.data.backupOptionArchiveCopy)));

        if (manifest->pub.data.backupOptionStandby != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_BACKUP_STANDBY,
                jsonFromVar(manifest->pub.data.backupOptionStandby));
        }

        if (manifest->pub.data.backupOptionBufferSize != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_BUFFER_SIZE,
                jsonFromVar(manifest->pub.data.backupOptionBufferSize));
        }

        if (manifest->pub.data.backupOptionChecksumPage != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_CHECKSUM_PAGE,
                jsonFromVar(manifest->pub.data.backupOptionChecksumPage));
        }

        // Set the option when compression is turned on.  In older versions this also implied gz compression but in newer versions
        // the type option must also be set if compression is not gz.
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_COMPRESS,
            jsonFromVar(VARBOOL(manifest->pub.data.backupOptionCompressType != compressTypeNone)));

        if (manifest->pub.data.backupOptionCompressLevel != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_COMPRESS_LEVEL,
                jsonFromVar(manifest->pub.data.backupOptionCompressLevel));
        }

        if (manifest->pub.data.backupOptionCompressLevelNetwork != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK,
                jsonFromVar(manifest->pub.data.backupOptionCompressLevelNetwork));
        }

        // Set the compression type.  Older versions will ignore this and assume gz compression if the compress option is set.
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_COMPRESS_TYPE,
            jsonFromVar(VARSTR(compressTypeStr(manifest->pub.data.backupOptionCompressType))));

        if (manifest->pub.data.backupOptionDelta != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_DELTA,
                jsonFromVar(manifest->pub.data.backupOptionDelta));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_HARDLINK,
            jsonFromVar(VARBOOL(manifest->pub.data.backupOptionHardLink)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_ONLINE,
            jsonFromVar(VARBOOL(manifest->pub.data.backupOptionOnline)));

        if (manifest->pub.data.backupOptionProcessMax != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION, MANIFEST_KEY_OPTION_PROCESS_MAX,
                jsonFromVar(manifest->pub.data.backupOptionProcessMax));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_TARGET, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *const target = manifestTarget(manifest, targetIdx);
                JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

                if (target->file != NULL)
                    jsonWriteStr(jsonWriteKeyStrId(json, MANIFEST_KEY_FILE), target->file);

                jsonWriteStr(jsonWriteKeyStrId(json, MANIFEST_KEY_PATH), target->path);

                if (target->tablespaceId != 0)
                    jsonWriteStrFmt(jsonWriteKeyZ(json, MANIFEST_KEY_TABLESPACE_ID), "%u", target->tablespaceId);

                if (target->tablespaceName != NULL)
                    jsonWriteStr(jsonWriteKeyZ(json, MANIFEST_KEY_TABLESPACE_NAME), target->tablespaceName);

                jsonWriteZ(
                    jsonWriteKeyStrId(json, MANIFEST_KEY_TYPE),
                    target->type == manifestTargetTypePath ? MANIFEST_TARGET_TYPE_PATH : MANIFEST_TARGET_TYPE_LINK);

                infoSaveValue(
                    infoSaveData, MANIFEST_SECTION_BACKUP_TARGET, strZ(target->name), jsonWriteResult(jsonWriteObjectEnd(json)));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_DB, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int dbIdx = 0; dbIdx < manifestDbTotal(manifest); dbIdx++)
            {
                const ManifestDb *const db = manifestDb(manifest, dbIdx);
                JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

                jsonWriteUInt(jsonWriteKeyZ(json, MANIFEST_KEY_DB_ID), db->id);
                jsonWriteUInt(jsonWriteKeyZ(json, MANIFEST_KEY_DB_LAST_SYSTEM_ID), db->lastSystemId);

                infoSaveValue(infoSaveData, MANIFEST_SECTION_DB, strZ(db->name), jsonWriteResult(jsonWriteObjectEnd(json)));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_METADATA, sectionNext))
    {
        if (manifest->pub.data.annotation != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_METADATA, MANIFEST_KEY_ANNOTATION,
                jsonFromVar(manifest->pub.data.annotation));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_FILE, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile file = manifestFile(manifest, fileIdx);
                JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

                // Bundle info
                if (file.bundleId != 0)
                {
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BUNDLE_ID), file.bundleId);

                    if (file.bundleOffset != 0)
                        jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BUNDLE_OFFSET), file.bundleOffset);
                }

                // Save if the file size is not zero and the checksum exists.  The checksum might not exist if this is a partial
                // save performed during a backup.
                if (file.size != 0 && file.checksumSha1[0] != 0)
                    jsonWriteZ(jsonWriteKeyStrId(json, MANIFEST_KEY_CHECKSUM), file.checksumSha1);

                if (file.checksumPage)
                {
                    jsonWriteBool(jsonWriteKeyZ(json, MANIFEST_KEY_CHECKSUM_PAGE), !file.checksumPageError);

                    if (file.checksumPageErrorList != NULL)
                        jsonWriteJson(jsonWriteKeyZ(json, MANIFEST_KEY_CHECKSUM_PAGE_ERROR), file.checksumPageErrorList);
                }

                if (!varEq(manifestOwnerVar(file.group), saveData->groupDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_GROUP), manifestOwnerVar(file.group));

                if (file.mode != saveData->fileModeDefault)
                    jsonWriteStrFmt(jsonWriteKeyZ(json, MANIFEST_KEY_MODE), "%04o", file.mode);

                if (file.reference != NULL)
                    jsonWriteStr(jsonWriteKeyStrId(json, MANIFEST_KEY_REFERENCE), file.reference);

                if (file.sizeRepo != file.size)
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_SIZE_REPO), file.sizeRepo);

                jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_SIZE), file.size);
                jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_TIMESTAMP), (uint64_t)file.timestamp);

                if (!varEq(manifestOwnerVar(file.user), saveData->userDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_USER), manifestOwnerVar(file.user));

                infoSaveValue(
                    infoSaveData, MANIFEST_SECTION_TARGET_FILE, strZ(file.name), jsonWriteResult(jsonWriteObjectEnd(json)));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT, sectionNext))
    {
        infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT, MANIFEST_KEY_GROUP, jsonFromVar(saveData->groupDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT, MANIFEST_KEY_MODE,
            jsonFromVar(VARSTR(strNewFmt("%04o", saveData->fileModeDefault))));
        infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT, MANIFEST_KEY_USER, jsonFromVar(saveData->userDefault));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_LINK, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(manifest); linkIdx++)
            {
                const ManifestLink *const link = manifestLink(manifest, linkIdx);
                JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

                jsonWriteStr(jsonWriteKeyStrId(json, MANIFEST_KEY_DESTINATION), link->destination);

                if (!varEq(manifestOwnerVar(link->group), saveData->groupDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_GROUP), manifestOwnerVar(link->group));

                if (!varEq(manifestOwnerVar(link->user), saveData->userDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_USER), manifestOwnerVar(link->user));

                infoSaveValue(
                    infoSaveData, MANIFEST_SECTION_TARGET_LINK, strZ(link->name), jsonWriteResult(jsonWriteObjectEnd(json)));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT, sectionNext))
    {
        if (manifestLinkTotal(manifest) > 0)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT, MANIFEST_KEY_GROUP, jsonFromVar(saveData->groupDefault));
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT, MANIFEST_KEY_USER, jsonFromVar(saveData->userDefault));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_PATH, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                const ManifestPath *const path = manifestPath(manifest, pathIdx);
                JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());

                if (!varEq(manifestOwnerVar(path->group), saveData->groupDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_GROUP), manifestOwnerVar(path->group));

                if (path->mode != saveData->pathModeDefault)
                    jsonWriteStrFmt(jsonWriteKeyZ(json, MANIFEST_KEY_MODE), "%04o", path->mode);

                if (!varEq(manifestOwnerVar(path->user), saveData->userDefault))
                    jsonWriteVar(jsonWriteKeyZ(json, MANIFEST_KEY_USER), manifestOwnerVar(path->user));

                infoSaveValue(
                    infoSaveData, MANIFEST_SECTION_TARGET_PATH, strZ(path->name), jsonWriteResult(jsonWriteObjectEnd(json)));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT, sectionNext))
    {
        infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT, MANIFEST_KEY_GROUP, jsonFromVar(saveData->groupDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT, MANIFEST_KEY_MODE,
            jsonFromVar(VARSTR(strNewFmt("%04o", saveData->pathModeDefault))));
        infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT, MANIFEST_KEY_USER, jsonFromVar(saveData->userDefault));
    }

    FUNCTION_TEST_RETURN_VOID();
}

void
manifestSave(Manifest *this, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Files can be added from outside the manifest so make sure they are sorted
        lstSort(this->pub.fileList, sortOrderAsc);

        // Set default values based on the base path
        const ManifestPath *const pathBase = manifestPathFind(this, MANIFEST_TARGET_PGDATA_STR);

        ManifestSaveData saveData =
        {
            .manifest = this,
            .userDefault = manifestOwnerVar(pathBase->user),
            .groupDefault = manifestOwnerVar(pathBase->group),
            .fileModeDefault = pathBase->mode & (S_IRUSR | S_IWUSR | S_IRGRP),
            .pathModeDefault = pathBase->mode,
        };

        // Save manifest
        infoSave(this->pub.info, write, manifestSaveCallback, &saveData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
manifestValidate(Manifest *this, bool strict)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(BOOL, strict);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *error = strNew();

        // Validate files
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
        {
            const ManifestFile file = manifestFile(this, fileIdx);

            // All files must have a checksum
            if (file.checksumSha1[0] == '\0')
                strCatFmt(error, "\nmissing checksum for file '%s'", strZ(file.name));

            // These are strict checks to be performed only after a backup and before the final manifest save
            if (strict)
            {
                // Zero-length files must have a specific checksum
                if (file.size == 0 && !strEqZ(HASH_TYPE_SHA1_ZERO_STR, file.checksumSha1))
                    strCatFmt(error, "\ninvalid checksum '%s' for zero size file '%s'", file.checksumSha1, strZ(file.name));

                // Non-zero size files must have non-zero repo size
                if (file.sizeRepo == 0 && file.size != 0)
                    strCatFmt(error, "\nrepo size must be > 0 for file '%s'", strZ(file.name));
            }
        }

        // Throw exception when there are errors
        if (strSize(error) > 0)
            THROW_FMT(FormatError, "manifest validation failed:%s", strZ(error));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Db functions and getters/setters
***********************************************************************************************************************************/
const ManifestDb *
manifestDbFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestDb *result = lstFind(this->pub.dbList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest db list", strZ(name));

    FUNCTION_TEST_RETURN_CONST(MANIFEST_DB, result);
}

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
static ManifestFilePack **
manifestFilePackFindInternal(const Manifest *this, const String *name)
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
manifestFilePackFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN_TYPE_P(ManifestFilePack, *manifestFilePackFindInternal(this, name));
}

void
manifestFileRemove(const Manifest *this, const String *name)
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

void
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
Link functions and getters/setters
***********************************************************************************************************************************/
const ManifestLink *
manifestLinkFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestLink *result = lstFind(this->pub.linkList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest link list", strZ(name));

    FUNCTION_TEST_RETURN_CONST(MANIFEST_LINK, result);
}

void
manifestLinkRemove(const Manifest *this, const String *name)
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

void
manifestLinkUpdate(const Manifest *this, const String *name, const String *destination)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(STRING, destination);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);
    ASSERT(destination != NULL);

    ManifestLink *link = (ManifestLink *)manifestLinkFind(this, name);

    MEM_CONTEXT_BEGIN(lstMemContext(this->pub.linkList))
    {
        if (!strEq(link->destination, destination))
            link->destination = strDup(destination);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Path functions and getters/setters
***********************************************************************************************************************************/
const ManifestPath *
manifestPathFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestPath *result = lstFind(this->pub.pathList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest path list", strZ(name));

    FUNCTION_TEST_RETURN_CONST(MANIFEST_PATH, result);
}

String *
manifestPathPg(const String *manifestPath)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, manifestPath);
    FUNCTION_TEST_END();

    ASSERT(manifestPath != NULL);

    // If something in pg_data/
    if (strBeginsWith(manifestPath, STRDEF(MANIFEST_TARGET_PGDATA "/")))
    {
        FUNCTION_TEST_RETURN(STRING, strNewZ(strZ(manifestPath) + sizeof(MANIFEST_TARGET_PGDATA)));
    }
    // Else not pg_data (this is faster since the length of everything else will be different than pg_data)
    else if (!strEq(manifestPath, MANIFEST_TARGET_PGDATA_STR))
    {
        // A tablespace target is the only valid option if not pg_data or pg_data/
        ASSERT(
            strEq(manifestPath, MANIFEST_TARGET_PGTBLSPC_STR) || strBeginsWith(manifestPath, STRDEF(MANIFEST_TARGET_PGTBLSPC "/")));

        FUNCTION_TEST_RETURN(STRING, strDup(manifestPath));
    }

    FUNCTION_TEST_RETURN(STRING, NULL);
}

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
const ManifestTarget *
manifestTargetFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestTarget *result = lstFind(this->pub.targetList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest target list", strZ(name));

    FUNCTION_TEST_RETURN_CONST(MANIFEST_TARGET, result);
}

String *
manifestTargetPath(const Manifest *this, const ManifestTarget *target)
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
    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *pgPath = strCat(strNew(), strPath(manifestPathPg(target->name)));

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

void
manifestTargetRemove(const Manifest *this, const String *name)
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

void
manifestTargetUpdate(const Manifest *this, const String *name, const String *path, const String *file)
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

    ManifestTarget *target = (ManifestTarget *)manifestTargetFind(this, name);

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

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
void
manifestBackupLabelSet(Manifest *this, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_BEGIN(this->pub.memContext)
    {
        this->pub.data.backupLabel = strDup(backupLabel);
        strLstAdd(this->pub.referenceList, backupLabel);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
typedef struct ManifestLoadFileData
{
    MemContext *memContext;                                         // Mem context
    const Storage *storage;                                         // Storage to load from
    const String *fileName;                                         // Base filename
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    Manifest *manifest;                                             // Loaded manifest object
} ManifestLoadFileData;

static bool
manifestLoadFileCallback(void *const data, const unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    ManifestLoadFileData *const loadData = data;
    bool result = false;

    if (try < 2)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Construct filename based on try
            const String *const fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strZ(loadData->fileName));

            // Attempt to load the file
            IoRead *const read = storageReadIo(storageNewReadP(loadData->storage, fileName));
            cipherBlockFilterGroupAdd(ioReadFilterGroup(read), loadData->cipherType, cipherModeDecrypt, loadData->cipherPass);

            MEM_CONTEXT_BEGIN(loadData->memContext)
            {
                loadData->manifest = manifestNewLoad(read);
                result = true;
            }
            MEM_CONTEXT_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

Manifest *
manifestLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    ManifestLoadFileData data =
    {
        .memContext = memContextCurrent(),
        .storage = storage,
        .fileName = fileName,
        .cipherType = cipherType,
        .cipherPass = cipherPass,
    };

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *fileNamePath = strZ(storagePathP(storage, fileName));

        infoLoad(
            strNewFmt("unable to load backup manifest file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
            manifestLoadFileCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, data.manifest);
}
