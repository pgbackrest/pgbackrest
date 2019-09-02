/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>

#include "common/crypto/cipherBlock.h"
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
STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR,           INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START);
VARIANT_STRDEF_EXTERN(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_VAR,    INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP);
STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR,            INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP);
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

STRING_STATIC(INFO_MANIFEST_SECTION_DB_STR,                         "db");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_STR,                "target:file");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR,        "target:file:default");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_LINK_STR,                "target:link");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR,        "target:link:default");

STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_STR,                "target:path");
STRING_STATIC(INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR,        "target:path:default");

#define INFO_MANIFEST_KEY_BACKUP_LABEL                              "backup-label"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_LABEL_STR,               INFO_MANIFEST_KEY_BACKUP_LABEL);
#define INFO_MANIFEST_KEY_BACKUP_LSN_START                          "backup-lsn-start"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_LSN_START_STR,           INFO_MANIFEST_KEY_BACKUP_LSN_START);
#define INFO_MANIFEST_KEY_BACKUP_LSN_STOP                           "backup-lsn-stop"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_LSN_STOP_STR,            INFO_MANIFEST_KEY_BACKUP_LSN_STOP);
#define INFO_MANIFEST_KEY_BACKUP_PRIOR                              "backup-prior"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_PRIOR_STR,               INFO_MANIFEST_KEY_BACKUP_PRIOR);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START               "backup-timestamp-copy-start"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START                    "backup-timestamp-start"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,     INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START);
#define INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                     "backup-timestamp-stop"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,      INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP);
#define INFO_MANIFEST_KEY_BACKUP_TYPE                               "backup-type"
    STRING_STATIC(INFO_MANIFEST_KEY_BACKUP_TYPE_STR,                INFO_MANIFEST_KEY_BACKUP_TYPE);
#define INFO_MANIFEST_KEY_CHECKSUM                                  "checksum"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_VAR,           INFO_MANIFEST_KEY_CHECKSUM);
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE                             "checksum-page"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR,      INFO_MANIFEST_KEY_CHECKSUM_PAGE);
#define INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR                       "checksum-page-error"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR);
#define INFO_MANIFEST_KEY_DB_CATALOG_VERSION                        "db-catalog-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR,         INFO_MANIFEST_KEY_DB_CATALOG_VERSION);
#define INFO_MANIFEST_KEY_DB_CONTROL_VERSION                        "db-control-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR,         INFO_MANIFEST_KEY_DB_CONTROL_VERSION);
#define INFO_MANIFEST_KEY_DB_ID                                     "db-id"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_ID_STR,                      INFO_MANIFEST_KEY_DB_ID);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_DB_ID_VAR,              INFO_MANIFEST_KEY_DB_ID);
#define INFO_MANIFEST_KEY_DB_LAST_SYSTEM_ID                         "db-last-system-id"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR,  INFO_MANIFEST_KEY_DB_LAST_SYSTEM_ID);
#define INFO_MANIFEST_KEY_DB_SYSTEM_ID                              "db-system-id"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR,               INFO_MANIFEST_KEY_DB_SYSTEM_ID);
#define INFO_MANIFEST_KEY_DB_VERSION                                "db-version"
    STRING_STATIC(INFO_MANIFEST_KEY_DB_VERSION_STR,                 INFO_MANIFEST_KEY_DB_VERSION);
#define INFO_MANIFEST_KEY_DESTINATION                               "destination"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_DESTINATION_VAR,        INFO_MANIFEST_KEY_DESTINATION);
#define INFO_MANIFEST_KEY_FILE                                      "file"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_FILE_VAR,               INFO_MANIFEST_KEY_FILE);
#define INFO_MANIFEST_KEY_GROUP                                     "group"
    STRING_STATIC(INFO_MANIFEST_KEY_GROUP_STR,                      INFO_MANIFEST_KEY_GROUP);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_GROUP_VAR,              INFO_MANIFEST_KEY_GROUP);
#define INFO_MANIFEST_KEY_PRIMARY                                   "ma" "st" "er"
    STRING_STATIC(INFO_MANIFEST_KEY_PRIMARY_STR,                    INFO_MANIFEST_KEY_PRIMARY);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_PRIMARY_VAR,            INFO_MANIFEST_KEY_PRIMARY);
#define INFO_MANIFEST_KEY_MODE                                      "mode"
    STRING_STATIC(INFO_MANIFEST_KEY_MODE_STR,                       INFO_MANIFEST_KEY_MODE);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_MODE_VAR,               INFO_MANIFEST_KEY_MODE);
