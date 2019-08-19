/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/debug.h"
#include "common/log.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "common/type/mcv.h"
#include "info/info.h"
#include "info/infoManifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(INFO_MANIFEST_FILE_STR,                               INFO_MANIFEST_FILE);

VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_VAR,   INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR,    INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_PRIOR_VAR,           INFO_MANIFEST_KEY_BACKUP_PRIOR);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_VAR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_VAR,  INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_TYPE_VAR,            INFO_MANIFEST_KEY_BACKUP_TYPE);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK_VAR,      INFO_MANIFEST_KEY_OPT_ARCHIVE_CHECK);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY_VAR,       INFO_MANIFEST_KEY_OPT_ARCHIVE_COPY);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY_VAR,     INFO_MANIFEST_KEY_OPT_BACKUP_STANDBY);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE_VAR,      INFO_MANIFEST_KEY_OPT_CHECKSUM_PAGE);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_COMPRESS_VAR,           INFO_MANIFEST_KEY_OPT_COMPRESS);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_HARDLINK_VAR,           INFO_MANIFEST_KEY_OPT_HARDLINK);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_OPT_ONLINE_VAR,             INFO_MANIFEST_KEY_OPT_ONLINE);

STRING_STATIC(INFO_MANIFEST_TARGET_TYPE_LINK_STR,                   "link");
STRING_STATIC(INFO_MANIFEST_TARGET_TYPE_PATH_STR,                   "path");

STRING_STATIC(INFO_MANIFEST_SECTION_BACKUP_STR,                     "backup");
STRING_STATIC(INFO_MANIFEST_SECTION_BACKUP_DB_STR,                  "backup:db");
STRING_STATIC(INFO_MANIFEST_SECTION_BACKUP_OPTION_STR,              "backup:option");
STRING_STATIC(INFO_MANIFEST_SECTION_BACKUP_TARGET_STR,              "backup:target");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_STR,                "target:file");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR,        "target:file:default");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_LINK_STR,                "target:link");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR,        "target:link:default");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_STR,                "target:path");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR,        "target:path:default");

#define INFO_MANIFEST_KEY_BACKUP_LABEL                              "backup-label"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_LABEL_STR,               INFO_MANIFEST_KEY_BACKUP_LABEL);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START               "backup-timestamp-copy-start"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START                    "backup-timestamp-start"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,     INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                     "backup-timestamp-stop"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,      INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP);
#define INFO_MANIFEST_KEY_BACKUP_TYPE                               "backup-type"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TYPE_STR,                INFO_MANIFEST_KEY_BACKUP_TYPE);
#define INFO_MANIFEST_KEY_DB_CATALOG_VERSION                        "db-catalog-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR,         INFO_MANIFEST_KEY_DB_CATALOG_VERSION);
#define INFO_MANIFEST_KEY_DB_CONTROL_VERSION                        "db-control-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR,         INFO_MANIFEST_KEY_DB_CONTROL_VERSION);
#define INFO_MANIFEST_KEY_DB_ID                                     "db-id"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_ID_STR,                      INFO_MANIFEST_KEY_DB_ID);
#define INFO_MANIFEST_KEY_DB_SYSTEM_ID                              "db-system-id"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR,               INFO_MANIFEST_KEY_DB_SYSTEM_ID);
#define INFO_MANIFEST_KEY_DB_VERSION                                "db-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_VERSION_STR,                 INFO_MANIFEST_KEY_DB_VERSION);
#define INFO_MANIFEST_KEY_CHECKSUM                                  "checksum"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_VAR,           INFO_MANIFEST_KEY_CHECKSUM);
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE                             "checksum-page"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR,      INFO_MANIFEST_KEY_CHECKSUM_PAGE);
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR                       "checksum-page-error"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR);
#define INFO_MANIFEST_KEY_DESTINATION                               "destination"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_DESTINATION_VAR,        INFO_MANIFEST_KEY_DESTINATION);
#define INFO_MANIFEST_KEY_FILE                                      "file"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_FILE_VAR,               INFO_MANIFEST_KEY_FILE);
#define INFO_MANIFEST_KEY_GROUP                                     "group"
    STRING_STATIC(INFO_MANIFEST_KEY_GROUP_STR,                      INFO_MANIFEST_KEY_GROUP);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_GROUP_VAR,              INFO_MANIFEST_KEY_GROUP);
