/***********************************************************************************************************************************
Manifest Handler
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
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(MANIFEST_FILE_STR,                                    MANIFEST_FILE);

STRING_EXTERN(MANIFEST_TARGET_PGDATA_STR,                           MANIFEST_TARGET_PGDATA);
STRING_EXTERN(MANIFEST_TARGET_PGTBLSPC_STR,                         MANIFEST_TARGET_PGTBLSPC);

STRING_STATIC(MANIFEST_TARGET_TYPE_LINK_STR,                        "link");
STRING_STATIC(MANIFEST_TARGET_TYPE_PATH_STR,                        "path");

STRING_STATIC(MANIFEST_SECTION_BACKUP_STR,                          "backup");
STRING_STATIC(MANIFEST_SECTION_BACKUP_DB_STR,                       "backup:db");
STRING_STATIC(MANIFEST_SECTION_BACKUP_OPTION_STR,                   "backup:option");
STRING_STATIC(MANIFEST_SECTION_BACKUP_TARGET_STR,                   "backup:target");

STRING_STATIC(MANIFEST_SECTION_DB_STR,                              "db");

STRING_STATIC(MANIFEST_SECTION_TARGET_FILE_STR,                     "target:file");
STRING_STATIC(MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR,             "target:file:default");

STRING_STATIC(MANIFEST_SECTION_TARGET_LINK_STR,                     "target:link");
STRING_STATIC(MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR,             "target:link:default");

STRING_STATIC(MANIFEST_SECTION_TARGET_PATH_STR,                     "target:path");
STRING_STATIC(MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR,             "target:path:default");

#define MANIFEST_KEY_BACKUP_ARCHIVE_START                           "backup-archive-start"
    STRING_STATIC(MANIFEST_KEY_BACKUP_ARCHIVE_START_STR,            MANIFEST_KEY_BACKUP_ARCHIVE_START);
#define MANIFEST_KEY_BACKUP_ARCHIVE_STOP                            "backup-archive-stop"
    STRING_STATIC(MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR,             MANIFEST_KEY_BACKUP_ARCHIVE_STOP);
#define MANIFEST_KEY_BACKUP_LABEL                                   "backup-label"
    STRING_STATIC(MANIFEST_KEY_BACKUP_LABEL_STR,                    MANIFEST_KEY_BACKUP_LABEL);
#define MANIFEST_KEY_BACKUP_LSN_START                               "backup-lsn-start"
    STRING_STATIC(MANIFEST_KEY_BACKUP_LSN_START_STR,                MANIFEST_KEY_BACKUP_LSN_START);
#define MANIFEST_KEY_BACKUP_LSN_STOP                                "backup-lsn-stop"
    STRING_STATIC(MANIFEST_KEY_BACKUP_LSN_STOP_STR,                 MANIFEST_KEY_BACKUP_LSN_STOP);
#define MANIFEST_KEY_BACKUP_PRIOR                                   "backup-prior"
    STRING_STATIC(MANIFEST_KEY_BACKUP_PRIOR_STR,                    MANIFEST_KEY_BACKUP_PRIOR);
#define MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START                    "backup-timestamp-copy-start"
    STRING_STATIC(MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR,     MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START);
#define MANIFEST_KEY_BACKUP_TIMESTAMP_START                         "backup-timestamp-start"
    STRING_STATIC(MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,          MANIFEST_KEY_BACKUP_TIMESTAMP_START);
#define MANIFEST_KEY_BACKUP_TIMESTAMP_STOP                          "backup-timestamp-stop"
    STRING_STATIC(MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,           MANIFEST_KEY_BACKUP_TIMESTAMP_STOP);
#define MANIFEST_KEY_BACKUP_TYPE                                    "backup-type"
    STRING_STATIC(MANIFEST_KEY_BACKUP_TYPE_STR,                     MANIFEST_KEY_BACKUP_TYPE);
#define MANIFEST_KEY_CHECKSUM                                       "checksum"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_CHECKSUM_VAR,                MANIFEST_KEY_CHECKSUM);
#define MANIFEST_KEY_CHECKSUM_PAGE                                  "checksum-page"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_CHECKSUM_PAGE_VAR,           MANIFEST_KEY_CHECKSUM_PAGE);
#define MANIFEST_KEY_CHECKSUM_PAGE_ERROR                            "checksum-page-error"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR,     MANIFEST_KEY_CHECKSUM_PAGE_ERROR);
#define MANIFEST_KEY_DB_ID                                          "db-id"
    STRING_STATIC(MANIFEST_KEY_DB_ID_STR,                           MANIFEST_KEY_DB_ID);
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_DB_ID_VAR,                   MANIFEST_KEY_DB_ID);
#define MANIFEST_KEY_DB_LAST_SYSTEM_ID                              "db-last-system-id"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR,       MANIFEST_KEY_DB_LAST_SYSTEM_ID);
#define MANIFEST_KEY_DB_SYSTEM_ID                                   "db-system-id"
    STRING_STATIC(MANIFEST_KEY_DB_SYSTEM_ID_STR,                    MANIFEST_KEY_DB_SYSTEM_ID);
#define MANIFEST_KEY_DB_VERSION                                     "db-version"
    STRING_STATIC(MANIFEST_KEY_DB_VERSION_STR,                      MANIFEST_KEY_DB_VERSION);
#define MANIFEST_KEY_DESTINATION                                    "destination"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_DESTINATION_VAR,             MANIFEST_KEY_DESTINATION);
#define MANIFEST_KEY_FILE                                           "file"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_FILE_VAR,                    MANIFEST_KEY_FILE);
#define MANIFEST_KEY_GROUP                                          "group"
    STRING_STATIC(MANIFEST_KEY_GROUP_STR,                           MANIFEST_KEY_GROUP);
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_GROUP_VAR,                   MANIFEST_KEY_GROUP);
#define MANIFEST_KEY_PRIMARY                                        "ma" "st" "er"
    STRING_STATIC(MANIFEST_KEY_PRIMARY_STR,                         MANIFEST_KEY_PRIMARY);
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_PRIMARY_VAR,                 MANIFEST_KEY_PRIMARY);
#define MANIFEST_KEY_MODE                                           "mode"
    STRING_STATIC(MANIFEST_KEY_MODE_STR,                            MANIFEST_KEY_MODE);
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_MODE_VAR,                    MANIFEST_KEY_MODE);
#define MANIFEST_KEY_PATH                                           "path"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_PATH_VAR,                    MANIFEST_KEY_PATH);
#define MANIFEST_KEY_REFERENCE                                      "reference"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_REFERENCE_VAR,               MANIFEST_KEY_REFERENCE);
#define MANIFEST_KEY_SIZE                                           "size"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_SIZE_VAR,                    MANIFEST_KEY_SIZE);
#define MANIFEST_KEY_SIZE_REPO                                      "repo-size"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_SIZE_REPO_VAR,               MANIFEST_KEY_SIZE_REPO);
#define MANIFEST_KEY_TABLESPACE_ID                                  "tablespace-id"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_TABLESPACE_ID_VAR,           MANIFEST_KEY_TABLESPACE_ID);
#define MANIFEST_KEY_TABLESPACE_NAME                                "tablespace-name"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_TABLESPACE_NAME_VAR,         MANIFEST_KEY_TABLESPACE_NAME);
#define MANIFEST_KEY_TIMESTAMP                                      "timestamp"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_TIMESTAMP_VAR,               MANIFEST_KEY_TIMESTAMP);
#define MANIFEST_KEY_TYPE                                           "type"
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_TYPE_VAR,                    MANIFEST_KEY_TYPE);
#define MANIFEST_KEY_USER                                           "user"
    STRING_STATIC(MANIFEST_KEY_USER_STR,                            MANIFEST_KEY_USER);
    VARIANT_STRDEF_STATIC(MANIFEST_KEY_USER_VAR,                    MANIFEST_KEY_USER);

#define MANIFEST_KEY_OPTION_ARCHIVE_CHECK                           "option-archive-check"
    STRING_STATIC(MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR,            MANIFEST_KEY_OPTION_ARCHIVE_CHECK);
#define MANIFEST_KEY_OPTION_ARCHIVE_COPY                            "option-archive-copy"
    STRING_STATIC(MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR,             MANIFEST_KEY_OPTION_ARCHIVE_COPY);
#define MANIFEST_KEY_OPTION_BACKUP_STANDBY                          "option-backup-standby"
    STRING_STATIC(MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR,           MANIFEST_KEY_OPTION_BACKUP_STANDBY);
#define MANIFEST_KEY_OPTION_BUFFER_SIZE                             "option-buffer-size"
    STRING_STATIC(MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,              MANIFEST_KEY_OPTION_BUFFER_SIZE);
#define MANIFEST_KEY_OPTION_CHECKSUM_PAGE                           "option-checksum-page"
    STRING_STATIC(MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,            MANIFEST_KEY_OPTION_CHECKSUM_PAGE);
#define MANIFEST_KEY_OPTION_COMPRESS                                "option-compress"
    STRING_STATIC(MANIFEST_KEY_OPTION_COMPRESS_STR,                 MANIFEST_KEY_OPTION_COMPRESS);
#define MANIFEST_KEY_OPTION_COMPRESS_LEVEL                          "option-compress-level"
    STRING_STATIC(MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,           MANIFEST_KEY_OPTION_COMPRESS_LEVEL);
#define MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK                  "option-compress-level-network"
    STRING_STATIC(MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,   MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK);
#define MANIFEST_KEY_OPTION_DELTA                                   "option-delta"
    STRING_STATIC(MANIFEST_KEY_OPTION_DELTA_STR,                    MANIFEST_KEY_OPTION_DELTA);
#define MANIFEST_KEY_OPTION_HARDLINK                                "option-hardlink"
    STRING_STATIC(MANIFEST_KEY_OPTION_HARDLINK_STR,                 MANIFEST_KEY_OPTION_HARDLINK);
#define MANIFEST_KEY_OPTION_ONLINE                                  "option-online"
    STRING_STATIC(MANIFEST_KEY_OPTION_ONLINE_STR,                   MANIFEST_KEY_OPTION_ONLINE);
#define MANIFEST_KEY_OPTION_PROCESS_MAX                             "option-process-max"
    STRING_STATIC(MANIFEST_KEY_OPTION_PROCESS_MAX_STR,              MANIFEST_KEY_OPTION_PROCESS_MAX);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct Manifest
{
    MemContext *memContext;                                         // Context that contains the Manifest

    Info *info;                                                     // Base info object
    StringList *ownerList;                                          // List of users/groups
    StringList *referenceList;                                      // List of file references

    ManifestData data;                                              // Manifest data and options
    List *targetList;                                               // List of paths
    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
    List *dbList;                                                   // List of databases
};

/***********************************************************************************************************************************
Internal functions to add types to their lists
***********************************************************************************************************************************/
// Helper to add owner to the owner list if it is not there already and return the pointer.  This saves a lot of space.
static const String *
manifestOwnerCache(Manifest *this, const Variant *owner)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
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
manifestDbAdd(Manifest *this, const ManifestDb *db)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_DB, db);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(db != NULL);
    ASSERT(db->name != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->dbList))
    {
        ManifestDb dbAdd =
        {
            .id = db->id,
            .lastSystemId = db->lastSystemId,
            .name = strDup(db->name),
        };

        lstAdd(this->dbList, &dbAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
manifestFileAdd(Manifest *this, const ManifestFile *file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_PATH, file); // !!! FIX THIS
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(file != NULL);
    ASSERT(file->name != NULL);

    MEM_CONTEXT_BEGIN(lstMemContext(this->fileList))
    {
        ManifestFile fileAdd =
        {
            .checksumPage = file->checksumPage,
            .checksumPageError = file->checksumPageError,
            .checksumPageErrorList = varLstDup(file->checksumPageErrorList),
            .group = manifestOwnerCache(this, VARSTR(file->group)),
            .mode = file->mode,
            .name = strDup(file->name),
            .primary = file->primary,
            .size = file->size,
            .sizeRepo = file->sizeRepo,
            .timestamp = file->timestamp,
            .user = manifestOwnerCache(this, VARSTR(file->user)),
        };

        memcpy(fileAdd.checksumSha1, file->checksumSha1, HASH_TYPE_SHA1_SIZE_HEX + 1);

        if (file->reference != NULL)
        {
            // Search for the reference in the list
            for (unsigned int referenceIdx = 0; referenceIdx < strLstSize(this->referenceList); referenceIdx++)
            {
                const String *found = strLstGet(this->referenceList, referenceIdx);

                if (strEq(file->reference, found))
                {
                    fileAdd.reference = found;
                    break;
                }
            }

            // If not found then add it
            if (fileAdd.reference == NULL)
            {
                strLstAdd(this->referenceList, file->reference);
                fileAdd.reference = strLstGet(this->referenceList, strLstSize(this->referenceList) - 1);
            }
        }

        lstAdd(this->fileList, &fileAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
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

    MEM_CONTEXT_BEGIN(lstMemContext(this->linkList))
    {
        ManifestLink linkAdd =
        {
            .destination = strDup(link->destination),
            .name = strDup(link->name),
            .group = manifestOwnerCache(this, VARSTR(link->group)),
            .user = manifestOwnerCache(this, VARSTR(link->user)),
        };

        lstAdd(this->linkList, &linkAdd);
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

    MEM_CONTEXT_BEGIN(lstMemContext(this->pathList))
    {
        ManifestPath pathAdd =
        {
            .mode = path->mode,
            .name = strDup(path->name),
            .group = manifestOwnerCache(this, VARSTR(path->group)),
            .user = manifestOwnerCache(this, VARSTR(path->user)),
        };

        lstAdd(this->pathList, &pathAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
manifestTargetAdd(Manifest *this, const ManifestTarget *target)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, target);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(target != NULL);
    ASSERT(target->path != NULL);

    // !!! Check that only link types have relative paths

    MEM_CONTEXT_BEGIN(lstMemContext(this->targetList))
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

        lstAdd(this->targetList, &targetAdd);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
static Manifest *
manifestNewInternal(void)
{
    FUNCTION_TEST_VOID();

    // Create object
    Manifest *this = memNew(sizeof(Manifest));
    this->memContext = memContextCurrent();

    // Create lists
    this->dbList = lstNewP(sizeof(ManifestDb), .comparator = lstComparatorStr);
    this->fileList = lstNewP(sizeof(ManifestFile), .comparator =  lstComparatorStr);
    this->linkList = lstNewP(sizeof(ManifestLink), .comparator =  lstComparatorStr);
    this->pathList = lstNewP(sizeof(ManifestPath), .comparator =  lstComparatorStr);
    this->ownerList = strLstNew();
    this->referenceList = strLstNew();
    this->targetList = lstNewP(sizeof(ManifestTarget), .comparator =  lstComparatorStr);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Load manifest
***********************************************************************************************************************************/
// Keep track of which values were found during load and which need to be loaded from defaults. There is no point in having
// multiple structs since most of the fields are the same and the size shouldn't be more than 4/8 bytes.
typedef struct ManifestLoadFound
{
    bool group:1;
    bool mode:1;
    bool primary:1;
    bool user:1;
} ManifestLoadFound;

typedef struct ManifestLoadData
{
    MemContext *memContext;                                         // Mem context for data needed only during load
    Manifest *manifest;                                             // Manifest info

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
} ManifestLoadData;

// Helper to convert owner to a variant.  Input could be boolean false (meaning there is no owner) or a string (there is an owner).
static const Variant *
manifestOwnerDefaultGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    ASSERT(ownerDefault != NULL);

    FUNCTION_TEST_RETURN(strEq(ownerDefault, FALSE_STR) ? BOOL_FALSE_VAR : varNewStr(jsonToStr(ownerDefault)));
}

static void
manifestLoadCallback(void *callbackData, const String *section, const String *key, const String *value)
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

    ManifestLoadData *loadData = (ManifestLoadData *)callbackData;
    Manifest *manifest = loadData->manifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (strEq(section, MANIFEST_SECTION_TARGET_FILE_STR))
    {
        KeyValue *fileKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(manifest->fileList))
        {
            ManifestLoadFound valueFound = {0};

            ManifestFile file =
            {
                .name = key,
                .reference = varStr(kvGetDefault(fileKv, MANIFEST_KEY_REFERENCE_VAR, NULL)),
                .size = varUInt64(kvGet(fileKv, MANIFEST_KEY_SIZE_VAR)),
                .timestamp = (time_t)varUInt64(kvGet(fileKv, MANIFEST_KEY_TIMESTAMP_VAR)),
            };

            file.sizeRepo = varUInt64(kvGetDefault(fileKv, MANIFEST_KEY_SIZE_REPO_VAR, VARUINT64(file.size)));

            if (file.size == 0)
                memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
            else
            {
                memcpy(
                    file.checksumSha1, strPtr(varStr(kvGet(fileKv, MANIFEST_KEY_CHECKSUM_VAR))), HASH_TYPE_SHA1_SIZE_HEX + 1);
            }

            const Variant *checksumPage = kvGetDefault(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_VAR, NULL);

            if (checksumPage != NULL)
            {
                file.checksumPage = true;
                file.checksumPageError = varBool(checksumPage);

                const Variant *checksumPageErrorList = kvGetDefault(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, NULL);

                if (checksumPageErrorList != NULL)
                    file.checksumPageErrorList = varVarLst(checksumPageErrorList);
            }

            if (kvKeyExists(fileKv, MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                file.group = manifestOwnerCache(manifest, kvGet(fileKv, MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(fileKv, MANIFEST_KEY_MODE_VAR))
            {
                valueFound.mode = true;
                file.mode = cvtZToMode(strPtr(varStr(kvGet(fileKv, MANIFEST_KEY_MODE_VAR))));
            }

            if (kvKeyExists(fileKv, MANIFEST_KEY_PRIMARY_VAR))
            {
                valueFound.primary = true;
                file.primary = varBool(kvGet(fileKv, MANIFEST_KEY_PRIMARY_VAR));
            }

            if (kvKeyExists(fileKv, MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                file.user = manifestOwnerCache(manifest, kvGet(fileKv, MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->fileFoundList, &valueFound);
            manifestFileAdd(manifest, &file);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_TARGET_PATH_STR))
    {
        KeyValue *pathKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(manifest->pathList))
        {
            ManifestLoadFound valueFound = {0};

            ManifestPath path =
            {
                .name = key,
            };

            if (kvKeyExists(pathKv, MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                path.group = manifestOwnerCache(manifest, kvGet(pathKv, MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(pathKv, MANIFEST_KEY_MODE_VAR))
            {
                valueFound.mode = true;
                path.mode = cvtZToMode(strPtr(varStr(kvGet(pathKv, MANIFEST_KEY_MODE_VAR))));
            }

            if (kvKeyExists(pathKv, MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                path.user = manifestOwnerCache(manifest, kvGet(pathKv, MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->pathFoundList, &valueFound);
            manifestPathAdd(manifest, &path);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_TARGET_LINK_STR))
    {
        KeyValue *linkKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(manifest->linkList))
        {
            ManifestLoadFound valueFound = {0};

            ManifestLink link =
            {
                .name = key,
                .destination = varStr(kvGet(linkKv, MANIFEST_KEY_DESTINATION_VAR)),
            };

            if (kvKeyExists(linkKv, MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                link.group = manifestOwnerCache(manifest, kvGet(linkKv, MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(linkKv, MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                link.user = manifestOwnerCache(manifest, kvGet(linkKv, MANIFEST_KEY_USER_VAR));
            }

            lstAdd(loadData->linkFoundList, &valueFound);
            manifestLinkAdd(manifest, &link);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, MANIFEST_KEY_GROUP_STR))
                loadData->fileGroupDefault = manifestOwnerDefaultGet(value);
            else if (strEq(key, MANIFEST_KEY_MODE_STR))
                loadData->fileModeDefault = cvtZToMode(strPtr(jsonToStr(value)));
            else if (strEq(key, MANIFEST_KEY_PRIMARY_STR))
                loadData->filePrimaryDefault = jsonToBool(value);
            else if (strEq(key, MANIFEST_KEY_USER_STR))
                loadData->fileUserDefault = manifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, MANIFEST_KEY_GROUP_STR))
                loadData->pathGroupDefault = manifestOwnerDefaultGet(value);
            else if (strEq(key, MANIFEST_KEY_MODE_STR))
                loadData->pathModeDefault = cvtZToMode(strPtr(jsonToStr(value)));
            else if (strEq(key, MANIFEST_KEY_USER_STR))
                loadData->pathUserDefault = manifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR))
    {
        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            if (strEq(key, MANIFEST_KEY_GROUP_STR))
                loadData->linkGroupDefault = manifestOwnerDefaultGet(value);
            else if (strEq(key, MANIFEST_KEY_USER_STR))
                loadData->linkUserDefault = manifestOwnerDefaultGet(value);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_BACKUP_TARGET_STR))
    {
        KeyValue *targetKv = varKv(jsonToVar(value));
        const String *targetType = varStr(kvGet(targetKv, MANIFEST_KEY_TYPE_VAR));

        ASSERT(strEq(targetType, MANIFEST_TARGET_TYPE_LINK_STR) || strEq(targetType, MANIFEST_TARGET_TYPE_PATH_STR));

        ManifestTarget target =
        {
            .name = key,
            .file = varStr(kvGetDefault(targetKv, MANIFEST_KEY_FILE_VAR, NULL)),
            .path = varStr(kvGet(targetKv, MANIFEST_KEY_PATH_VAR)),
            .tablespaceId =
                cvtZToUInt(strPtr(varStr(kvGetDefault(targetKv, MANIFEST_KEY_TABLESPACE_ID_VAR, VARSTRDEF("0"))))),
            .tablespaceName = varStr(kvGetDefault(targetKv, MANIFEST_KEY_TABLESPACE_NAME_VAR, NULL)),
            .type = strEq(targetType, MANIFEST_TARGET_TYPE_PATH_STR) ? manifestTargetTypePath : manifestTargetTypeLink,
        };

        manifestTargetAdd(manifest, &target);
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_DB_STR))
    {
        KeyValue *dbKv = varKv(jsonToVar(value));

        MEM_CONTEXT_BEGIN(lstMemContext(manifest->dbList))
        {
            ManifestDb db =
            {
                .name = strDup(key),
                .id = varUIntForce(kvGet(dbKv, MANIFEST_KEY_DB_ID_VAR)),
                .lastSystemId = varUIntForce(kvGet(dbKv, MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR)),
            };

            manifestDbAdd(manifest, &db);
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_BACKUP_STR))
    {
        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            if (strEq(key, MANIFEST_KEY_BACKUP_ARCHIVE_START_STR))
                manifest->data.archiveStart = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR))
                manifest->data.archiveStop = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_LABEL_STR))
                manifest->data.backupLabel = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_LSN_START_STR))
                manifest->data.lsnStart = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_LSN_STOP_STR))
                manifest->data.lsnStop = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_PRIOR_STR))
                manifest->data.backupLabelPrior = jsonToStr(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR))
                manifest->data.backupTimestampCopyStart = (time_t)jsonToUInt64(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR))
                manifest->data.backupTimestampStart = (time_t)jsonToUInt64(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR))
                manifest->data.backupTimestampStop = (time_t)jsonToUInt64(value);
            else if (strEq(key, MANIFEST_KEY_BACKUP_TYPE_STR))
                manifest->data.backupType = backupType(jsonToStr(value));
        }
        MEM_CONTEXT_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_BACKUP_DB_STR))
    {
        if (strEq(key, MANIFEST_KEY_DB_ID_STR))
            manifest->data.pgId = jsonToUInt(value);
        else if (strEq(key, MANIFEST_KEY_DB_SYSTEM_ID_STR))
            manifest->data.pgSystemId = jsonToUInt64(value);
        else if (strEq(key, MANIFEST_KEY_DB_VERSION_STR))
            manifest->data.pgVersion = pgVersionFromStr(jsonToStr(value));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    else if (strEq(section, MANIFEST_SECTION_BACKUP_OPTION_STR))
    {
        MEM_CONTEXT_BEGIN(manifest->memContext)
        {
            // Required options
            if (strEq(key, MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR))
                manifest->data.backupOptionArchiveCheck = jsonToBool(value);
            else if (strEq(key, MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR))
                manifest->data.backupOptionArchiveCopy = jsonToBool(value);
            else if (strEq(key, MANIFEST_KEY_OPTION_COMPRESS_STR))
                manifest->data.backupOptionCompress = jsonToBool(value);
            else if (strEq(key, MANIFEST_KEY_OPTION_HARDLINK_STR))
                manifest->data.backupOptionHardLink = jsonToBool(value);
            else if (strEq(key, MANIFEST_KEY_OPTION_ONLINE_STR))
                manifest->data.backupOptionOnline = jsonToBool(value);

            // Options that were added after v1.00 and may not be present in every manifest
            else if (strEq(key, MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR))
                manifest->data.backupOptionStandby = varNewBool(jsonToBool(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_BUFFER_SIZE_STR))
                manifest->data.backupOptionBufferSize = varNewUInt(jsonToUInt(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR))
                manifest->data.backupOptionChecksumPage = varNewBool(jsonToBool(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR))
                manifest->data.backupOptionCompressLevel = varNewUInt(jsonToUInt(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR))
                manifest->data.backupOptionCompressLevelNetwork = varNewUInt(jsonToUInt(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_DELTA_STR))
                manifest->data.backupOptionDelta = varNewBool(jsonToBool(value));
            else if (strEq(key, MANIFEST_KEY_OPTION_PROCESS_MAX_STR))
                manifest->data.backupOptionProcessMax = varNewUInt(jsonToUInt(value));
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

    MEM_CONTEXT_NEW_BEGIN("Manifest")
    {
        this = manifestNewInternal();

        // Load the manifest
        ManifestLoadData loadData =
        {
            .memContext = memContextNew("load"),
            .manifest = this,
        };

        MEM_CONTEXT_BEGIN(loadData.memContext)
        {
            loadData.fileFoundList = lstNew(sizeof(ManifestLoadFound));
            loadData.linkFoundList = lstNew(sizeof(ManifestLoadFound));
            loadData.pathFoundList = lstNew(sizeof(ManifestLoadFound));
        }
        MEM_CONTEXT_END();

        this->info = infoNewLoad(read, manifestLoadCallback, &loadData);

        // Process file defaults
        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
        {
            ManifestFile *file = lstGet(this->fileList, fileIdx);
            ManifestLoadFound *found = lstGet(loadData.fileFoundList, fileIdx);

            if (!found->group)
                file->group = manifestOwnerCache(this, loadData.fileGroupDefault);

            if (!found->mode)
                file->mode = loadData.fileModeDefault;

            if (!found->primary)
                file->primary = loadData.filePrimaryDefault;

            if (!found->user)
                file->user = manifestOwnerCache(this, loadData.fileUserDefault);
        }

        // Process link defaults
        for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
        {
            ManifestLink *link = lstGet(this->linkList, linkIdx);
            ManifestLoadFound *found = lstGet(loadData.linkFoundList, linkIdx);

            if (!found->group)
                link->group = manifestOwnerCache(this, loadData.linkGroupDefault);

            if (!found->user)
                link->user = manifestOwnerCache(this, loadData.linkUserDefault);
        }

        // Process path defaults
        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            ManifestPath *path = lstGet(this->pathList, pathIdx);
            ManifestLoadFound *found = lstGet(loadData.pathFoundList, pathIdx);

            if (!found->group)
                path->group = manifestOwnerCache(this, loadData.pathGroupDefault);

            if (!found->mode)
                path->mode = loadData.pathModeDefault;

            if (!found->user)
                path->user = manifestOwnerCache(this, loadData.pathUserDefault);
        }

        // Make sure the base path exists
        manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR);

        // Free the context holding temporary load data
        memContextFree(loadData.memContext);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(MANIFEST, this);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
typedef struct ManifestSaveData
{
    Manifest *manifest;                                     // Manifest object to be saved

    const Variant *fileGroupDefault;                                // File default group
    mode_t fileModeDefault;                                         // File default mode
    bool filePrimaryDefault;                                        // File default primary
    const Variant *fileUserDefault;                                 // File default user

    const Variant *linkGroupDefault;                                // Link default group
    const Variant *linkUserDefault;                                 // Link default user

    const Variant *pathGroupDefault;                                // Path default group
    mode_t pathModeDefault;                                         // Path default mode
    const Variant *pathUserDefault;                                 // Path default user
} ManifestSaveData;

// Helper to convert the owner MCV to a default.  If the input is NULL boolean false should be returned, else the owner string.
static const Variant *
manifestOwnerGet(const String *ownerDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, ownerDefault);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(ownerDefault == NULL ? BOOL_FALSE_VAR : varNewStr(ownerDefault));
}

static void
manifestSaveCallback(void *callbackData, const String *sectionNext, InfoSave *infoSaveData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, callbackData);
        FUNCTION_TEST_PARAM(STRING, sectionNext);
        FUNCTION_TEST_PARAM(INFO_SAVE, infoSaveData);
    FUNCTION_TEST_END();

    ASSERT(callbackData != NULL);
    ASSERT(infoSaveData != NULL);

    ManifestSaveData *saveData = (ManifestSaveData *)callbackData;
    Manifest *manifest = saveData->manifest;

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_STR, sectionNext))
    {
        if (manifest->data.archiveStart != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_ARCHIVE_START_STR,
                jsonFromStr(manifest->data.archiveStart));
        }

        if (manifest->data.archiveStop != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_ARCHIVE_STOP_STR,
                jsonFromStr(manifest->data.archiveStop));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_LABEL_STR,
            jsonFromStr(manifest->data.backupLabel));

        if (manifest->data.lsnStart != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_LSN_START_STR,
                jsonFromStr(manifest->data.lsnStart));
        }

        if (manifest->data.lsnStop != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_LSN_STOP_STR,
                jsonFromStr(manifest->data.lsnStop));
        }

        if (manifest->data.backupLabelPrior != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_PRIOR_STR,
                jsonFromStr(manifest->data.backupLabelPrior));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_TIMESTAMP_COPY_START_STR,
            jsonFromInt64(manifest->data.backupTimestampCopyStart));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_TIMESTAMP_START_STR,
            jsonFromInt64(manifest->data.backupTimestampStart));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_TIMESTAMP_STOP_STR,
            jsonFromInt64(manifest->data.backupTimestampStop));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_STR, MANIFEST_KEY_BACKUP_TYPE_STR,
            jsonFromStr(backupTypeStr(manifest->data.backupType)));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, STRDEF("db-catalog-version"),
            jsonFromUInt(pgCatalogVersion(manifest->data.pgVersion)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, STRDEF("db-control-version"),
            jsonFromUInt(pgControlVersion(manifest->data.pgVersion)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, MANIFEST_KEY_DB_ID_STR, jsonFromUInt(manifest->data.pgId));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, MANIFEST_KEY_DB_SYSTEM_ID_STR,
            jsonFromUInt64(manifest->data.pgSystemId));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_DB_STR, MANIFEST_KEY_DB_VERSION_STR,
            jsonFromStr(pgVersionToStr(manifest->data.pgVersion)));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_ARCHIVE_CHECK_STR,
            jsonFromBool(manifest->data.backupOptionArchiveCheck));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_ARCHIVE_COPY_STR,
            jsonFromBool(manifest->data.backupOptionArchiveCopy));

        if (manifest->data.backupOptionStandby != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_BACKUP_STANDBY_STR,
                jsonFromVar(manifest->data.backupOptionStandby, 0));
        }

        if (manifest->data.backupOptionBufferSize != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,
                jsonFromVar(manifest->data.backupOptionBufferSize, 0));
        }

        if (manifest->data.backupOptionChecksumPage != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,
                jsonFromVar(manifest->data.backupOptionChecksumPage, 0));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_STR,
            jsonFromBool(manifest->data.backupOptionCompress));

        if (manifest->data.backupOptionCompressLevel != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,
                jsonFromVar(manifest->data.backupOptionCompressLevel, 0));
        }

        if (manifest->data.backupOptionCompressLevelNetwork != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,
                jsonFromVar(manifest->data.backupOptionCompressLevelNetwork, 0));
        }

        if (manifest->data.backupOptionDelta != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_DELTA_STR,
                jsonFromVar(manifest->data.backupOptionDelta, 0));
        }

        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_HARDLINK_STR,
            jsonFromBool(manifest->data.backupOptionHardLink));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_ONLINE_STR,
            jsonFromBool(manifest->data.backupOptionOnline));

        if (manifest->data.backupOptionProcessMax != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_PROCESS_MAX_STR,
                jsonFromVar(manifest->data.backupOptionProcessMax, 0));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_BACKUP_TARGET_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *target = manifestTarget(manifest, targetIdx);
                KeyValue *targetKv = kvNew();

                if (target->file != NULL)
                    kvPut(targetKv, MANIFEST_KEY_FILE_VAR, VARSTR(target->file));

                kvPut(targetKv, MANIFEST_KEY_PATH_VAR, VARSTR(target->path));

                if (target->tablespaceId != 0)
                    kvPut(targetKv, MANIFEST_KEY_TABLESPACE_ID_VAR, VARSTR(strNewFmt("%u", target->tablespaceId)));

                if (target->tablespaceName != NULL)
                    kvPut(targetKv, MANIFEST_KEY_TABLESPACE_NAME_VAR, VARSTR(target->tablespaceName));

                kvPut(
                    targetKv, MANIFEST_KEY_TYPE_VAR,
                    VARSTR(
                        target->type == manifestTargetTypePath ?
                            MANIFEST_TARGET_TYPE_PATH_STR : MANIFEST_TARGET_TYPE_LINK_STR));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_BACKUP_TARGET_STR, target->name, jsonFromKv(targetKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_DB_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int dbIdx = 0; dbIdx < manifestDbTotal(manifest); dbIdx++)
            {
                const ManifestDb *db = manifestDb(manifest, dbIdx);
                KeyValue *dbKv = kvNew();

                kvPut(dbKv, MANIFEST_KEY_DB_ID_VAR, VARUINT(db->id));
                kvPut(dbKv, MANIFEST_KEY_DB_LAST_SYSTEM_ID_VAR, VARUINT(db->lastSystemId));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_DB_STR, db->name, jsonFromKv(dbKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_FILE_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile *file = manifestFile(manifest, fileIdx);
                KeyValue *fileKv = kvNew();

                if (file->size != 0)
                    kvPut(fileKv, MANIFEST_KEY_CHECKSUM_VAR, VARSTRZ(file->checksumSha1));

                if (file->checksumPage)
                {
                    kvPut(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_VAR, VARBOOL(file->checksumPageError));

                    if (file->checksumPageErrorList != NULL)
                        kvPut(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, varNewVarLst(file->checksumPageErrorList));
                }

                if (!varEq(manifestOwnerGet(file->group), saveData->fileGroupDefault))
                    kvPut(fileKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerGet(file->group));

                if (file->primary != saveData->filePrimaryDefault)
                    kvPut(fileKv, MANIFEST_KEY_PRIMARY_VAR, VARBOOL(file->primary));

                if (file->mode != saveData->fileModeDefault)
                    kvPut(fileKv, MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", file->mode)));

                if (file->reference != NULL)
                    kvPut(fileKv, MANIFEST_KEY_REFERENCE_VAR, VARSTR(file->reference));

                if (file->sizeRepo != file->size)
                    kvPut(fileKv, MANIFEST_KEY_SIZE_REPO_VAR, varNewUInt64(file->sizeRepo));

                kvPut(fileKv, MANIFEST_KEY_SIZE_VAR, varNewUInt64(file->size));

                kvPut(fileKv, MANIFEST_KEY_TIMESTAMP_VAR, varNewUInt64((uint64_t)file->timestamp));

                if (!varEq(manifestOwnerGet(file->user), saveData->fileUserDefault))
                    kvPut(fileKv, MANIFEST_KEY_USER_VAR, manifestOwnerGet(file->user));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_FILE_STR, file->name, jsonFromKv(fileKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_GROUP_STR,
            jsonFromVar(saveData->fileGroupDefault, 0));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_PRIMARY_STR,
            jsonFromBool(saveData->filePrimaryDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->fileModeDefault)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->fileUserDefault, 0));
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_LINK_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(manifest); linkIdx++)
            {
                const ManifestLink *link = manifestLink(manifest, linkIdx);
                KeyValue *linkKv = kvNew();

                if (!varEq(manifestOwnerGet(link->user), saveData->linkUserDefault))
                    kvPut(linkKv, MANIFEST_KEY_USER_VAR, manifestOwnerGet(link->user));

                if (!varEq(manifestOwnerGet(link->group), saveData->linkGroupDefault))
                    kvPut(linkKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerGet(link->group));

                kvPut(linkKv, MANIFEST_KEY_DESTINATION_VAR, VARSTR(link->destination));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_LINK_STR, link->name, jsonFromKv(linkKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, sectionNext))
    {
        if (lstSize(manifest->linkList) > 0)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, MANIFEST_KEY_GROUP_STR,
                jsonFromVar(saveData->linkGroupDefault, 0));
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, MANIFEST_KEY_USER_STR,
                jsonFromVar(saveData->linkUserDefault, 0));
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_PATH_STR, sectionNext))
    {
        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                const ManifestPath *path = manifestPath(manifest, pathIdx);
                KeyValue *pathKv = kvNew();

                if (!varEq(manifestOwnerGet(path->group), saveData->pathGroupDefault))
                    kvPut(pathKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerGet(path->group));

                if (path->mode != saveData->pathModeDefault)
                    kvPut(pathKv, MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", path->mode)));

                if (!varEq(manifestOwnerGet(path->user), saveData->pathUserDefault))
                    kvPut(pathKv, MANIFEST_KEY_USER_VAR, manifestOwnerGet(path->user));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_PATH_STR, path->name, jsonFromKv(pathKv, 0));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, sectionNext))
    {
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, MANIFEST_KEY_GROUP_STR,
            jsonFromVar(saveData->pathGroupDefault, 0));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->pathModeDefault)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->pathUserDefault, 0));
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
        ManifestSaveData saveData =
        {
            .manifest = this,
        };

        // Get default file values
        MostCommonValue *fileGroupMcv = mcvNew();
        MostCommonValue *fileModeMcv = mcvNew();
        MostCommonValue *filePrimaryMcv = mcvNew();
        MostCommonValue *fileUserMcv = mcvNew();

        ASSERT(lstSize(this->fileList) > 0);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
        {
            ManifestFile *file = lstGet(this->fileList, fileIdx);

            mcvUpdate(fileGroupMcv, VARSTR(file->group));
            mcvUpdate(fileModeMcv, VARUINT(file->mode));
            mcvUpdate(filePrimaryMcv, VARBOOL(file->primary));
            mcvUpdate(fileUserMcv, VARSTR(file->user));
        }

        saveData.fileGroupDefault = manifestOwnerGet(varStr(mcvResult(fileGroupMcv)));
        saveData.fileModeDefault = varUInt(mcvResult(fileModeMcv));
        saveData.filePrimaryDefault = varBool(mcvResult(filePrimaryMcv));
        saveData.fileUserDefault = manifestOwnerGet(varStr(mcvResult(fileUserMcv)));

        // Get default link values
        if (lstSize(this->linkList) > 0)
        {
            MostCommonValue *linkGroupMcv = mcvNew();
            MostCommonValue *linkUserMcv = mcvNew();

            for (unsigned int linkIdx = 0; linkIdx < lstSize(this->linkList); linkIdx++)
            {
                ManifestLink *link = lstGet(this->linkList, linkIdx);

                mcvUpdate(linkGroupMcv, VARSTR(link->group));
                mcvUpdate(linkUserMcv, VARSTR(link->user));
            }

            saveData.linkGroupDefault = manifestOwnerGet(varStr(mcvResult(linkGroupMcv)));
            saveData.linkUserDefault = manifestOwnerGet(varStr(mcvResult(linkUserMcv)));
        }

        // Get default path values
        MostCommonValue *pathGroupMcv = mcvNew();
        MostCommonValue *pathModeMcv = mcvNew();
        MostCommonValue *pathUserMcv = mcvNew();

        ASSERT(lstSize(this->pathList) > 0);

        for (unsigned int pathIdx = 0; pathIdx < lstSize(this->pathList); pathIdx++)
        {
            ManifestPath *path = lstGet(this->pathList, pathIdx);

            mcvUpdate(pathGroupMcv, VARSTR(path->group));
            mcvUpdate(pathModeMcv, VARUINT(path->mode));
            mcvUpdate(pathUserMcv, VARSTR(path->user));
        }

        saveData.pathGroupDefault = manifestOwnerGet(varStr(mcvResult(pathGroupMcv)));
        saveData.pathModeDefault = varUInt(mcvResult(pathModeMcv));
        saveData.pathUserDefault = manifestOwnerGet(varStr(mcvResult(pathUserMcv)));

        infoSave(this->info, write, manifestSaveCallback, &saveData);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Ensure that symlinks do not point to the same directory or a subdirectory of another link