#define INFO_MANIFEST_KEY_PATH                                      "path"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_PATH_VAR,               INFO_MANIFEST_KEY_PATH);
#define INFO_MANIFEST_KEY_REFERENCE                                 "reference"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_REFERENCE_VAR,          INFO_MANIFEST_KEY_REFERENCE);
#define INFO_MANIFEST_KEY_SIZE                                      "size"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_SIZE_VAR,               INFO_MANIFEST_KEY_SIZE);
#define INFO_MANIFEST_KEY_SIZE_REPO                                 "repo-size"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_SIZE_REPO_VAR,          INFO_MANIFEST_KEY_SIZE_REPO);
#define INFO_MANIFEST_KEY_TABLESPACE_ID                             "tablespace-id"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TABLESPACE_ID_VAR,      INFO_MANIFEST_KEY_TABLESPACE_ID);
#define INFO_MANIFEST_KEY_TABLESPACE_NAME                           "tablespace-name"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TABLESPACE_NAME_VAR,    INFO_MANIFEST_KEY_TABLESPACE_NAME);
#define INFO_MANIFEST_KEY_TIMESTAMP                                 "timestamp"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TIMESTAMP_VAR,          INFO_MANIFEST_KEY_TIMESTAMP);
#define INFO_MANIFEST_KEY_TYPE                                      "type"
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_TYPE_VAR,               INFO_MANIFEST_KEY_TYPE);
#define INFO_MANIFEST_KEY_USER                                      "user"
    STRING_STATIC(INFO_MANIFEST_KEY_USER_STR,                       INFO_MANIFEST_KEY_USER);
    VARIANT_STRDEF_STATIC(INFO_MANIFEST_KEY_USER_VAR,               INFO_MANIFEST_KEY_USER);

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

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoManifest
{
    MemContext *memContext;                                         // Context that contains the InfoManifest

    Info *info;                                                     // Base info object
    StringList *ownerList;                                          // List of users/groups
    StringList *referenceList;                                      // List of file references

    InfoManifestData data;                                          // Manifest data and options
    List *targetList;                                               // List of paths
    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
    List *dbList;                                                   // List of databases
};

/***********************************************************************************************************************************
Load manifest
***********************************************************************************************************************************/
// Keep track of which values were found during load and which need to be loaded from defaults. There is no point in having
// multiple structs since most of the fields are the same and the size shouldn't be more than 4/8 bytes.
typedef struct InfoManifestLoadFound
{
    bool group:1;
    bool mode:1;
    bool primary:1;
    bool user:1;
} InfoManifestLoadFound;

typedef struct InfoManifestLoadData
{
    MemContext *memContext;                                         // Mem context for data needed only during load
    InfoManifest *infoManifest;                                     // Manifest info

    List *fileFoundList;                                            // Values found in files
    const Variant *fileGroupDefault;                                // File default group
    mode_t fileModeDefault;                                         // File default mode
    bool filePrimaryDefault;                                        // File default primary
    const Variant *fileUserDefault;                                 // File default user

    List *linkFoundList;                                            // Values found in links
    const Variant *linkGroupDefault;                                // Link default group
    const Variant *linkUserDefault;                                 // Link default user

    List *pathFoundList;                                            // Values found in paths
    const Variant *pathGroupDefault;                                // Path default group
    mode_t pathModeDefault;                                         // Path default mode
    const Variant *pathUserDefault;                                 // Path default user
} InfoManifestLoadData;

// Helper to convert owner to a variant.  Input could be boolean false (meaning there is no owner) or a string (there is an owner).
static const Variant *
infoManifestOwnerDefaultGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    ASSERT(ownerDefault != NULL);

    FUNCTION_TEST_RETURN(strEq(ownerDefault, FALSE_STR) ? BOOL_FALSE_VAR : varNewStr(jsonToStr(ownerDefault)));
}