#define INFO_MANIFEST_KEY_MASTER                                    "master"
    STRING_STATIC(INFO_MANIFEST_KEY_MASTER_STR,                     INFO_MANIFEST_KEY_MASTER);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_MASTER_VAR,             INFO_MANIFEST_KEY_MASTER);
#define INFO_MANIFEST_KEY_MODE                                      "mode"
    STRING_STATIC(INFO_MANIFEST_KEY_MODE_STR,                       INFO_MANIFEST_KEY_MODE);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_MODE_VAR,               INFO_MANIFEST_KEY_MODE);

#define INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK                      "option-archive-check"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR,       INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK);
#define INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY                       "option-archive-copy"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR,        INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY);
#define INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY                     "option-backup-standby"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR,      INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY);
#define INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE                        "option-buffer-size"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,         INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE);
#define INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE                      "option-checksum-page"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,       INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE);
#define INFO_MANIFEST_KEY_OPTION_COMPRESS                           "option-compress"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_COMPRESS_STR,            INFO_MANIFEST_KEY_OPTION_COMPRESS);
#define INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL                     "option-compress-level"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,      INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL);
#define INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK             "option-compress-level-network"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK);
#define INFO_MANIFEST_KEY_OPTION_DELTA                              "option-delta"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_DELTA_STR,               INFO_MANIFEST_KEY_OPTION_DELTA);
#define INFO_MANIFEST_KEY_OPTION_HARDLINK                           "option-hardlink"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_HARDLINK_STR,            INFO_MANIFEST_KEY_OPTION_HARDLINK);
#define INFO_MANIFEST_KEY_OPTION_ONLINE                             "option-online"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_ONLINE_STR,              INFO_MANIFEST_KEY_OPTION_ONLINE);
#define INFO_MANIFEST_KEY_OPTION_PROCESS_MAX                        "option-process-max"
    STRING_STATIC(INFO_MANIFEST_KEY_OPTION_PROCESS_MAX_STR,         INFO_MANIFEST_KEY_OPTION_PROCESS_MAX);

#define INFO_MANIFEST_KEY_PATH                                      "path"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_PATH_VAR,               INFO_MANIFEST_KEY_PATH);
#define INFO_MANIFEST_KEY_SIZE                                      "size"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_SIZE_VAR,               INFO_MANIFEST_KEY_SIZE);
#define INFO_MANIFEST_KEY_SIZE_REPO                                 "size-repo"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_SIZE_REPO_VAR,          INFO_MANIFEST_KEY_SIZE_REPO);
#define INFO_MANIFEST_KEY_TIMESTAMP                                 "timestamp"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TIMESTAMP_VAR,          INFO_MANIFEST_KEY_TIMESTAMP);
#define INFO_MANIFEST_KEY_TYPE                                      "type"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TYPE_VAR,               INFO_MANIFEST_KEY_TYPE);
#define INFO_MANIFEST_KEY_USER                                      "user"
    STRING_STATIC(INFO_MANIFEST_KEY_USER_STR,                       INFO_MANIFEST_KEY_USER);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_USER_VAR,               INFO_MANIFEST_KEY_USER);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoManifest
{
    MemContext *memContext;                                         // Context that contains the InfoManifest

    Info *info;                                                     // Base info object
    StringList *ownerList;                                          // List of users/groups

    InfoManifestData data;                                          // Manifest data and options
    List *targetList;                                               // List of paths
    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
};

/***********************************************************************************************************************************
Convert the owner to a variant.  The input could be boolean false (meaning there is no owner) or a string (there is an owner).
***********************************************************************************************************************************/
static const Variant *
infoManifestOwnerDefaultGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    ASSERT(ownerDefault != NULL);

    FUNCTION_TEST_RETURN(strEq(ownerDefault, FALSE_STR) ? BOOL_FALSE_VAR : varNewStr(jsonToStr(ownerDefault)));
}

