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
#define MANIFEST_KEY_BACKUP_BLOCK_INCR                              "backup-block-incr"
#define MANIFEST_KEY_BACKUP_BUNDLE                                  "backup-bundle"
#define MANIFEST_KEY_BACKUP_BUNDLE_RAW                              "backup-bundle-raw"
#define MANIFEST_KEY_BACKUP_LABEL                                   "backup-label"
#define MANIFEST_KEY_BACKUP_LSN_START                               "backup-lsn-start"
#define MANIFEST_KEY_BACKUP_LSN_STOP                                "backup-lsn-stop"
#define MANIFEST_KEY_BACKUP_PRIOR                                   "backup-prior"
#define MANIFEST_KEY_BACKUP_REFERENCE                               "backup-reference"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START                    "backup-timestamp-copy-start"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_START                         "backup-timestamp-start"
#define MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                          "backup-timestamp-stop"
#define MANIFEST_KEY_BACKUP_TYPE                                    "backup-type"
#define MANIFEST_KEY_BLOCK_INCR                                     STRID5("bi", 0x1220)
#define MANIFEST_KEY_BLOCK_INCR_CHECKSUM                            STRID5("bic", 0xd220)
#define MANIFEST_KEY_BLOCK_INCR_MAP                                 STRID5("bim", 0x35220)
#define MANIFEST_KEY_BUNDLE_ID                                      STRID5("bni", 0x25c20)
#define MANIFEST_KEY_BUNDLE_OFFSET                                  STRID5("bno", 0x3dc20)
#define MANIFEST_KEY_CHECKSUM                                       STRID5("checksum", 0x6d66b195030)
#define MANIFEST_KEY_CHECKSUM_REPO                                  STRID5("rck", 0x2c720)
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
#define MANIFEST_KEY_SIZE_ORIGINAL                                  STRID5("szo", 0x3f530)
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
    bool group : 1;
    bool mode : 1;
    bool user : 1;
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

// Helper to transform a variant that could be boolean or string into a string. If the boolean is false return NULL else return the
// string. The boolean cannot be true.
static const String *
manifestOwnerGet(const Variant *const owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(VARIANT, owner);
    FUNCTION_TEST_END();

    ASSERT(owner != NULL);

    // If bool then it should be false. This indicates that the owner could not be mapped to a name during the backup.
    if (varType(owner) == varTypeBool)
    {
        CHECK(FormatError, !varBool(owner), "owner bool must be false");
        FUNCTION_TEST_RETURN_CONST(STRING, NULL);
    }

    FUNCTION_TEST_RETURN_CONST(STRING, varStr(owner));
}