// Helper to add owner to the owner list if it is not there already and return the pointer.  This saves a lot of space.
static const String *
infoManifestOwnerCache(InfoManifest *this, const Variant *owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
        FUNCTION_TEST_PARAM(VARIANT, owner);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

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

static void
infoManifestLoadCallback(void *callbackData, const String *section, const String *key, const String *value)
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
    ASSERT(value != NULL);

    InfoManifestLoadData *loadData = (InfoManifestLoadData *)callbackData;
    InfoManifest *infoManifest = loadData->infoManifest;


    // -----------------------------------------------------------------------------------------------------------------------------
    if (strEq(section, INFO_MANIFEST_SECTION_TARGET_FILE_STR))
    {
        KeyValue *fileKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(infoManifest->fileList))
        {
            InfoManifestLoadFound valueFound = {0};

            InfoManifestFile file =
            {
                .name = strDup(key),
                .size = varUInt64(kvGet(fileKv, INFO_MANIFEST_KEY_SIZE_VAR)),
                .timestamp = (time_t)varUInt64(kvGet(fileKv, INFO_MANIFEST_KEY_TIMESTAMP_VAR)),
            };

            file.sizeRepo = varUInt64(kvGetDefault(fileKv, INFO_MANIFEST_KEY_SIZE_REPO_VAR, VARUINT64(file.size)));

            if (file.size == 0)
                memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
            else
            {
                memcpy(
                    file.checksumSha1, strPtr(varStr(kvGet(fileKv, INFO_MANIFEST_KEY_CHECKSUM_VAR))), HASH_TYPE_SHA1_SIZE_HEX + 1);
            }

            const Variant *checksumPage = kvGetDefault(fileKv, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, NULL);

            if (checksumPage != NULL)
            {
                file.checksumPage = true;
                file.checksumPageError = varBool(checksumPage);

                const Variant *checksumPageErrorList = kvGetDefault(fileKv, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, NULL);

                if (checksumPageErrorList != NULL)
                    file.checksumPageErrorList = varLstDup(varVarLst(checksumPageErrorList));
            }

            if (kvKeyExists(fileKv, INFO_MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                file.group = infoManifestOwnerCache(infoManifest, kvGet(fileKv, INFO_MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(fileKv, INFO_MANIFEST_KEY_MODE_VAR))
            {
                valueFound.mode = true;
                file.mode = cvtZToMode(strPtr(varStr(kvGet(fileKv, INFO_MANIFEST_KEY_MODE_VAR))));
            }

            if (kvKeyExists(fileKv, INFO_MANIFEST_KEY_PRIMARY_VAR))
            {
                valueFound.primary = true;
                file.primary = varBool(kvGet(fileKv, INFO_MANIFEST_KEY_PRIMARY_VAR));
            }

            const Variant *reference = kvGetDefault(fileKv, INFO_MANIFEST_KEY_REFERENCE_VAR, NULL);

            if (reference != NULL)
            {
                // Search for the reference in the list
                for (unsigned int referenceIdx = 0; referenceIdx < strLstSize(infoManifest->referenceList); referenceIdx++)
                {
                    const String *found = strLstGet(infoManifest->referenceList, referenceIdx);

                    if (strEq(varStr(reference), found))
                    {
                        file.reference = found;
                        break;
                    }
                }

                // If not found then add it
                if (file.reference == NULL)
                {
                    strLstAdd(infoManifest->referenceList, varStr(reference));
                    file.reference = strLstGet(infoManifest->referenceList, strLstSize(infoManifest->referenceList) - 1);
                }
            }

            if (kvKeyExists(fileKv, INFO_MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                file.user = infoManifestOwnerCache(infoManifest, kvGet(fileKv, INFO_MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->fileFoundList, &valueFound);
            lstAdd(infoManifest->fileList, &file);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_TARGET_PATH_STR))
    {
        KeyValue *pathKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(infoManifest->pathList))
        {
            InfoManifestLoadFound valueFound = {0};

            InfoManifestPath path =
            {
                .name = strDup(key),
            };

            if (kvKeyExists(pathKv, INFO_MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                path.group = infoManifestOwnerCache(infoManifest, kvGet(pathKv, INFO_MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(pathKv, INFO_MANIFEST_KEY_MODE_VAR))
            {
                valueFound.mode = true;
                path.mode = cvtZToMode(strPtr(varStr(kvGet(pathKv, INFO_MANIFEST_KEY_MODE_VAR))));
            }

            if (kvKeyExists(pathKv, INFO_MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                path.user = infoManifestOwnerCache(infoManifest, kvGet(pathKv, INFO_MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->pathFoundList, &valueFound);
            lstAdd(infoManifest->pathList, &path);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_TARGET_LINK_STR))
    {
        KeyValue *linkKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(infoManifest->linkList))
        {
            InfoManifestLoadFound valueFound = {0};

            InfoManifestLink link =
            {
                .name = strDup(key),
                .destination = strDup(varStr(kvGet(linkKv, INFO_MANIFEST_KEY_DESTINATION_VAR))),
            };

            if (kvKeyExists(linkKv, INFO_MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                link.group = infoManifestOwnerCache(infoManifest, kvGet(linkKv, INFO_MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(linkKv, INFO_MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                link.user = infoManifestOwnerCache(infoManifest, kvGet(linkKv, INFO_MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->linkFoundList, &valueFound);
            lstAdd(infoManifest->linkList, &link);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, INFO_MANIFEST_KEY_GROUP_STR))
                loadData->fileGroupDefault = infoManifestOwnerDefaultGet(value);
            else if (strEq(key, INFO_MANIFEST_KEY_MODE_STR))
                loadData->fileModeDefault = cvtZToMode(strPtr(jsonToStr(value)));
            else if (strEq(key, INFO_MANIFEST_KEY_PRIMARY_STR))
                loadData->filePrimaryDefault = jsonToBool(value);
            else if (strEq(key, INFO_MANIFEST_KEY_USER_STR))
                loadData->fileUserDefault = infoManifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, INFO_MANIFEST_KEY_GROUP_STR))
                loadData->pathGroupDefault = infoManifestOwnerDefaultGet(value);
            else if (strEq(key, INFO_MANIFEST_KEY_MODE_STR))
                loadData->pathModeDefault = cvtZToMode(strPtr(jsonToStr(value)));
            else if (strEq(key, INFO_MANIFEST_KEY_USER_STR))
                loadData->pathUserDefault = infoManifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, INFO_MANIFEST_KEY_GROUP_STR))
                loadData->linkGroupDefault = infoManifestOwnerDefaultGet(value);
            else if (strEq(key, INFO_MANIFEST_KEY_USER_STR))
                loadData->linkUserDefault = infoManifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR))
    {
        KeyValue *targetKv = varKv(jsonToVar(value));
        const String *targetType = varStr(kvGet(targetKv, INFO_MANIFEST_KEY_TYPE_VAR));

        ASSERT(strEq(targetType, INFO_MANIFEST_TARGET_TYPE_LINK_STR) || strEq(targetType, INFO_MANIFEST_TARGET_TYPE_PATH_STR));

        MEM_CONTEXT_BEGIN(lstMemContext(infoManifest->targetList))
        {
            InfoManifestTarget target =
            {
                .name = strDup(key),
                .file = strDup(varStr(kvGetDefault(targetKv, INFO_MANIFEST_KEY_FILE_VAR, NULL))),
                .path = strDup(varStr(kvGet(targetKv, INFO_MANIFEST_KEY_PATH_VAR))),
                .tablespaceId =
                    cvtZToUInt(strPtr(varStr(kvGetDefault(targetKv, INFO_MANIFEST_KEY_TABLESPACE_ID_VAR, VARSTRDEF("0"))))),
                .tablespaceName = strDup(varStr(kvGetDefault(targetKv, INFO_MANIFEST_KEY_TABLESPACE_NAME_VAR, NULL))),
                .type = strEq(targetType, INFO_MANIFEST_TARGET_TYPE_PATH_STR) ? manifestTargetTypePath : manifestTargetTypeLink,
            };

            lstAdd(infoManifest->targetList, &target);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_DB_STR))
    {
        KeyValue *dbKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(infoManifest->dbList))
        {
            InfoManifestDb db =
            {
                .name = strDup(key),
                .id = varUIntForce(kvGet(dbKv, INFO_MANIFEST_KEY_DB_ID_VAR)),
                .lastSystemId = varUIntForce(kvGet(dbKv, INFO_MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR)),
            };

            lstAdd(infoManifest->dbList, &db);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_BACKUP_STR))
    {
        MEM_CONTEXT_BEGIN(infoManifest->memContext)
        {
            if (strEq(key, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR))
                infoManifest->data.archiveStart = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR))
                infoManifest->data.archiveStop = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_LABEL_STR))
                infoManifest->data.backupLabel = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_LSN_START_STR))
                infoManifest->data.lsnStart = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_LSN_STOP_STR))
                infoManifest->data.lsnStop = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_PRIOR_STR))
                infoManifest->data.backupLabelPrior = jsonToStr(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR))
                infoManifest->data.backupTimestampCopyStart = (time_t)jsonToUInt64(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR))
                infoManifest->data.backupTimestampStart = (time_t)jsonToUInt64(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR))
                infoManifest->data.backupTimestampStop = (time_t)jsonToUInt64(value);
            else if (strEq(key, INFO_MANIFEST_KEY_BACKUP_TYPE_STR))
                infoManifest->data.backupType = backupType(jsonToStr(value));
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_BACKUP_DB_STR))
    {
        if (strEq(key, INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR))
            infoManifest->data.pgCatalogVersion = jsonToUInt(value);
        else if (strEq(key, INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR))
            infoManifest->data.pgControlVersion = jsonToUInt(value);
        else if (strEq(key, INFO_MANIFEST_KEY_DB_ID_STR))
            infoManifest->data.pgId = jsonToUInt(value);
        else if (strEq(key, INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR))
            infoManifest->data.pgSystemId = jsonToUInt64(value);
        else if (strEq(key, INFO_MANIFEST_KEY_DB_VERSION_STR))
            infoManifest->data.pgVersion = pgVersionFromStr(jsonToStr(value));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR))
    {
        MEM_CONTEXT_BEGIN(infoManifest->memContext)
        {
            // Required options
            if (strEq(key, INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR))
                infoManifest->data.backupOptionArchiveCheck = jsonToBool(value);
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR))
                infoManifest->data.backupOptionArchiveCopy = jsonToBool(value);
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_COMPRESS_STR))
                infoManifest->data.backupOptionCompress = jsonToBool(value);
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_HARDLINK_STR))
                infoManifest->data.backupOptionHardLink = jsonToBool(value);
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_ONLINE_STR))
                infoManifest->data.backupOptionOnline = jsonToBool(value);

            // Options that were added after v1.00 and may not be present in every manifest
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR))
                infoManifest->data.backupOptionStandby = varNewBool(jsonToBool(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE_STR))
                infoManifest->data.backupOptionBufferSize = varNewUInt(jsonToUInt(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR))
                infoManifest->data.backupOptionChecksumPage = varNewBool(jsonToBool(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR))
                infoManifest->data.backupOptionCompressLevel = varNewUInt(jsonToUInt(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR))
                infoManifest->data.backupOptionCompressLevelNetwork = varNewUInt(jsonToUInt(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_DELTA_STR))
                infoManifest->data.backupOptionDelta = varNewBool(jsonToBool(value));
            else if (strEq(key, INFO_MANIFEST_KEY_OPTION_PROCESS_MAX_STR))
                infoManifest->data.backupOptionProcessMax = varNewUInt(jsonToUInt(value));
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

