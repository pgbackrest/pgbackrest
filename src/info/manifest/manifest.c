/***********************************************************************************************************************************
Backup Manifest Handler
***********************************************************************************************************************************/
#include <build.h>

#include <ctype.h>
#include <string.h>
#include <time.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "info/manifest/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/storage.h"
#include "version.h"

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

// {uncrustify_off - order required for C includes}
#include "store.c.inc"
#include "link.c.inc"
#include "build.c.inc"
#include "serialize.c.inc"
// {uncrustify_on}

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static Manifest *
manifestNewInternal(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_AUDIT_HELPER();

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

// Regular expression constants
#define RELATION_EXP                                                "[0-9]+(_(fsm|vm)){0,1}(\\.[0-9]+){0,1}"
#define DB_PATH_EXP                                                                                                                \
    "(" MANIFEST_TARGET_PGDATA "/(" PG_PATH_GLOBAL "|" PG_PATH_BASE "/[0-9]+)|" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+)"

FN_EXTERN Manifest *
manifestNewBuild(
    const Storage *const storagePg, const unsigned int pgVersion, const unsigned int pgCatalogVersion, const time_t timestampStart,
    const bool online, const bool checksumPage, const bool bundle, const bool blockIncr, const ManifestBlockIncrMap *blockIncrMap,
    const StringList *const excludeList, const Pack *const tablespaceList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT, pgCatalogVersion);
        FUNCTION_LOG_PARAM(TIME, timestampStart);
        FUNCTION_LOG_PARAM(BOOL, online);
        FUNCTION_LOG_PARAM(BOOL, checksumPage);
        FUNCTION_LOG_PARAM(BOOL, bundle);
        FUNCTION_LOG_PARAM(BOOL, blockIncr);
        FUNCTION_LOG_PARAM(VOID, blockIncrMap);
        FUNCTION_LOG_PARAM(STRING_LIST, excludeList);
        FUNCTION_LOG_PARAM(PACK, tablespaceList);
    FUNCTION_LOG_END();

    ASSERT(storagePg != NULL);
    ASSERT(pgVersion != 0);

    Manifest *this;

    OBJ_NEW_BASE_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        this = manifestNewInternal();
        this->pub.info = infoNew(NULL);
        this->pub.data.backrestVersion = strNewZ(PROJECT_VERSION);
        this->pub.data.pgVersion = pgVersion;
        this->pub.data.pgCatalogVersion = pgCatalogVersion;
        this->pub.data.backupTimestampStart = timestampStart;
        this->pub.data.backupType = backupTypeFull;
        this->pub.data.backupOptionOnline = online;
        this->pub.data.backupOptionChecksumPage = varNewBool(checksumPage);
        this->pub.data.bundle = bundle;
        this->pub.data.bundleRaw = blockIncr;
        this->pub.data.blockIncr = blockIncr;

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
                .blockIncrMap = blockIncrMap,
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
                    const String *const exclude = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(strLstGet(excludeList, excludeIdx)));

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
            const StorageInfo info = storageInfoP(storagePg, pgPath, .followLink = true);

            const ManifestPath path =
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

            const ManifestTarget target =
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

            // Remove unlogged relations from the manifest. This can't be done during the initial build because of the requirement
            // to check for _init files which will sort after the vast majority of the relation files. We could check storage for
            // each _init file but that would be expensive.
            // -------------------------------------------------------------------------------------------------------------------------
            RegExp *relationExp = regExpNew(strNewFmt("^" DB_PATH_EXP "/" RELATION_EXP "$", strZ(buildData.tablespaceId)));
            unsigned int fileIdx = 0;
            char lastRelationFileId[21] = "";                   // Large enough for a 64-bit unsigned integer
            bool lastRelationFileIdUnlogged = false;

#ifdef DEBUG_MEM
            // Record the temp context size before the loop begins
            const size_t sizeBegin = memContextSize(memContextCurrent());