***********************************************************************************************************************************/
void
manifestLinkCheck(const Manifest *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
    FUNCTION_LOG_END();

    for (unsigned int linkIdx1 = 0; linkIdx1 < manifestTargetTotal(this); linkIdx1++)
    {
        const ManifestTarget *link1 = manifestTarget(this, linkIdx1);

        if (link1->type == manifestTargetTypeLink)
        {
            for (unsigned int linkIdx2 = 0; linkIdx2 < manifestTargetTotal(this); linkIdx2++)
            {
                const ManifestTarget *link2 = manifestTarget(this, linkIdx2);

                if (link2->type == manifestTargetTypeLink && link1 != link2)
                {
                    if (!(link1->file != NULL && link2->file != NULL) &&
                        strBeginsWith(
                            strNewFmt("%s/", strPtr(manifestTargetPath(this, link1))),
                            strNewFmt("%s/", strPtr(manifestTargetPath(this, link2)))))
                    {
                        THROW_FMT(
                            LinkDestinationError,
                            "link '%s' (%s) destination is a subdirectory of or the same directory as link '%s' (%s)",
                            strPtr(manifestPgPath(link1->name)), strPtr(manifestTargetPath(this, link1)),
                            strPtr(manifestPgPath(link2->name)), strPtr(manifestTargetPath(this, link2)));
                    }
                }
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Return the base target, i.e. the target that is the data directory
***********************************************************************************************************************************/
const ManifestTarget *
manifestTargetBase(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR));
}

/***********************************************************************************************************************************
Return an absolute path to the target
***********************************************************************************************************************************/
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
        FUNCTION_TEST_RETURN(strDup(target->path));

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *pgPath = strPath(manifestPgPath(target->name));

        if (strSize(pgPath) != 0)
            strCat(pgPath, "/");

        strCat(pgPath, strPtr(target->path));

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strPathAbsolute(pgPath, manifestTargetBase(this)->path);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Return the data directory relative path for any manifest file/link/path/target name
***********************************************************************************************************************************/
String *
manifestPgPath(const String *manifestPath)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, manifestPath);
    FUNCTION_TEST_END();

    ASSERT(manifestPath != NULL);

    // If something in pg_data/
    if (strBeginsWith(manifestPath, STRDEF(MANIFEST_TARGET_PGDATA "/")))
    {
        FUNCTION_TEST_RETURN(strNew(strPtr(manifestPath) + sizeof(MANIFEST_TARGET_PGDATA)));
    }
    // Else not pg_data (this is faster since the length of everything else will be different than pg_data)
    else if (!strEq(manifestPath, MANIFEST_TARGET_PGDATA_STR))
    {
        // A tablespace target is the only valid option if not pg_data or pg_data/
        ASSERT(strBeginsWith(manifestPath, STRDEF(MANIFEST_TARGET_PGTBLSPC "/")));

        FUNCTION_TEST_RETURN(strDup(manifestPath));
    }

    FUNCTION_TEST_RETURN(NULL);
}