InfoManifest *
infoManifestNewLoad(IoRead *read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    InfoManifest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoManifest")
    {
        // Create object
        this = memNew(sizeof(InfoManifest));
        this->memContext = MEM_CONTEXT_NEW();

        // Create lists
        this->dbList = lstNew(sizeof(InfoManifestDb));
        this->fileList = lstNew(sizeof(InfoManifestFile));
        this->linkList = lstNew(sizeof(InfoManifestLink));
        this->pathList = lstNew(sizeof(InfoManifestPath));
        this->ownerList = strLstNew();
        this->referenceList = strLstNew();
        this->targetList = lstNew(sizeof(InfoManifestTarget));

        // Load the manifest
        InfoManifestLoadData loadData =
        {
            .memContext = memContextNew("load"),
            .infoManifest = this,
        };

        MEM_CONTEXT_BEGIN(loadData.memContext)
        {
            loadData.fileFoundList = lstNew(sizeof(InfoManifestLoadFound));
            loadData.linkFoundList = lstNew(sizeof(InfoManifestLoadFound));
            loadData.pathFoundList = lstNew(sizeof(InfoManifestLoadFound));
        }
        MEM_CONTEXT_END();

        this->info = infoNewLoad(read, infoManifestLoadCallback, &loadData);

        // Process file defaults
        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
        {
            InfoManifestFile *file = lstGet(this->fileList, fileIdx);
            InfoManifestLoadFound *found = lstGet(loadData.fileFoundList, fileIdx);

            if (!found->group)
                file->group = infoManifestOwnerCache(this, loadData.fileGroupDefault);

            if (!found->mode)
                file->mode = loadData.fileModeDefault;

            if (!found->primary)
                file->primary = loadData.filePrimaryDefault;

            if (!found->user)
                file->user = infoManifestOwnerCache(this, loadData.fileUserDefault);
        }

        // Process link defaults
        for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
        {
            InfoManifestLink *link = lstGet(this->linkList, linkIdx);
            InfoManifestLoadFound *found = lstGet(loadData.linkFoundList, linkIdx);

            if (!found->group)
                link->group = infoManifestOwnerCache(this, loadData.linkGroupDefault);

            if (!found->user)
                link->user = infoManifestOwnerCache(this, loadData.linkUserDefault);
        }

        // Process path defaults
        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            InfoManifestPath *path = lstGet(this->pathList, pathIdx);
            InfoManifestLoadFound *found = lstGet(loadData.pathFoundList, pathIdx);

            if (!found->group)
                path->group = infoManifestOwnerCache(this, loadData.pathGroupDefault);

            if (!found->mode)
                path->mode = loadData.pathModeDefault;

            if (!found->user)
                path->user = infoManifestOwnerCache(this, loadData.pathUserDefault);
        }

        memContextFree(loadData.memContext);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
typedef struct InfoManifestSaveData
{
    InfoManifest *infoManifest;                                     // InfoManifest object to be saved

    const Variant *fileGroupDefault;                                // File default group
    mode_t fileModeDefault;                                         // File default mode
    bool filePrimaryDefault;                                        // File default primary
    const Variant *fileUserDefault;                                 // File default user

    const Variant *linkGroupDefault;                                // Link default group
    const Variant *linkUserDefault;                                 // Link default user

    const Variant *pathGroupDefault;                                // Path default group
    mode_t pathModeDefault;                                         // Path default mode
    const Variant *pathUserDefault;                                 // Path default user
} InfoManifestSaveData;

// Helper to convert the owner MCV to a default.  If the input is NULL boolean false should be returned, else the owner string.
static const Variant *
infoManifestOwnerGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(ownerDefault == NULL ? BOOL_FALSE_VAR : varNewStr(ownerDefault));
}