// Helper to check the variant type of owner and duplicate (call in the containing context)
static const Variant *
manifestOwnerDefaultGet(const Variant *const ownerDefault)
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
manifestLoadCallback(void *const callbackData, const String *const section, const String *const key, JsonRead *const json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, section);
        FUNCTION_TEST_PARAM(STRING, key);
        FUNCTION_TEST_PARAM(JSON_READ, json);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_CALLBACK();

    ASSERT(callbackData != NULL);
    ASSERT(section != NULL);
    ASSERT(key != NULL);
    ASSERT(json != NULL);

    ManifestLoadData *const loadData = (ManifestLoadData *)callbackData;
    Manifest *const manifest = loadData->manifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (strEqZ(section, MANIFEST_SECTION_TARGET_FILE))
    {
        ManifestFile file = {.name = key};

        jsonReadObjectBegin(json);

        // Block incremental info
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_BLOCK_INCR))
        {
            file.blockIncrSize = (size_t)jsonReadUInt64(json) * BLOCK_INCR_SIZE_FACTOR;

            if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_BLOCK_INCR_CHECKSUM))
                file.blockIncrChecksumSize = (size_t)jsonReadUInt64(json);
            else
                file.blockIncrChecksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN;

            if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_BLOCK_INCR_MAP))
                file.blockIncrMapSize = jsonReadUInt64(json);
        }

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
            file.checksumSha1 = bufPtr(bufNewDecode(encodingHex, jsonReadStr(json)));

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

        // The repo checksum might not exist if this is a partial save that was done during the backup to preserve checksums for
        // already backed up files or if this is an older manifest
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_CHECKSUM_REPO))
            file.checksumRepoSha1 = bufPtr(bufNewDecode(encodingHex, jsonReadStr(json)));

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
            file.checksumSha1 = bufPtrConst(HASH_TYPE_SHA1_ZERO_BUF);

        // If original is not present in the manifest file then it is the same as size (i.e. the file did not change size during
        // copy) -- to save space the original size is only stored in the manifest file if it is different than size.
        if (jsonReadKeyExpectStrId(json, MANIFEST_KEY_SIZE_ORIGINAL))
            file.sizeOriginal = jsonReadUInt64(json);
        else
            file.sizeOriginal = file.size;

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
        if (strEqZ(key, MANIFEST_KEY_MODE))
            manifest->fileModeDefault = cvtZToMode(strZ(jsonReadStr(json)));
        else
        {
            const Variant *const valueVar = jsonReadVar(json);

            MEM_CONTEXT_BEGIN(manifest->pub.memContext)
            {
                if (strEqZ(key, MANIFEST_KEY_GROUP))
                    manifest->fileGroupDefault = strDup(manifestOwnerGet(valueVar));
                else if (strEqZ(key, MANIFEST_KEY_USER))
                    manifest->fileUserDefault = strDup(manifestOwnerGet(valueVar));
            }
            MEM_CONTEXT_END();
        }
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_PATH_DEFAULT))
    {
        if (strEqZ(key, MANIFEST_KEY_MODE))
            loadData->pathModeDefault = cvtZToMode(strZ(jsonReadStr(json)));
        else
        {
            const Variant *const valueVar = jsonReadVar(json);

            MEM_CONTEXT_BEGIN(loadData->memContext)
            {
                if (strEqZ(key, MANIFEST_KEY_GROUP))
                    loadData->pathGroupDefault = manifestOwnerDefaultGet(valueVar);
                else if (strEqZ(key, MANIFEST_KEY_USER))
                    loadData->pathUserDefault = manifestOwnerDefaultGet(valueVar);
            }
            MEM_CONTEXT_END();
        }
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT))
    {
        const Variant *const valueVar = jsonReadVar(json);

        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEqZ(key, MANIFEST_KEY_GROUP))
                loadData->linkGroupDefault = manifestOwnerDefaultGet(valueVar);
            else if (strEqZ(key, MANIFEST_KEY_USER))
                loadData->linkUserDefault = manifestOwnerDefaultGet(valueVar);
        }
        MEM_CONTEXT_END();
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_TARGET))
    {
        ManifestTarget target = {.name = key};

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
                manifest->pub.data.annotation = jsonReadVar(json);
        }
        MEM_CONTEXT_END();
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP))
    {
        if (strEqZ(key, MANIFEST_KEY_BACKUP_BLOCK_INCR))
            manifest->pub.data.blockIncr = jsonReadBool(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_BUNDLE))
            manifest->pub.data.bundle = jsonReadBool(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_BUNDLE_RAW))
            manifest->pub.data.bundleRaw = jsonReadBool(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START))
            manifest->pub.data.backupTimestampCopyStart = (time_t)jsonReadUInt64(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_START))
            manifest->pub.data.backupTimestampStart = (time_t)jsonReadUInt64(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_TIMESTAMP_STOP))
            manifest->pub.data.backupTimestampStop = (time_t)jsonReadUInt64(json);
        else if (strEqZ(key, MANIFEST_KEY_BACKUP_TYPE))
        {
            manifest->pub.data.backupType = (BackupType)jsonReadStrId(json);
            ASSERT(
                manifest->pub.data.backupType == backupTypeFull || manifest->pub.data.backupType == backupTypeDiff ||
                manifest->pub.data.backupType == backupTypeIncr);
        }
        else
        {
            MEM_CONTEXT_BEGIN(manifest->pub.memContext)
            {
                String *const valueStr = jsonReadStr(json);

                if (strEqZ(key, MANIFEST_KEY_BACKUP_ARCHIVE_START))
                    manifest->pub.data.archiveStart = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_ARCHIVE_STOP))
                    manifest->pub.data.archiveStop = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_LABEL))
                    manifest->pub.data.backupLabel = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_LSN_START))
                    manifest->pub.data.lsnStart = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_LSN_STOP))
                    manifest->pub.data.lsnStop = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_PRIOR))
                    manifest->pub.data.backupLabelPrior = valueStr;
                else if (strEqZ(key, MANIFEST_KEY_BACKUP_REFERENCE))
                {
                    manifest->pub.referenceList = strLstNewSplitZ(valueStr, ",");
                    loadData->referenceListFound = true;

                    strFree(valueStr);
                }
            }
            MEM_CONTEXT_END();
        }
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_DB))
    {
        if (strEqZ(key, MANIFEST_KEY_DB_ID))
            manifest->pub.data.pgId = jsonReadUInt(json);
        else if (strEqZ(key, MANIFEST_KEY_DB_SYSTEM_ID))
            manifest->pub.data.pgSystemId = jsonReadUInt64(json);
        else if (strEqZ(key, MANIFEST_KEY_DB_CATALOG_VERSION))
            manifest->pub.data.pgCatalogVersion = jsonReadUInt(json);
        else if (strEqZ(key, MANIFEST_KEY_DB_VERSION))
            manifest->pub.data.pgVersion = pgVersionFromStr(jsonReadStr(json));
    }
    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEqZ(section, MANIFEST_SECTION_BACKUP_OPTION))
    {
        // Required options
        if (strEqZ(key, MANIFEST_KEY_OPTION_ARCHIVE_CHECK))
            manifest->pub.data.backupOptionArchiveCheck = jsonReadBool(json);
        else if (strEqZ(key, MANIFEST_KEY_OPTION_ARCHIVE_COPY))
            manifest->pub.data.backupOptionArchiveCopy = jsonReadBool(json);
        // Historically this option meant to add gz compression
        else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS))
            manifest->pub.data.backupOptionCompressType = jsonReadBool(json) ? compressTypeGz : compressTypeNone;
        // This new option allows any type of compression to be specified. It must be parsed after the option above so the value
        // does not get overwritten. Since options are stored in alpha order this should always be true.
        else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_TYPE))
            manifest->pub.data.backupOptionCompressType = compressTypeEnum(strStrId(jsonReadStr(json)));
        else if (strEqZ(key, MANIFEST_KEY_OPTION_HARDLINK))
            manifest->pub.data.backupOptionHardLink = jsonReadBool(json);
        else if (strEqZ(key, MANIFEST_KEY_OPTION_ONLINE))
            manifest->pub.data.backupOptionOnline = jsonReadBool(json);
        else
        {
            // Options that were added after v1.00 and may not be present in every manifest
            MEM_CONTEXT_BEGIN(manifest->pub.memContext)
            {
                if (strEqZ(key, MANIFEST_KEY_OPTION_BUFFER_SIZE))
                    manifest->pub.data.backupOptionBufferSize = varNewUInt(jsonReadUInt(json));
                else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL))
                    manifest->pub.data.backupOptionCompressLevel = varNewUInt(jsonReadUInt(json));
                else if (strEqZ(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK))
                    manifest->pub.data.backupOptionCompressLevelNetwork = varNewUInt(jsonReadUInt(json));
                else if (strEqZ(key, MANIFEST_KEY_OPTION_PROCESS_MAX))
                    manifest->pub.data.backupOptionProcessMax = varNewUInt(jsonReadUInt(json));
                else if (strEqZ(key, MANIFEST_KEY_OPTION_BACKUP_STANDBY))
                    manifest->pub.data.backupOptionStandby = varNewBool(jsonReadBool(json));
                else if (strEqZ(key, MANIFEST_KEY_OPTION_CHECKSUM_PAGE))
                    manifest->pub.data.backupOptionChecksumPage = jsonReadVar(json);
                else if (strEqZ(key, MANIFEST_KEY_OPTION_DELTA))
                    manifest->pub.data.backupOptionDelta = jsonReadVar(json);
            }
            MEM_CONTEXT_END();
        }
    }

    FUNCTION_TEST_RETURN_VOID();
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