#endif

            while (fileIdx < manifestFileTotal(this))
            {
                // If this file looks like a relation. Note that this never matches on _init forks.
                const String *const filePathName = manifestFileNameGet(this, fileIdx);

                if (regExpMatch(relationExp, filePathName))
                {
                    // Get the filename (without path)
                    const char *const fileName = strBaseZ(filePathName);
                    const size_t fileNameSize = strlen(fileName);

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

                    // Store the last relation so it does not need to be found every time
                    if (strcmp(lastRelationFileId, relationFileId) != 0)
                    {
                        // Determine if the relation is unlogged
                        String *const relationInit = strNewFmt(
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
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(MANIFEST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
manifestBuildValidate(Manifest *const this, const bool delta, const time_t copyStart, const CompressType compressType)
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
        // Store the delta option. If true we can skip checks that automatically enable delta.
        this->pub.data.backupOptionDelta = varNewBool(delta);

        // If online then add one second to the copy start time to allow for database updates during the last second that the
        // manifest was being built. It's up to the caller to actually wait the remainder of the second, but for comparison purposes
        // we want the time when the waiting started.
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
                        " checksum",
                        strZ(manifestPathPg(file.name)), (int64_t)file.timestamp, (int64_t)copyStart);

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
static void
manifestDeltaCheck(Manifest *const this, const Manifest *const manifestPrior)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(MANIFEST, manifestPrior);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check for anomalies between manifests if delta is not already enabled
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
                    if (file.sizeOriginal != filePrior.sizeOriginal && file.timestamp == filePrior.timestamp)
                    {
                        LOG_WARN_FMT(
                            "file '%s' has same timestamp (%" PRId64 ") as prior but different size (prior %" PRIu64 ", current"
                            " %" PRIu64 "), enabling delta checksum",
                            strZ(manifestPathPg(file.name)), (int64_t)file.timestamp, filePrior.sizeOriginal, file.sizeOriginal);

                        this->pub.data.backupOptionDelta = BOOL_TRUE_VAR;
                        break;
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
manifestBuildIncr(
    Manifest *const this, const Manifest *const manifestPrior, const BackupType type, const String *const archiveStart)
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

        // Bundle raw must not change in a backup set
        this->pub.data.bundleRaw = manifestPrior->pub.data.bundleRaw;
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

        // Enable delta if/when there are timestamp anomalies
        manifestDeltaCheck(this, manifestPrior);

        // Find files to (possibly) reference in the prior manifest
        const bool delta = varBool(this->pub.data.backupOptionDelta);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->pub.fileList); fileIdx++)
        {
            ManifestFile file = manifestFile(this, fileIdx);

            // Check if a prior file exists for files that will be copied (i.e. not zero-length files when bundling). If a prior
            // file does exist it may be possible to reference it instead of copying the file.
            if (file.copy && manifestFileExists(manifestPrior, file.name))
            {
                const ManifestFile filePrior = manifestFileFind(manifestPrior, file.name);

                // If file size is equal to prior size then the file can be referenced instead of copied if it has not changed (this
                // must be determined during the backup).
                const bool fileSizeEqual = file.size == filePrior.size;

                // If prior file was stored with block incremental and file size is at least prior file block size then preserve
                // prior values. If file size is less than prior file block size then the entire file will need to be stored and the
                // map will be useless as long as the file stays at this size. It is not possible to change the block size in a
                // backup set and the map info will be required to compare against the prior block incremental.
                const bool fileBlockIncrPreserve = filePrior.blockIncrMapSize > 0 && file.size >= filePrior.blockIncrSize;

                // Perform delta if enabled and file size is equal to prior but not zero. Files of unequal length are always
                // different while zero-length files are always the same, so it wastes time to check them. It is possible for a file
                // to be truncated down to equal the prior file during backup, but the overhead of checking for such an unlikely
                // event does not seem worth the possible space saved.
                file.delta = delta && fileSizeEqual && file.size != 0;

                // Do not copy if size and prior size are both zero. Zero-length files are always equal so the file can simply be
                // referenced to the prior file. Note that this is only for the case where zero-length files are being explicitly
                // written to the repo. Bundled zero-length files disable copy at manifest build time and never reference the prior
                // file, even if it is also zero-length.
                if (file.size == 0 && filePrior.size == 0)
                    file.copy = false;

                // If delta is disabled and size/timestamp are equal then the file is not copied
                if (!file.delta && fileSizeEqual && file.timestamp == filePrior.timestamp)
                    file.copy = false;

                ASSERT(file.copy || !file.delta);
                ASSERT(file.copy || fileSizeEqual);
                ASSERT(!file.delta || fileSizeEqual);

                // Preserve values if the file will not be copied, is possibly equal to the prior file, or will be stored with block
                // incremental and the prior file is also stored with block incremental. If the file will not be copied then the
                // file size must be equal to the prior file so there is no need to check that condition separately.
                if (fileSizeEqual || fileBlockIncrPreserve)
                {
                    file.size = filePrior.size;
                    file.sizeRepo = filePrior.sizeRepo;
                    file.checksumSha1 = filePrior.checksumSha1;
                    file.checksumRepoSha1 = filePrior.checksumRepoSha1;
                    file.reference = filePrior.reference != NULL ? filePrior.reference : manifestPrior->pub.data.backupLabel;
                    file.checksumPage = filePrior.checksumPage;
                    file.checksumPageError = filePrior.checksumPageError;
                    file.checksumPageErrorList = filePrior.checksumPageErrorList;
                    file.bundleId = filePrior.bundleId;
                    file.bundleOffset = filePrior.bundleOffset;
                    file.blockIncrSize = filePrior.blockIncrSize;
                    file.blockIncrChecksumSize = filePrior.blockIncrChecksumSize;
                    file.blockIncrMapSize = filePrior.blockIncrMapSize;

                    ASSERT(file.checksumSha1 != NULL);
                    ASSERT(
                        (!file.checksumPage && !file.checksumPageError && file.checksumPageErrorList == NULL) ||
                        (file.checksumPage && !file.checksumPageError && file.checksumPageErrorList == NULL) ||
                        (file.checksumPage && file.checksumPageError));
                    ASSERT(file.size == 0 || file.sizeRepo != 0);
                    ASSERT(file.reference != NULL);
                    ASSERT(file.bundleId != 0 || file.bundleOffset == 0);
                    ASSERT(
                        (file.blockIncrSize == 0 && file.blockIncrChecksumSize == 0 && file.blockIncrMapSize == 0) ||
                        (file.blockIncrSize > 0 && file.blockIncrChecksumSize > 0 && file.blockIncrMapSize > 0));
                }

                manifestFileUpdate(this, &file);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
manifestBuildComplete(
    Manifest *const this, const String *const lsnStart, const String *const archiveStart, const time_t timestampStop,
    const String *const lsnStop, const String *const archiveStop, const unsigned int pgId, const uint64_t pgSystemId,
    const Pack *const dbList, const bool optionArchiveCheck, const bool optionArchiveCopy, const size_t optionBufferSize,
    const unsigned int optionCompressLevel, const unsigned int optionCompressLevelNetwork, const bool optionHardLink,
    const unsigned int optionProcessMax, const bool optionStandby, const KeyValue *const annotation)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
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
FN_EXTERN Manifest *
manifestNewLoad(IoRead *const read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    Manifest *this;

    OBJ_NEW_BASE_BEGIN(Manifest, .childQty = MEM_CONTEXT_QTY_MAX)
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

        // Add the label to the reference list in case the manifest was created before 2.42 when the explicit reference list was
        // added. Most references are added when the file list is loaded but the current backup will never be referenced from a file
        // (the reference is assumed) so it must be added here.
        if (!loadData.referenceListFound)
            strLstAddIfMissing(this->pub.referenceList, this->pub.data.backupLabel);

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
            ManifestPath *const path = lstGet(this->pub.pathList, pathIdx);
            const ManifestLoadFound *const found = lstGet(loadData.pathFoundList, pathIdx);

            if (!found->group)
                path->group = manifestOwnerCache(this, manifestOwnerGet(loadData.pathGroupDefault));

            if (!found->mode)
                path->mode = loadData.pathModeDefault;

            if (!found->user)
                path->user = manifestOwnerCache(this, manifestOwnerGet(loadData.pathUserDefault));
        }

        // Sort the lists. They should already be sorted in the file but it is possible that this system has a different collation
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
FN_EXTERN void
manifestValidate(Manifest *const this, const bool strict)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM(BOOL, strict);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const error = strNew();

        // Validate files
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
        {
            const ManifestFile file = manifestFile(this, fileIdx);

            // All files must have a checksum
            if (file.checksumSha1 == NULL)
                strCatFmt(error, "\nmissing checksum for file '%s'", strZ(file.name));

            // These are strict checks to be performed only after a backup and before the final manifest save
            if (strict)
            {
                // Zero-length files must have a specific checksum
                if (file.size == 0 && !bufEq(HASH_TYPE_SHA1_ZERO_BUF, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE)))
                {
                    strCatFmt(
                        error, "\ninvalid checksum '%s' for zero size file '%s'",
                        strZ(strNewEncode(encodingHex, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE))), strZ(file.name));
                }

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

/**********************************************************************************************************************************/
FN_EXTERN String *
manifestPathPg(const String *const manifestPath)
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

/**********************************************************************************************************************************/
FN_EXTERN void
manifestBackupLabelSet(Manifest *const this, const String *const backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(this->pub.data.backupLabel == NULL);

    MEM_CONTEXT_BEGIN(this->pub.memContext)
    {
        this->pub.data.backupLabel = strDup(backupLabel);
        strLstAdd(this->pub.referenceList, backupLabel);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}