static void
infoManifestSaveCallback(void *callbackData, const String *sectionNext, InfoSave *infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(infoSaveData != NULL);

    InfoManifestSaveData *saveData = (InfoManifestSaveData *)callbackData;
    InfoManifest *infoManifest = saveData->infoManifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, sectionNext))
    {
        if (infoManifest->data.archiveStart != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_START_STR,
                jsonFromStr(infoManifest->data.archiveStart));
        }

        if (infoManifest->data.archiveStop != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR,
                jsonFromStr(infoManifest->data.archiveStop));
        }

        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_LABEL_STR,
            jsonFromStr(infoManifest->data.backupLabel));

        if (infoManifest->data.lsnStart != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_LSN_START_STR,
                jsonFromStr(infoManifest->data.lsnStart));
        }

        if (infoManifest->data.lsnStop != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_LSN_STOP_STR,
                jsonFromStr(infoManifest->data.lsnStop));
        }

        if (infoManifest->data.backupLabelPrior != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_PRIOR_STR,
                jsonFromStr(infoManifest->data.backupLabelPrior));
        }

        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR,
            jsonFromInt64(infoManifest->data.backupTimestampCopyStart));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,
            jsonFromInt64(infoManifest->data.backupTimestampStart));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,
            jsonFromInt64(infoManifest->data.backupTimestampStop));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_STR, INFO_MANIFEST_KEY_BACKUP_TYPE_STR,
            jsonFromStr(backupTypeStr(infoManifest->data.backupType)));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CATALOG_VERSION_STR,
            jsonFromUInt(infoManifest->data.pgCatalogVersion));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_CONTROL_VERSION_STR,
            jsonFromUInt(infoManifest->data.pgControlVersion));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_ID_STR, jsonFromUInt(infoManifest->data.pgId));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_SYSTEM_ID_STR,
            jsonFromUInt64(infoManifest->data.pgSystemId));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_DB_STR, INFO_MANIFEST_KEY_DB_VERSION_STR,
            jsonFromStr(pgVersionToStr(infoManifest->data.pgVersion)));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR,
            jsonFromBool(infoManifest->data.backupOptionArchiveCheck));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR,
            jsonFromBool(infoManifest->data.backupOptionArchiveCopy));

        if (infoManifest->data.backupOptionStandby != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR,
                jsonFromVar(infoManifest->data.backupOptionStandby, 0));
        }

        if (infoManifest->data.backupOptionBufferSize != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,
                jsonFromVar(infoManifest->data.backupOptionBufferSize, 0));
        }

        if (infoManifest->data.backupOptionChecksumPage != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,
                jsonFromVar(infoManifest->data.backupOptionChecksumPage, 0));
        }

        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_STR,
            jsonFromBool(infoManifest->data.backupOptionCompress));

        if (infoManifest->data.backupOptionCompressLevel != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,
                jsonFromVar(infoManifest->data.backupOptionCompressLevel, 0));
        }

        if (infoManifest->data.backupOptionCompressLevelNetwork != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,
                jsonFromVar(infoManifest->data.backupOptionCompressLevelNetwork, 0));
        }

        if (infoManifest->data.backupOptionDelta != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_DELTA_STR,
                jsonFromVar(infoManifest->data.backupOptionDelta, 0));
        }

        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_HARDLINK_STR,
            jsonFromBool(infoManifest->data.backupOptionHardLink));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_ONLINE_STR,
            jsonFromBool(infoManifest->data.backupOptionOnline));

        if (infoManifest->data.backupOptionProcessMax != NULL)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_BACKUP_OPTION_STR, INFO_MANIFEST_KEY_OPTION_PROCESS_MAX_STR,
                jsonFromVar(infoManifest->data.backupOptionProcessMax, 0));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int targetIdx = 0; targetIdx < lstSize(infoManifest->targetList); targetIdx++)
            {
                InfoManifestTarget *target = lstGet(infoManifest->targetList, targetIdx);
                KeyValue *targetKv = kvNew();

                if (target->file != NULL)
                    kvPut(targetKv, INFO_MANIFEST_KEY_FILE_VAR, VARSTR(target->file));

                kvPut(targetKv, INFO_MANIFEST_KEY_PATH_VAR, VARSTR(target->path));

                if (target->tablespaceId != 0)
                    kvPut(targetKv, INFO_MANIFEST_KEY_TABLESPACE_ID_VAR, VARSTR(strNewFmt("%u", target->tablespaceId)));

                if (target->tablespaceName != NULL)
                    kvPut(targetKv, INFO_MANIFEST_KEY_TABLESPACE_NAME_VAR, VARSTR(target->tablespaceName));

                kvPut(
                    targetKv, INFO_MANIFEST_KEY_TYPE_VAR,
                    VARSTR(
                        target->type == manifestTargetTypePath ?
                            INFO_MANIFEST_TARGET_TYPE_PATH_STR : INFO_MANIFEST_TARGET_TYPE_LINK_STR));

                infoSaveValue(infoSaveData, INFO_MANIFEST_SECTION_BACKUP_TARGET_STR, target->name, jsonFromKv(targetKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_DB_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int dbIdx = 0; dbIdx < lstSize(infoManifest->dbList); dbIdx++)
            {
                InfoManifestDb *db = lstGet(infoManifest->dbList, dbIdx);
                KeyValue *dbKv = kvNew();

                kvPut(dbKv, INFO_MANIFEST_KEY_DB_ID_VAR, VARUINT(db->id));
                kvPut(dbKv, INFO_MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR, VARUINT(db->lastSystemId));

                infoSaveValue(infoSaveData, INFO_MANIFEST_SECTION_DB_STR, db->name, jsonFromKv(dbKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int fileIdx = 0; fileIdx < lstSize(infoManifest->fileList); fileIdx++)
            {
                InfoManifestFile *file = lstGet(infoManifest->fileList, fileIdx);
                KeyValue *fileKv = kvNew();

                if (file->size != 0)
                    kvPut(fileKv, INFO_MANIFEST_KEY_CHECKSUM_VAR, VARSTRZ(file->checksumSha1));

                if (file->checksumPage)
                {
                    kvPut(fileKv, INFO_MANIFEST_KEY_CHECKSUM_PAGE_VAR, VARBOOL(file->checksumPageError));

                    if (file->checksumPageErrorList != NULL)
                        kvPut(fileKv, INFO_MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, varNewVarLst(file->checksumPageErrorList));
                }

                if (!varEq(infoManifestOwnerGet(file->group), saveData->fileGroupDefault))
                    kvPut(fileKv, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(file->group));

                if (file->primary != saveData->filePrimaryDefault)
                    kvPut(fileKv, INFO_MANIFEST_KEY_PRIMARY_VAR, VARBOOL(file->primary));

                if (file->mode != saveData->fileModeDefault)
                    kvPut(fileKv, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", file->mode)));

                if (file->reference != NULL)
                    kvPut(fileKv, INFO_MANIFEST_KEY_REFERENCE_VAR, VARSTR(file->reference));

                if (file->sizeRepo != file->size)
                    kvPut(fileKv, INFO_MANIFEST_KEY_SIZE_REPO_VAR, varNewUInt64(file->sizeRepo));

                kvPut(fileKv, INFO_MANIFEST_KEY_SIZE_VAR, varNewUInt64(file->size));

                kvPut(fileKv, INFO_MANIFEST_KEY_TIMESTAMP_VAR, varNewUInt64((uint64_t)file->timestamp));

                if (!varEq(infoManifestOwnerGet(file->user), saveData->fileUserDefault))
                    kvPut(fileKv, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(file->user));

                infoSaveValue(infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_STR, file->name, jsonFromKv(fileKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR,
            jsonFromVar(saveData->fileGroupDefault, 0));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_PRIMARY_STR,
            jsonFromBool(saveData->filePrimaryDefault));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->fileModeDefault)));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->fileUserDefault, 0));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_LINK_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int linkIdx = 0; linkIdx < lstSize(infoManifest->linkList); linkIdx++)
            {
                InfoManifestLink *link = lstGet(infoManifest->linkList, linkIdx);
                KeyValue *linkKv = kvNew();

                if (!varEq(infoManifestOwnerGet(link->user), saveData->linkUserDefault))
                    kvPut(linkKv, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(link->user));

                if (!varEq(infoManifestOwnerGet(link->group), saveData->linkGroupDefault))
                    kvPut(linkKv, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(link->group));

                kvPut(linkKv, INFO_MANIFEST_KEY_DESTINATION_VAR, VARSTR(link->destination));

                infoSaveValue(infoSaveData, INFO_MANIFEST_SECTION_TARGET_LINK_STR, link->name, jsonFromKv(linkKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, sectionNext))
    {
        if (lstSize(infoManifest->linkList) > 0)
        {
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR,
                jsonFromVar(saveData->linkGroupDefault, 0));
            infoSaveValue(
                infoSaveData, INFO_MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR,
                jsonFromVar(saveData->linkUserDefault, 0));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int pathIdx = 0; pathIdx < lstSize(infoManifest->pathList); pathIdx++)
            {
                InfoManifestPath *path = lstGet(infoManifest->pathList, pathIdx);
                KeyValue *pathKv = kvNew();

                if (!varEq(infoManifestOwnerGet(path->group), saveData->pathGroupDefault))
                    kvPut(pathKv, INFO_MANIFEST_KEY_GROUP_VAR, infoManifestOwnerGet(path->group));

                if (path->mode != saveData->pathModeDefault)
                    kvPut(pathKv, INFO_MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", path->mode)));

                if (!varEq(infoManifestOwnerGet(path->user), saveData->pathUserDefault))
                    kvPut(pathKv, INFO_MANIFEST_KEY_USER_VAR, infoManifestOwnerGet(path->user));

                infoSaveValue(infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_STR, path->name, jsonFromKv(pathKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_GROUP_STR,
            jsonFromVar(saveData->pathGroupDefault, 0));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->pathModeDefault)));
        infoSaveValue(
            infoSaveData, INFO_MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, INFO_MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->pathUserDefault, 0));
    }

    FUNCTION_TEST_RETURN_VOID();
}