// Helper to convert the owner MCV to a default. If the input is NULL boolean false should be returned, else the owner string.
static const Variant *
manifestOwnerVar(const String *const ownerDefault)
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

    FUNCTION_AUDIT_CALLBACK();

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

        if (manifest->pub.data.blockIncr)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_BLOCK_INCR,
                jsonFromVar(VARBOOL(manifest->pub.data.blockIncr)));
        }

        if (manifest->pub.data.bundle)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_BUNDLE, jsonFromVar(VARBOOL(manifest->pub.data.bundle)));
        }

        if (manifest->pub.data.bundleRaw)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_BUNDLE_RAW,
                jsonFromVar(VARBOOL(manifest->pub.data.bundleRaw)));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_LABEL, jsonFromVar(VARSTR(manifest->pub.data.backupLabel)));

        if (manifest->pub.data.lsnStart != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP, MANIFEST_KEY_BACKUP_LSN_START,
                jsonFromVar(VARSTR(manifest->pub.data.lsnStart)));
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
            jsonFromVar(VARSTR(strNewStrId(manifest->pub.data.backupType))));
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

        // Set the option when compression is turned on. In older versions this also implied gz compression but in newer versions
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

        // Set the compression type. Older versions will ignore this and assume gz compression if the compress option is set.
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

                // Block incremental info
                if (file.blockIncrSize != 0)
                {
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BLOCK_INCR), file.blockIncrSize / BLOCK_INCR_SIZE_FACTOR);

                    if (file.blockIncrChecksumSize != BLOCK_INCR_CHECKSUM_SIZE_MIN)
                        jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BLOCK_INCR_CHECKSUM), file.blockIncrChecksumSize);

                    if (file.blockIncrMapSize != 0)
                        jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BLOCK_INCR_MAP), file.blockIncrMapSize);
                }

                // Bundle info
                if (file.bundleId != 0)
                {
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BUNDLE_ID), file.bundleId);

                    if (file.bundleOffset != 0)
                        jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_BUNDLE_OFFSET), file.bundleOffset);
                }

                // Save if the file size is not zero and the checksum exists. The checksum might not exist if this is a partial save
                // performed during a backup.
                if (file.size != 0 && file.checksumSha1 != NULL)
                {
                    jsonWriteStr(
                        jsonWriteKeyStrId(json, MANIFEST_KEY_CHECKSUM),
                        strNewEncode(encodingHex, BUF(file.checksumSha1, HASH_TYPE_SHA1_SIZE)));
                }

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

                // Save if the repo checksum is not null. The repo checksum for zero-length files may vary depending on compression
                // and encryption applied.
                if (file.checksumRepoSha1 != NULL)
                {
                    jsonWriteStr(
                        jsonWriteKeyStrId(json, MANIFEST_KEY_CHECKSUM_REPO),
                        strNewEncode(encodingHex, BUF(file.checksumRepoSha1, HASH_TYPE_SHA1_SIZE)));
                }

                if (file.reference != NULL)
                    jsonWriteStr(jsonWriteKeyStrId(json, MANIFEST_KEY_REFERENCE), file.reference);

                if (file.sizeRepo != file.size)
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_SIZE_REPO), file.sizeRepo);

                jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_SIZE), file.size);

                if (file.sizeOriginal != file.size)
                    jsonWriteUInt64(jsonWriteKeyStrId(json, MANIFEST_KEY_SIZE_ORIGINAL), file.sizeOriginal);

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

FN_EXTERN Manifest *
manifestLoadFile(
    const Storage *const storage, const String *const fileName, const CipherType cipherType, const String *const cipherPass)
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
        const char *const fileNamePath = strZ(storagePathP(storage, fileName));

        infoLoad(
            strNewFmt("unable to load backup manifest file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
            manifestLoadFileCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, data.manifest);
}

FN_EXTERN void
manifestSave(Manifest *const this, IoWrite *const write)
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