/***********************************************************************************************************************************
Return manifest configuration and options
***********************************************************************************************************************************/
const ManifestData *
manifestData(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(&this->data);
}

/***********************************************************************************************************************************
Db functions and getters/setters
***********************************************************************************************************************************/
const ManifestDb *
manifestDb(const Manifest *this, unsigned int dbIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(UINT, dbIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstGet(this->dbList, dbIdx));
}

const ManifestDb *
manifestDbFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestDb *result = lstFind(this->dbList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest db list", strPtr(name));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
If the database requested is not found in the list, return the default passed rather than throw an error
***********************************************************************************************************************************/
const ManifestDb *
manifestDbFindDefault(const Manifest *this, const String *name, const ManifestDb *dbDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(MANIFEST_DB, dbDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(lstFindDefault(this->dbList, &name, (void *)dbDefault));
}

unsigned int
manifestDbTotal(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize(this->dbList));
}

/***********************************************************************************************************************************
File functions and getters/setters
***********************************************************************************************************************************/
const ManifestFile *
manifestFile(const Manifest *this, unsigned int fileIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(UINT, fileIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstGet(this->fileList, fileIdx));
}

const ManifestFile *
manifestFileFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestFile *result = lstFind(this->fileList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest file list", strPtr(name));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
If the file requested is not found in the list, return the default passed rather than throw an error
***********************************************************************************************************************************/
const ManifestFile *
manifestFileFindDefault(const Manifest *this, const String *name, const ManifestFile *fileDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, fileDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(lstFindDefault(this->fileList, &name, (void *)fileDefault));
}

unsigned int
manifestFileTotal(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize(this->fileList));
}

/***********************************************************************************************************************************
Link functions and getters/setters
***********************************************************************************************************************************/
const ManifestLink *
manifestLink(const Manifest *this, unsigned int linkIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(UINT, linkIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstGet(this->linkList, linkIdx));
}

const ManifestLink *
manifestLinkFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestLink *result = lstFind(this->linkList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest link list", strPtr(name));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
If the link requested is not found in the list, return the default passed rather than throw an error
***********************************************************************************************************************************/
const ManifestLink *
manifestLinkFindDefault(const Manifest *this, const String *name, const ManifestLink *linkDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, linkDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(lstFindDefault(this->linkList, &name, (void *)linkDefault));
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

    if (!lstRemove(this->linkList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest link list", strPtr(name));

    FUNCTION_TEST_RETURN_VOID();
}

unsigned int
manifestLinkTotal(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize(this->linkList));
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

    MEM_CONTEXT_BEGIN(lstMemContext(this->linkList))
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
manifestPath(const Manifest *this, unsigned int pathIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(UINT, pathIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstGet(this->pathList, pathIdx));
}

const ManifestPath *
manifestPathFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestPath *result = lstFind(this->pathList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest path list", strPtr(name));

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
If the path requested is not found in the list, return the default passed rather than throw an error
***********************************************************************************************************************************/
const ManifestPath *
manifestPathFindDefault(const Manifest *this, const String *name, const ManifestPath *pathDefault)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(MANIFEST_TARGET, pathDefault);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    FUNCTION_TEST_RETURN(lstFindDefault(this->pathList, &name, (void *)pathDefault));
}

unsigned int
manifestPathTotal(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize(this->pathList));
}

/***********************************************************************************************************************************
Target functions and getters/setters
***********************************************************************************************************************************/
const ManifestTarget *
manifestTarget(const Manifest *this, unsigned int targetIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(UINT, targetIdx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstGet(this->targetList, targetIdx));
}

const ManifestTarget *
manifestTargetFind(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const ManifestTarget *result = lstFind(this->targetList, &name);

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find '%s' in manifest target list", strPtr(name));

    FUNCTION_TEST_RETURN(result);
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

    if (!lstRemove(this->targetList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest target list", strPtr(name));

    FUNCTION_TEST_RETURN_VOID();
}

unsigned int
manifestTargetTotal(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(lstSize(this->targetList));
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

    MEM_CONTEXT_BEGIN(lstMemContext(this->targetList))
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
Helper function to load backup manifest files
***********************************************************************************************************************************/
typedef struct ManifestLoadFileData
{
    MemContext *memContext;                                         // Mem context
    const Storage *storage;                                         // Storage to load from
    const String *fileName;                                         // Base filename
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    Manifest *manifest;                                     // Loaded manifest object
} ManifestLoadFileData;

static bool
manifestLoadFileCallback(void *data, unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    ManifestLoadFileData *loadData = (ManifestLoadFileData *)data;
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
            loadData->manifest = manifestNewLoad(read);
            result = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

Manifest *
manifestLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
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
        const char *fileNamePath = strPtr(storagePathNP(storage, fileName));

        infoLoad(
            strNewFmt("unable to load backup manifest file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
            manifestLoadFileCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, data.manifest);
}