void
infoManifestSave(InfoManifest *this, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_MANIFEST, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        InfoManifestSaveData saveData =
        {
            .infoManifest = this,
        };

        // Get default file values
        MostCommonValue *fileGroupMcv = mcvNew();
        MostCommonValue *fileModeMcv = mcvNew();
        MostCommonValue *filePrimaryMcv = mcvNew();
        MostCommonValue *fileUserMcv = mcvNew();

        ASSERT(lstSize(this->fileList) > 0);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
        {
            InfoManifestFile *file = lstGet(this->fileList, fileIdx);

            mcvUpdate(fileGroupMcv, VARSTR(file->group));
            mcvUpdate(fileModeMcv, VARUINT(file->mode));
            mcvUpdate(filePrimaryMcv, VARBOOL(file->primary));
            mcvUpdate(fileUserMcv, VARSTR(file->user));
        }

        saveData.fileGroupDefault = infoManifestOwnerGet(varStr(mcvResult(fileGroupMcv)));
        saveData.fileModeDefault = varUInt(mcvResult(fileModeMcv));
        saveData.filePrimaryDefault = varBool(mcvResult(filePrimaryMcv));
        saveData.fileUserDefault = infoManifestOwnerGet(varStr(mcvResult(fileUserMcv)));

        // Get default link values
        if (lstSize(this->linkList) > 0)
        {
            MostCommonValue *linkGroupMcv = mcvNew();
            MostCommonValue *linkUserMcv = mcvNew();

            for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
            {
                InfoManifestLink *link = lstGet(this->linkList, linkIdx);

                mcvUpdate(linkGroupMcv, VARSTR(link->group));
                mcvUpdate(linkUserMcv, VARSTR(link->user));
            }

            saveData.linkGroupDefault = infoManifestOwnerGet(varStr(mcvResult(linkGroupMcv)));
            saveData.linkUserDefault = infoManifestOwnerGet(varStr(mcvResult(linkUserMcv)));
        }

        // Get default path values
        MostCommonValue *pathGroupMcv = mcvNew();
        MostCommonValue *pathModeMcv = mcvNew();
        MostCommonValue *pathUserMcv = mcvNew();

        ASSERT(lstSize(this->pathList) > 0);

        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            InfoManifestPath *path = lstGet(this->pathList, pathIdx);

            mcvUpdate(pathGroupMcv, VARSTR(path->group));
            mcvUpdate(pathModeMcv, VARUINT(path->mode));
            mcvUpdate(pathUserMcv, VARSTR(path->user));
        }

        saveData.pathGroupDefault = infoManifestOwnerGet(varStr(mcvResult(pathGroupMcv)));
        saveData.pathModeDefault = varUInt(mcvResult(pathModeMcv));
        saveData.pathUserDefault = infoManifestOwnerGet(varStr(mcvResult(pathUserMcv)));

        infoSave(this->info, write, infoManifestSaveCallback, &saveData);
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