/***********************************************************************************************************************************
Add owner to the owner list if it is not there already and return the pointer.  This saves a lot of space.
***********************************************************************************************************************************/
static const String *
infoManifestOwnerCache(InfoManifest *this, const Variant *owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
        FUNCTION_TEST_PARAM(VARIANT, owner);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(owner != NULL);

    const String *result = NULL;

    if (varType(owner) == varTypeString)
    {
        // Search for the owner in the list
        for (unsigned int ownerIdx = 0; ownerIdx < strLstSize(this->ownerList); ownerIdx++)
        {
            const String *found = strLstGet(this->ownerList, ownerIdx);

            if (strEq(varStr(owner), found))
            {
                result = found;
                break;
            }
        }

        // If not found then add it
        if (result == NULL)
        {
            strLstAdd(this->ownerList, varStr(owner));
            result = strLstGet(this->ownerList, strLstSize(this->ownerList) - 1);
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Load from file
***********************************************************************************************************************************/
InfoManifest *
infoManifestNewLoad(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    InfoManifest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoManifest")
    {
        // Create object
        this = memNew(sizeof(InfoManifest));
        this->memContext = MEM_CONTEXT_NEW();

        // Create lists
        this->ownerList = strLstNew();
        this->targetList = lstNew(sizeof(InfoManifestPath));
        this->pathList = lstNew(sizeof(InfoManifestPath));
        this->fileList = lstNew(sizeof(InfoManifestFile));
        this->linkList = lstNew(sizeof(InfoManifestLink));

        // Load the manifest
        Ini *iniLocal = NULL;
        this->info = infoNewLoad(storage, fileName, cipherType, cipherPass, &iniLocal);

        // Load configuration
        // -------------------------------------------------------------------------------------------------------------------------
        this->data.backupLabel = jsonToStr(iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_LABEL_STR));
        this->data.backupTimestampCopyStart = (time_t)jsonToUInt64(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR));
        this->data.backupTimestampStart = (time_t)jsonToUInt64(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR));
        this->data.backupTimestampStop = (time_t)jsonToUInt64(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR));
        this->data.backupType = backupType(
            jsonToStr(iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TYPE_STR)));

        this->data.pgId = jsonToUInt(iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_ID_STR));
        this->data.pgVersion = pgVersionFromStr(
            jsonToStr(iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_VERSION_STR)));
        this->data.pgSystemId = jsonToUInt64(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR));
        this->data.pgControlVersion = jsonToUInt(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR));
        this->data.pgCatalogVersion = jsonToUInt(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR));

        this->data.backupOptionArchiveCheck = jsonToBool(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR));
        this->data.backupOptionArchiveCopy = jsonToBool(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR));
        this->data.backupOptionStandby = jsonToBool(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR, FALSE_STR));
        this->data.backupOptionBufferSize = (size_t)jsonToUInt64(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE_STR, STRDEF("0")));
        this->data.backupOptionChecksumPage = jsonToBool(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR, FALSE_STR));
        this->data.backupOptionCompress = jsonToBool(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_STR));
        this->data.backupOptionCompressLevel = jsonToUInt(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR, STRDEF("6")));
        this->data.backupOptionCompressLevelNetwork = jsonToUInt(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,
                STRDEF("3")));
        this->data.backupOptionDelta = jsonToBool(
            iniGetDefault(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_DELTA_STR, FALSE_STR));
        this->data.backupOptionHardLink = jsonToBool(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_HARDLINK_STR));
        this->data.backupOptionOnline = jsonToBool(
            iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ONLINE_STR));
        this->data.backupOptionProcessMax = jsonToUInt(
            iniGetDefault(
                iniLocal, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_PROCESS_MAX_STR, STRDEF("1")));

        // Load targets
        // -------------------------------------------------------------------------------------------------------------------------
        bool linkPresent = false;

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Load target list
            StringList *targetKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR);

            for (unsigned int targetKeyIdx = 0; targetKeyIdx < strLstSize(targetKeyList); targetKeyIdx++)
            {
                const String *targetName = strLstGet(targetKeyList, targetKeyIdx);
                KeyValue *targetData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR, targetName)));
                const String *targetType = varStr(kvGet(targetData, INFO_MANIFEST_KEY_TYPE_VAR));

                ASSERT(
                    strEq(targetType, INFO_MANIFEST_TARGET_TYPE_LINK_STR) || strEq(targetType, INFO_MANIFEST_TARGET_TYPE_PATH_STR));

                memContextSwitch(lstMemContext(this->targetList));

                InfoManifestTarget target =
                {
                    .name = strDup(targetName),
                    .type = strEq(targetType, INFO_MANIFEST_TARGET_TYPE_PATH_STR) ? manifestTargetTypePath : manifestTargetTypeLink,
                    .path = strDup(varStr(kvGet(targetData, INFO_MANIFEST_KEY_PATH_VAR))),
                    .file = strDup(varStr(kvGetDefault(targetData, INFO_MANIFEST_KEY_FILE_VAR, NULL))),
                };

                // Is a link present?  This is so we know links must be loaded later since they are optional.
                if (target.type == manifestTargetTypeLink)
                    linkPresent = true;

                lstAdd(this->targetList, &target);
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Load paths
        // ---------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Load path defaults
            const Variant *pathUserDefault = infoManifestOwnerDefaultGet(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR));
            const Variant *pathGroupDefault = infoManifestOwnerDefaultGet(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR));
            const String *pathModeDefault = jsonToStr(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR));

            // Load path list
            StringList *pathKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR);

            for (unsigned int pathKeyIdx = 0; pathKeyIdx < strLstSize(pathKeyList); pathKeyIdx++)
            {
                memContextSwitch(lstMemContext(this->pathList));
                InfoManifestPath path = {.name = strDup(strLstGet(pathKeyList, pathKeyIdx))};
                memContextSwitch(MEM_CONTEXT_TEMP());

                KeyValue *pathData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path.name)));

                path.user = infoManifestOwnerCache(this, kvGetDefault(pathData, INFO_MANIFEST_KEY_USER_VAR, pathUserDefault));
                path.group = infoManifestOwnerCache(this, kvGetDefault(pathData, INFO_MANIFEST_KEY_GROUP_VAR, pathGroupDefault));
                path.mode = cvtZToMode(strPtr(varStr(kvGetDefault(pathData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(pathModeDefault)))));

                lstAdd(this->pathList, &path);
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Load files
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Load file defaults
            const Variant *fileUserDefault = infoManifestOwnerDefaultGet(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR));
            const Variant *fileGroupDefault = infoManifestOwnerDefaultGet(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR));
            const String *fileModeDefault = jsonToStr(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR));
            const Variant *fileMasterDefault = jsonToVar(
                iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MASTER_STR));

            // Load file list
            StringList *fileKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_STR);

            for (unsigned int fileKeyIdx = 0; fileKeyIdx < strLstSize(fileKeyList); fileKeyIdx++)
            {
                memContextSwitch(lstMemContext(this->fileList));
                InfoManifestFile file = {.name = strDup(strLstGet(fileKeyList, fileKeyIdx))};
                memContextSwitch(MEM_CONTEXT_TEMP());

                KeyValue *fileData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_FILE_STR, file.name)));

                file.user = infoManifestOwnerCache(this, kvGetDefault(fileData, INFO_MANIFEST_KEY_USER_VAR, fileUserDefault));
                file.group = infoManifestOwnerCache(this, kvGetDefault(fileData, INFO_MANIFEST_KEY_GROUP_VAR, fileGroupDefault));
                file.mode = cvtZToMode(strPtr(varStr(kvGetDefault(fileData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(fileModeDefault)))));
                file.master = varBool(kvGetDefault(fileData, INFO_MANIFEST_KEY_MASTER_VAR, fileMasterDefault));
                file.size = varUInt64(kvGet(fileData, INFO_MANIFEST_KEY_SIZE_VAR));
                file.sizeRepo = varUInt64(kvGetDefault(fileData, INFO_MANIFEST_KEY_SIZE_REPO_VAR, VARUINT64(file.size)));
                file.timestamp = (time_t)varUInt64(kvGet(fileData, INFO_MANIFEST_KEY_TIMESTAMP_VAR));

                if (kvGetDefault(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, NULL) != NULL)
                {
                    file.checksumPage = true;

                    const Variant *checksumPageError = kvGetDefault(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, NULL);

                    if (checksumPageError != NULL)
                    {
                        memContextSwitch(lstMemContext(this->fileList));
                        file.checksumPageError = varLstDup(varVarLst(checksumPageError));
                        memContextSwitch(MEM_CONTEXT_TEMP());
                    }
                }

                if (file.size == 0)
                    memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
                else
                {
                    memcpy(
                        file.checksumSha1, strPtr(varStr(kvGet(fileData, INFO_MANIFEST_KEY_CHECKSUM_VAR))),
                        HASH_TYPE_SHA1_SIZE_HEX + 1);
                }

                lstAdd(this->fileList, &file);
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Load links
        // ---------------------------------------------------------------------------------------------------------------------
        if (linkPresent)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Load link defaults
                const Variant *linkUserDefault = infoManifestOwnerDefaultGet(
                    iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR));
                const Variant *linkGroupDefault = infoManifestOwnerDefaultGet(
                    iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR));

                // Load link list
                StringList *linkKeyList = iniSectionKeyList(iniLocal, INFO_MANIFEST_SECTION_TARGET_LINK_STR);

                for (unsigned int linkKeyIdx = 0; linkKeyIdx < strLstSize(linkKeyList); linkKeyIdx++)
                {
                    memContextSwitch(lstMemContext(this->linkList));
                    InfoManifestLink link = {.name = strDup(strLstGet(linkKeyList, linkKeyIdx))};
                    memContextSwitch(MEM_CONTEXT_TEMP());

                    KeyValue *linkData = varKv(jsonToVar(iniGet(iniLocal, INFO_MANIFEST_SECTION_TARGET_LINK_STR, link.name)));

                    memContextSwitch(lstMemContext(this->linkList));
                    link.destination = strDup(varStr(kvGet(linkData, INFO_MANIFEST_KEY_DESTINATION_VAR)));
                    memContextSwitch(MEM_CONTEXT_TEMP());

                    link.user = infoManifestOwnerCache(this, kvGetDefault(linkData, INFO_MANIFEST_KEY_USER_VAR, linkUserDefault));
                    link.group = infoManifestOwnerCache(
                        this, kvGetDefault(linkData, INFO_MANIFEST_KEY_GROUP_VAR, linkGroupDefault));

                    lstAdd(this->linkList, &link);
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
// Helper to convert the owner MCV to a default.  If the input is NULL boolean false should be returned, else the owner string.
static const Variant *
infoManifestOwnerGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(ownerDefault == NULL ? BOOL_FALSE_VAR : varNewStr(ownerDefault));
}