/***********************************************************************************************************************************
Helper function to load backup manifest files
***********************************************************************************************************************************/
typedef struct InfoManifestLoadFileData
{
    MemContext *memContext;                                         // Mem context
    const Storage *storage;                                         // Storage to load from
    const String *fileName;                                         // Base filename
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    InfoManifest *infoManifest;                                     // Loaded infoManifest object
} InfoManifestLoadFileData;

static bool
infoManifestLoadFileCallback(void *data, unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    InfoManifestLoadFileData *loadData = (InfoManifestLoadFileData *)data;
    bool result = false;

    if (try < 2)
    {
        // Construct filename based on try
        const String *fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strPtr(loadData->fileName));

        // Attempt to load the file
        IoRead *read = storageReadIo(storageNewReadNP(loadData->storage, fileName));
        cipherBlockFilterGroupAdd(ioReadFilterGroup(read), loadData->cipherType, cipherModeDecrypt, loadData->cipherPass);

        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            loadData->infoManifest = infoManifestNewLoad(read);
            result = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

InfoManifest *
infoManifestLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
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

    InfoManifestLoadFileData data =
    {
        .memContext = memContextCurrent(),
        .storage = storage,
        .fileName = fileName,
        .cipherType = cipherType,
        .cipherPass = cipherPass,
    };

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *fileNamePath = strPtr(storagePathNP(storage, fileName));

        infoLoad(
            strNewFmt("unable to load backup manifest file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
            infoManifestLoadFileCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, data.infoManifest);
}