void
infoManifestSave(
    InfoManifest *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_MANIFEST, this);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Ini *ini = iniNew();

        // Load configuration
        // -------------------------------------------------------------------------------------------------------------------------
        iniSet(ini, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_LABEL_STR, jsonFromStr(this->data.backupLabel));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR,
            jsonFromInt64(this->data.backupTimestampCopyStart));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,
            jsonFromInt64(this->data.backupTimestampStart));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,
            jsonFromInt64(this->data.backupTimestampStop));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TYPE_STR,
            jsonFromStr(backupTypeStr(this->data.backupType)));

        iniSet(ini, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_ID_STR, jsonFromUInt(this->data.pgId));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_VERSION_STR,
            jsonFromStr(pgVersionToStr(this->data.pgVersion)));
        iniSet(ini, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR, jsonFromUInt64(this->data.pgSystemId));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR,
            jsonFromUInt(this->data.pgControlVersion));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR,
            jsonFromUInt(this->data.pgCatalogVersion));

        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR,
            jsonFromBool(this->data.backupOptionArchiveCheck));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR,
            jsonFromBool(this->data.backupOptionArchiveCopy));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR,
            jsonFromBool(this->data.backupOptionStandby));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,
            jsonFromUInt64(this->data.backupOptionBufferSize));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,
            jsonFromBool(this->data.backupOptionChecksumPage));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_STR,
            jsonFromBool(this->data.backupOptionCompress));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,
            jsonFromUInt(this->data.backupOptionCompressLevel));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,
            jsonFromUInt(this->data.backupOptionCompressLevelNetwork));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_DELTA_STR,
            jsonFromBool(this->data.backupOptionDelta));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_HARDLINK_STR,
            jsonFromBool(this->data.backupOptionHardLink));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ONLINE_STR,
            jsonFromBool(this->data.backupOptionOnline));
        iniSet(
            ini, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_PROCESS_MAX_STR,
            jsonFromUInt(this->data.backupOptionProcessMax));

        // Save targets
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Save target list
            for (unsigned int targetIdx = 0; targetIdx < lstSize(this->targetList); targetIdx++)
            {
                InfoManifestTarget *target = lstGet(this->targetList, targetIdx);
                KeyValue *targetData = kvNew();

                kvPut(
                    targetData, INFO_MANIFEST_KEY_TYPE_VAR,
                    VARSTR(
                        target->type == manifestTargetTypePath ?
                            INFO_MANIFEST_TARGET_TYPE_PATH_STR : INFO_MANIFEST_TARGET_TYPE_LINK_STR));

                kvPut(targetData, INFO_MANIFEST_KEY_PATH_VAR, VARSTR(target->path));

                if (target->file != NULL)
                    kvPut(targetData, INFO_MANIFEST_KEY_FILE_VAR, VARSTR(target->file));

                iniSet(ini, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR, target->name, jsonFromKv(targetData, 0));
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Save paths
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Save default path values
            MostCommonValue *userMcv = mcvNew();
            MostCommonValue *groupMcv = mcvNew();
            MostCommonValue *modeMcv = mcvNew();

            for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
            {
                InfoManifestPath *path = lstGet(this->pathList, pathIdx);

                mcvUpdate(userMcv, VARSTR(path->user));
                mcvUpdate(groupMcv, VARSTR(path->group));
                mcvUpdate(modeMcv, VARUINT(path->mode));
            }

            const Variant *userDefault = infoManifestOwnerGet(varStr(mcvResult(userMcv)));
            const Variant *groupDefault = infoManifestOwnerGet(varStr(mcvResult(groupMcv)));
            const Variant *modeDefault = mcvResult(modeMcv);

            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, jsonFromVar(userDefault, 0));
            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR, jsonFromVar(groupDefault, 0));
            iniSet(
                ini, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR,
                jsonFromStr(strNewFmt("%04o", varUInt(modeDefault))));

            // Save path list
            for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
            {
                InfoManifestPath *path = lstGet(this->pathList, pathIdx);
                KeyValue *pathData = kvNew();

                if (!varEq(infoManifestOwnerGet(path->user), userDefault))
                    kvPut(pathData, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(path->user));

                if (!varEq(infoManifestOwnerGet(path->group), groupDefault))
                    kvPut(pathData, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(path->group));

                if (!varEq(VARUINT(path->mode), modeDefault))
                    kvPut(pathData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", path->mode)));

                iniSet(ini, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path->name, jsonFromKv(pathData, 0));
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Save files
        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Save default file values
            MostCommonValue *userMcv = mcvNew();
            MostCommonValue *groupMcv = mcvNew();
            MostCommonValue *modeMcv = mcvNew();
            MostCommonValue *masterMcv = mcvNew();

            for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
            {
                InfoManifestFile *file = lstGet(this->fileList, fileIdx);

                mcvUpdate(userMcv, VARSTR(file->user));
                mcvUpdate(groupMcv, VARSTR(file->group));
                mcvUpdate(modeMcv, VARUINT(file->mode));
                mcvUpdate(masterMcv, VARBOOL(file->master));
            }

            const Variant *userDefault = infoManifestOwnerGet(varStr(mcvResult(userMcv)));
            const Variant *groupDefault = infoManifestOwnerGet(varStr(mcvResult(groupMcv)));
            const Variant *modeDefault = mcvResult(modeMcv);
            const Variant *masterDefault = mcvResult(masterMcv);

            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, jsonFromVar(userDefault, 0));
            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR, jsonFromVar(groupDefault, 0));
            iniSet(
                ini, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR,
                jsonFromStr(strNewFmt("%04o", varUInt(modeDefault))));
            iniSet(ini, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MASTER_STR, jsonFromVar(masterDefault, 0));

            // Save file list
            for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
            {
                InfoManifestFile *file = lstGet(this->fileList, fileIdx);
                KeyValue *fileData = kvNew();

                if (!varEq(infoManifestOwnerGet(file->user), userDefault))
                    kvPut(fileData, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(file->user));

                if (!varEq(infoManifestOwnerGet(file->group), groupDefault))
                    kvPut(fileData, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(file->group));

                if (!varEq(VARUINT(file->mode), modeDefault))
                    kvPut(fileData, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", file->mode)));

                if (!varEq(VARBOOL(file->master), masterDefault))
                    kvPut(fileData, INFO_MANIFEST_KEY_MASTER_VAR, VARBOOL(file->master));

                kvPut(fileData, INFO_MANIFEST_KEY_SIZE_VAR, varNewUInt64(file->size));

                if (file->sizeRepo != file->size)
                    kvPut(fileData, INFO_MANIFEST_KEY_SIZE_REPO_VAR, varNewUInt64(file->sizeRepo));

                kvPut(fileData, INFO_MANIFEST_KEY_TIMESTAMP_VAR, varNewUInt64((uint64_t)file->timestamp));

                if (file->checksumPage)
                {
                    if (file->checksumPageError == NULL)
                        kvPut(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, BOOL_TRUE_VAR);
                    else
                    {
                        kvPut(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, BOOL_FALSE_VAR);
                        kvPut(fileData, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, varNewVarLst(file->checksumPageError));
                    }
                }

                if (file->size != 0)
                    kvPut(fileData, INFO_MANIFEST_KEY_CHECKSUM_VAR, VARSTRZ(file->checksumSha1));

                iniSet(ini, INFO_MANIFEST_SECTION_TARGET_FILE_STR, file->name, jsonFromKv(fileData, 0));
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Save links
        // -------------------------------------------------------------------------------------------------------------------------
        if (lstSize(this->linkList) > 0)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                // Save default link values
                MostCommonValue *userMcv = mcvNew();
                MostCommonValue *groupMcv = mcvNew();

                for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
                {
                    InfoManifestLink *link = lstGet(this->linkList, linkIdx);

                    mcvUpdate(userMcv, VARSTR(link->user));
                    mcvUpdate(groupMcv, VARSTR(link->group));
                }

                const Variant *userDefault = infoManifestOwnerGet(varStr(mcvResult(userMcv)));
                const Variant *groupDefault = infoManifestOwnerGet(varStr(mcvResult(groupMcv)));

                iniSet(ini, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR, jsonFromVar(userDefault, 0));
                iniSet(
                    ini, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR, jsonFromVar(groupDefault, 0));

                // Save link list
                for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
                {
                    InfoManifestLink *link = lstGet(this->linkList, linkIdx);
                    KeyValue *linkData = kvNew();

                    if (!varEq(infoManifestOwnerGet(link->user), userDefault))
                        kvPut(linkData, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(link->user));

                    if (!varEq(infoManifestOwnerGet(link->group), groupDefault))
                        kvPut(linkData, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(link->group));

                    kvPut(linkData, INFO_MANIFEST_KEY_DESTINATION_VAR, VARSTR(link->destination));

                    iniSet(ini, INFO_MANIFEST_SECTION_TARGET_LINK_STR, link->name, jsonFromKv(linkData, 0));
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        infoSave(this->info, ini, storage, fileName, cipherType, cipherPass);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return manifest configuration and options
***********************************************************************************************************************************/
const InfoManifestData *
infoManifestData(const InfoManifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(&this->data);
}
