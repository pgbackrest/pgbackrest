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
#include "common/type/mcv.h"
#include "common/type/object.h"
#include "info/info.h"
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
#define MANIFEST_KEY_OPTION_COMPRESS_TYPE                           "option-compress-type"
    STRING_STATIC(MANIFEST_KEY_OPTION_COMPRESS_TYPE_STR,            MANIFEST_KEY_OPTION_COMPRESS_TYPE);
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
    List *targetList;                                               // List of targets
    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
    List *dbList;                                                   // List of databases
};

OBJECT_DEFINE_MOVE(MANIFEST);

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
        FUNCTION_TEST_RETURN(strLstAddIfMissing(this->ownerList, owner));

    FUNCTION_TEST_RETURN(NULL);
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

void
manifestFileAdd(Manifest *this, const ManifestFile *file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
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
            .group = manifestOwnerCache(this, file->group),
            .mode = file->mode,
            .name = strDup(file->name),
            .primary = file->primary,
            .size = file->size,
            .sizeRepo = file->sizeRepo,
            .timestamp = file->timestamp,
            .user = manifestOwnerCache(this, file->user),
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
            .group = manifestOwnerCache(this, link->group),
            .user = manifestOwnerCache(this, link->user),
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
            .group = manifestOwnerCache(this, path->group),
            .user = manifestOwnerCache(this, path->user),
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
    ASSERT(target->name != NULL);

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
Internal constructor
***********************************************************************************************************************************/
static Manifest *
manifestNewInternal(void)
{
    FUNCTION_TEST_VOID();

    Manifest *this = memNew(sizeof(Manifest));

    *this = (Manifest)
    {
        .memContext = memContextCurrent(),
        .dbList = lstNewP(sizeof(ManifestDb), .comparator = lstComparatorStr),
        .fileList = lstNewP(sizeof(ManifestFile), .comparator =  lstComparatorStr),
        .linkList = lstNewP(sizeof(ManifestLink), .comparator =  lstComparatorStr),
        .pathList = lstNewP(sizeof(ManifestPath), .comparator =  lstComparatorStr),
        .ownerList = strLstNew(),
        .referenceList = strLstNew(),
        .targetList = lstNewP(sizeof(ManifestTarget), .comparator =  lstComparatorStr),
    };

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
typedef struct ManifestBuildData
{
    Manifest *manifest;
    const Storage *storagePg;
    const String *tablespaceId;                                     // Tablespace id if PostgreSQL version has one
    bool online;                                                    // Is this an online backup?
    bool checksumPage;                                              // Are page checksums being checked?
    const String *manifestWalName;                                  // Wal manifest name for this version of PostgreSQL
    RegExp *dbPathExp;                                              // Identify paths containing relations
    RegExp *tempRelationExp;                                        // Identify temp relations
    RegExp *standbyExp;                                             // Identify files that must be copied from the primary
    const VariantList *tablespaceList;                              // List of tablespaces in the database
    StringList *excludeContent;                                     // Exclude contents of directories
    StringList *excludeSingle;                                      // Exclude a single file/link/path

    // These change with each level of recursion
    const String *manifestParentName;                               // Manifest name of this file/link/path's parent
    const String *pgPath;                                           // Current path in the PostgreSQL data directory
    bool dbPath;                                                    // Does this path contain relations?
} ManifestBuildData;

// Callback to process files/links/paths and add them to the manifest
static void
manifestBuildCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, *storageInfo);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    // Skip all . paths because they have already been recorded on the previous level of recursion
    if (strEq(info->name, DOT_STR))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Skip any path/file/link that begins with pgsql_tmp.  The files are removed when the server is restarted and the directories
    // are recreated.
    if (strBeginsWithZ(info->name, PG_PREFIX_PGSQLTMP))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Get build data
    ManifestBuildData buildData = *(ManifestBuildData *)data;
    unsigned int pgVersion = buildData.manifest->data.pgVersion;

    // Contruct the name used to identify this file/link/path in the manifest
    const String *manifestName = strNewFmt("%s/%s", strPtr(buildData.manifestParentName), strPtr(info->name));

    // Skip excluded files/links/paths
    if (buildData.excludeSingle != NULL && strLstExists(buildData.excludeSingle, manifestName))
    {
        LOG_INFO_FMT(
            "exclude '%s/%s' from backup using '%s' exclusion", strPtr(buildData.pgPath), strPtr(info->name),
            strPtr(strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA))));

        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Process storage types
    switch (info->type)
    {
        // Add paths
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypePath:
        {
            // There should not be any paths in pg_tblspc
            if (strEqZ(buildData.manifestParentName, MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC))
            {
                THROW_FMT(
                    LinkExpectedError, "'%s' is not a symlink - " MANIFEST_TARGET_PGTBLSPC " should contain only symlinks",
                    strPtr(manifestName));
            }

            // Add path to manifest
            ManifestPath path =
            {
                .name = manifestName,
                .mode = info->mode,
                .user = info->user,
                .group = info->group,
            };

            manifestPathAdd(buildData.manifest, &path);

            // Skip excluded path content
            if (buildData.excludeContent != NULL && strLstExists(buildData.excludeContent, manifestName))
            {
                LOG_INFO_FMT(
                    "exclude contents of '%s/%s' from backup using '%s/' exclusion", strPtr(buildData.pgPath), strPtr(info->name),
                    strPtr(strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA))));

                FUNCTION_TEST_RETURN_VOID();
                return;
            }

            // Skip the contents of these paths if they exist in the base path since they won't be reused after recovery
            if (strEq(buildData.manifestParentName, MANIFEST_TARGET_PGDATA_STR))
            {
                // Skip pg_dynshmem/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGDYNSHMEM) && pgVersion >= PG_VERSION_94)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip pg_notify/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGNOTIFY) && pgVersion >= PG_VERSION_90)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip pg_replslot/* since these files are generally not useful after a restore
                if (strEqZ(info->name, PG_PATH_PGREPLSLOT) && pgVersion >= PG_VERSION_94)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip pg_serial/* since these files are reset
                if (strEqZ(info->name, PG_PATH_PGSERIAL) && pgVersion >= PG_VERSION_91)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip pg_snapshots/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGSNAPSHOTS) && pgVersion >= PG_VERSION_92)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip temporary statistics in pg_stat_tmp even when stats_temp_directory is set because PGSS_TEXT_FILE is always
                // created there
                if (strEqZ(info->name, PG_PATH_PGSTATTMP) && pgVersion >= PG_VERSION_84)
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }

                // Skip pg_subtrans/* since these files are reset
                if (strEqZ(info->name, PG_PATH_PGSUBTRANS))
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }
            }

            // Skip the contents of archive_status when online
            if (buildData.online && strEq(buildData.manifestParentName, buildData.manifestWalName) &&
                strEqZ(info->name, PG_PATH_ARCHIVE_STATUS))
            {
                FUNCTION_TEST_RETURN_VOID();
                return;
            }

            // Recurse into the path
            ManifestBuildData buildDataSub = buildData;
            buildDataSub.manifestParentName = manifestName;
            buildDataSub.pgPath = strNewFmt("%s/%s", strPtr(buildData.pgPath), strPtr(info->name));

            if (buildData.dbPathExp != NULL)
                buildDataSub.dbPath = regExpMatch(buildData.dbPathExp, manifestName);

            storageInfoListP(
                buildDataSub.storagePg, buildDataSub.pgPath, manifestBuildCallback, &buildDataSub, .sortOrder = sortOrderAsc);

            break;
        }

        // Add files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeFile:
        {
            // There should not be any files in pg_tblspc
            if (strEqZ(buildData.manifestParentName, MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC))
            {
                THROW_FMT(
                    LinkExpectedError, "'%s' is not a symlink - " MANIFEST_TARGET_PGTBLSPC " should contain only symlinks",
                    strPtr(manifestName));
            }

            // Skip pg_internal.init since it is recreated on startup.  It's also possible, (though unlikely) that a temp file with
            // the creating process id as the extension can exist so skip that as well.  This seems to be a bug in PostgreSQL since
            // the temp file should be removed on startup.  Use regExpMatchOne() here instead of preparing a regexp in advance since
            // the likelihood of needing the regexp should be very small.
            if ((pgVersion <= PG_VERSION_84 || buildData.dbPath) && strBeginsWithZ(info->name, PG_FILE_PGINTERNALINIT) &&
                (strSize(info->name) == sizeof(PG_FILE_PGINTERNALINIT) - 1 ||
                    regExpMatchOne(STRDEF("\\.[0-9]+"), strSub(info->name, sizeof(PG_FILE_PGINTERNALINIT) - 1))))
            {
                FUNCTION_TEST_RETURN_VOID();
                return;
            }

            // Skip files in the root data path
            if (strEq(buildData.manifestParentName, MANIFEST_TARGET_PGDATA_STR))
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
                    // Skip running process options
                    strEqZ(info->name, PG_FILE_POSTMASTEROPTS) ||
                    // Skip process id file to avoid confusing postgres after restore
                    strEqZ(info->name, PG_FILE_POSTMASTERPID))
                {
                    FUNCTION_TEST_RETURN_VOID();
                    return;
                }
            }

            // Skip the contents of the wal path when online. WAL will be restored from the archive or stored in the wal directory
            // at the end of the backup if the archive-copy option is set.
            if (buildData.online && strEq(buildData.manifestParentName, buildData.manifestWalName))
            {
                FUNCTION_TEST_RETURN_VOID();
                return;
            }

            // Skip temp relations in db paths
            if (buildData.dbPath && regExpMatch(buildData.tempRelationExp, info->name))
            {
                FUNCTION_TEST_RETURN_VOID();
                return;
            }

            // Add file to manifest
            ManifestFile file =
            {
                .name = manifestName,
                .mode = info->mode,
                .user = info->user,
                .group = info->group,
                .size = info->size,
                .sizeRepo = info->size,
                .timestamp = info->timeModified,
            };

            // Set a flag to indicate if this file must be copied from the primary
            file.primary =
                strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL) ||
                !regExpMatch(buildData.standbyExp, manifestName);

            // Determine if this file should be page checksummed
            if (buildData.dbPath && buildData.checksumPage)
            {
                file.checksumPage =
                    !strEndsWithZ(manifestName, "/" PG_FILE_PGFILENODEMAP) && !strEndsWithZ(manifestName, "/" PG_FILE_PGVERSION) &&
                    !strEqZ(manifestName, MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);
            }

            manifestFileAdd(buildData.manifest, &file);
            break;
        }

        // Add links
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeLink:
        {
            // If the destination is another link then error.  In the future we'll allow this by following the link chain to the
            // eventual destination but for now we are trying to maintain compatibility during the migration.  To do this check we
            // need to read outside of the data directory but it is a read-only operation so is considered safe.
            const String *linkDestinationAbsolute = strPathAbsolute(info->linkDestination, buildData.pgPath);

            StorageInfo linkedCheck = storageInfoP(
                buildData.storagePg, linkDestinationAbsolute, .ignoreMissing = true, .noPathEnforce = true);

            if (linkedCheck.exists && linkedCheck.type == storageTypeLink)
            {
                THROW_FMT(
                    LinkDestinationError, "link '%s/%s' cannot reference another link '%s'", strPtr(buildData.pgPath),
                    strPtr(info->name), strPtr(linkDestinationAbsolute));
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
            if (strEq(buildData.manifestParentName, STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC)))
            {
                // Strip pg_data off the manifest name so it begins with pg_tblspc instead.  This reflects how the files are stored
                // in the backup directory.
                manifestName = strSub(manifestName, sizeof(MANIFEST_TARGET_PGDATA));

                // Identify this target as a tablespace
                target.name = manifestName;
                target.tablespaceId = cvtZToUInt(strPtr(info->name));

                // Look for this tablespace in the provided list (list may be null for off-line backup)
                if (buildData.tablespaceList != NULL)
                {
                    // Search list
                    for (unsigned int tablespaceIdx = 0; tablespaceIdx < varLstSize(buildData.tablespaceList); tablespaceIdx++)
                    {
                        const VariantList *tablespace = varVarLst(varLstGet(buildData.tablespaceList, tablespaceIdx));

                        if (target.tablespaceId == varUIntForce(varLstGet(tablespace, 0)))
                            target.tablespaceName = varStr(varLstGet(tablespace, 1));
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
                    target.tablespaceName = strNewFmt("ts%s", strPtr(info->name));

                // Add a dummy pg_tblspc path entry if it does not already exist.  This entry will be ignored by restore but it is
                // part of the original manifest format so we need to have it.
                lstSort(buildData.manifest->pathList, sortOrderAsc);
                const ManifestPath *pathBase = manifestPathFind(buildData.manifest, MANIFEST_TARGET_PGDATA_STR);

                if (manifestPathFindDefault(buildData.manifest, MANIFEST_TARGET_PGTBLSPC_STR, NULL) == NULL)
                {
                    ManifestPath path =
                    {
                        .name = MANIFEST_TARGET_PGTBLSPC_STR,
                        .mode = pathBase->mode,
                        .user = pathBase->user,
                        .group = pathBase->group,
                    };

                    manifestPathAdd(buildData.manifest, &path);
                }

                // If the tablespace id is present then the tablespace link destination path is not the path where data will be
                // stored so we can just store it as dummy path.
                if (buildData.tablespaceId != NULL)
                {
                    const ManifestPath *pathTblSpc = manifestPathFind(
                        buildData.manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC));

                    ManifestPath path =
                    {
                        .name = manifestName,
                        .mode = pathTblSpc->mode,
                        .user = pathTblSpc->user,
                        .group = pathTblSpc->group,
                    };

                    manifestPathAdd(buildData.manifest, &path);

                    // Update build structure to reflect the path added above and the tablespace id
                    buildData.manifestParentName = manifestName;
                    manifestName = strNewFmt("%s/%s", strPtr(manifestName), strPtr(buildData.tablespaceId));
                    buildData.pgPath = strNewFmt("%s/%s", strPtr(buildData.pgPath), strPtr(info->name));
                    linkName = buildData.tablespaceId;
                }
                // If no tablespace id then parent manifest name is the tablespace directory
                else
                    buildData.manifestParentName = MANIFEST_TARGET_PGTBLSPC_STR;
            }

            // Add info about the linked file/path
            const String *linkPgPath = strNewFmt("%s/%s", strPtr(buildData.pgPath), strPtr(linkName));
            StorageInfo linkedInfo = storageInfoP(
                buildData.storagePg, linkPgPath, .followLink = true, .ignoreMissing = true);
            linkedInfo.name = linkName;

            // If the link destination exists then proceed as usual
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
                    // Tablespace links should never be to a file
                    CHECK(target.tablespaceId == 0);

                    // Identify target as a file
                    target.path = strPath(info->linkDestination);
                    target.file = strBase(info->linkDestination);
                }

                // Use the callback to add and do all related checks
                manifestBuildCallback(&buildData, &linkedInfo);
            }
            // Else dummy up the target with a destination so manifestLinkCheck() can be run.  This is so errors about links with
            // destinations in PGDATA will take precedence over missing a destination.  We will probably simplify this once the
            // migration is done and it doesn't matter which error takes precedence.
            else
                target.path = info->linkDestination;

            // Add target and link
            manifestTargetAdd(buildData.manifest, &target);
            manifestLinkAdd(buildData.manifest, &link);

            // Make sure the link is valid
            manifestLinkCheck(buildData.manifest);

            // If the link check was successful but the destination does not exist then check it again to generate an error
            if (!linkedInfo.exists)
                storageInfoP(buildData.storagePg, linkPgPath, .followLink = true);

            break;
        }

        // Skip special files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeSpecial:
        {
            LOG_WARN_FMT("exclude special file '%s/%s' from backup", strPtr(buildData.pgPath), strPtr(info->name));
            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

// Regular expression constants
#define RELATION_EXP                                                "[0-9]+(_(fsm|vm)){0,1}(\\.[0-9]+){0,1}"
#define DB_PATH_EXP                                                                                                                \
    "(" MANIFEST_TARGET_PGDATA "/(" PG_PATH_GLOBAL "|" PG_PATH_BASE "/[0-9]+)|" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+)"

Manifest *
manifestNewBuild(
    const Storage *storagePg, unsigned int pgVersion, bool online, bool checksumPage, const StringList *excludeList,
    const VariantList *tablespaceList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(BOOL, online);
        FUNCTION_LOG_PARAM(BOOL, checksumPage);
        FUNCTION_LOG_PARAM(STRING_LIST, excludeList);
        FUNCTION_LOG_PARAM(VARIANT_LIST, tablespaceList);
    FUNCTION_LOG_END();

    ASSERT(storagePg != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(!checksumPage || pgVersion >= PG_VERSION_93);

    Manifest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("Manifest")
    {
        this = manifestNewInternal();
        this->info = infoNew(NULL);
        this->data.backrestVersion = strNew(PROJECT_VERSION);
        this->data.pgVersion = pgVersion;
        this->data.backupOptionOnline = online;
        this->data.backupOptionChecksumPage = varNewBool(checksumPage);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Data needed to build the manifest
            ManifestBuildData buildData =
            {
                .manifest = this,
                .storagePg = storagePg,
                .tablespaceId = pgTablespaceId(pgVersion),
                .online = online,
                .checksumPage = checksumPage,
                .tablespaceList = tablespaceList,
                .manifestParentName = MANIFEST_TARGET_PGDATA_STR,
                .manifestWalName = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strPtr(pgWalPath(pgVersion))),
                .pgPath = storagePathP(storagePg, NULL),
            };

            // We won't identify db paths for PostgreSQL < 9.0.  This means that temp relations will not be excluded but it doesn't
            // seem worth supporting this feature on such old versions of PostgreSQL.
            // ---------------------------------------------------------------------------------------------------------------------
            if (pgVersion >= PG_VERSION_90)
            {
                ASSERT(buildData.tablespaceId != NULL);

                // Expression to identify database paths
                buildData.dbPathExp = regExpNew(strNewFmt("^" DB_PATH_EXP "$", strPtr(buildData.tablespaceId)));

                // Expression to find temp relations
                buildData.tempRelationExp = regExpNew(STRDEF("^t[0-9]+_" RELATION_EXP "$"));
            }

            // Build expression to identify files that can be copied from the standby when standby backup is supported
            // ---------------------------------------------------------------------------------------------------------------------
            buildData.standbyExp = regExpNew(
                strNewFmt(
                    "^((" MANIFEST_TARGET_PGDATA "/(" PG_PATH_BASE "|" PG_PATH_GLOBAL "|%s|" PG_PATH_PGMULTIXACT "))|"
                        MANIFEST_TARGET_PGTBLSPC ")/",
                    strPtr(pgXactPath(pgVersion))));

            // Build list of exclusions
            // ---------------------------------------------------------------------------------------------------------------------
            if (excludeList != NULL)
            {
                for (unsigned int excludeIdx = 0; excludeIdx < strLstSize(excludeList); excludeIdx++)
                {
                    const String *exclude = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strPtr(strLstGet(excludeList, excludeIdx)));

                    // If the exclusions refers to the contents of a path
                    if (strEndsWithZ(exclude, "/"))
                    {
                        if (buildData.excludeContent == NULL)
                            buildData.excludeContent = strLstNew();

                        strLstAdd(buildData.excludeContent, strSubN(exclude, 0, strSize(exclude) - 1));
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
            StorageInfo info = storageInfoP(storagePg, buildData.pgPath, .followLink = true);

            ManifestPath path =
            {
                .name = MANIFEST_TARGET_PGDATA_STR,
                .mode = info.mode,
                .user = info.user,
                .group = info.group,
            };

            manifestPathAdd(this, &path);

            ManifestTarget target =
            {
                .name = MANIFEST_TARGET_PGDATA_STR,
                .path = buildData.pgPath,
                .type = manifestTargetTypePath,
            };

            manifestTargetAdd(this, &target);

            // Gather info for the rest of the files/links/paths
            storageInfoListP(
                storagePg, buildData.pgPath, manifestBuildCallback, &buildData, .errorOnMissing = true, .sortOrder = sortOrderAsc);

            // These may not be in order even if the incoming data was sorted
            lstSort(this->fileList, sortOrderAsc);
            lstSort(this->linkList, sortOrderAsc);
            lstSort(this->pathList, sortOrderAsc);
            lstSort(this->targetList, sortOrderAsc);

            // Remove unlogged relations from the manifest.  This can't be done during the initial build because of the requirement
            // to check for _init files which will sort after the vast majority of the relation files.  We could check storage for
            // each _init file but that would be expensive.
            // -------------------------------------------------------------------------------------------------------------------------
            if (pgVersion >= PG_VERSION_91)
            {
                RegExp *relationExp = regExpNew(strNewFmt("^" DB_PATH_EXP "/" RELATION_EXP "$", strPtr(buildData.tablespaceId)));
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
                    const ManifestFile *file = manifestFile(this, fileIdx);

                    if (regExpMatch(relationExp, file->name))
                    {
                        // Get the filename (without path)
                        const char *fileName = strBaseZ(file->name);
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
                                "%.*s%s_init", (int)(strSize(file->name) - fileNameSize), strPtr(file->name), relationFileId);
                            lastRelationFileIdUnlogged = manifestFileFindDefault(this, relationInit, NULL) != NULL;
                            strFree(relationInit);

                            // Save the file id so we don't need to do the lookup next time if if doesn't change
                            strcpy(lastRelationFileId, relationFileId);
                        }

                        // If relation is unlogged then remove it
                        if (lastRelationFileIdUnlogged)
                        {
                            manifestFileRemove(this, file->name);
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
    MEM_CONTEXT_NEW_END();

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

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Store the delta option.  If true we can skip checks that automatically enable delta.
        this->data.backupOptionDelta = varNewBool(delta);

        // If online then add one second to the copy start time to allow for database updates during the last second that the
        // manifest was being built.  It's up to the caller to actually wait the remainder of the second, but for comparison
        // purposes we want the time when the waiting started.
        this->data.backupTimestampCopyStart = copyStart + (this->data.backupOptionOnline ? 1 : 0);

        // This value is not needed in this function, but it is needed for resumed manifests and this is last place to set it before
        // processing begins
        this->data.backupOptionCompressType = compressType;
    }
    MEM_CONTEXT_END();

    // Check the manifest for timestamp anomalies that require a delta backup (if delta is not already specified)
    if (!varBool(this->data.backupOptionDelta))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
            {
                const ManifestFile *file = manifestFile(this, fileIdx);

                // Check for timestamp in the future
                if (file->timestamp > copyStart)
                {
                    LOG_WARN_FMT(
                        "file '%s' has timestamp in the future, enabling delta checksum", strPtr(manifestPathPg(file->name)));

                    this->data.backupOptionDelta = BOOL_TRUE_VAR;
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
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(STRING, archiveStart);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(manifestPrior != NULL);
    ASSERT(type == backupTypeDiff || type == backupTypeIncr);
    ASSERT(type != backupTypeDiff || manifestPrior->data.backupType == backupTypeFull);
    ASSERT(archiveStart == NULL || strSize(archiveStart) == 24);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Set prior backup label
        this->data.backupLabelPrior = strDup(manifestPrior->data.backupLabel);

        // Set diff/incr backup type
        this->data.backupType = type;
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
                strPtr(manifestData(manifestPrior)->backupLabel));

            this->data.backupOptionDelta = BOOL_TRUE_VAR;
        }
        // Else enable delta if online differs
        else if (manifestData(manifestPrior)->backupOptionOnline != this->data.backupOptionOnline)
        {
            LOG_WARN_FMT(
                "the online option has changed since the %s backup, enabling delta checksum",
                strPtr(manifestData(manifestPrior)->backupLabel));

            this->data.backupOptionDelta = BOOL_TRUE_VAR;
        }

        // Check for anomalies between manifests if delta is not already enabled.  This can't be combined with the main comparison
        // loop below because delta changes the behavior of that loop.
        if (!varBool(this->data.backupOptionDelta))
        {
            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
            {
                const ManifestFile *file = manifestFile(this, fileIdx);
                const ManifestFile *filePrior = manifestFileFindDefault(manifestPrior, file->name, NULL);

                // If file was found in prior manifest then perform checks
                if (filePrior != NULL)
                {
                    // Check for timestamp earlier than the prior backup
                    if (file->timestamp < filePrior->timestamp)
                    {
                        LOG_WARN_FMT(
                            "file '%s' has timestamp earlier than prior backup, enabling delta checksum",
                            strPtr(manifestPathPg(file->name)));

                        this->data.backupOptionDelta = BOOL_TRUE_VAR;
                        break;
                    }

                    // Check for size change with no timestamp change
                    if (file->size != filePrior->size && file->timestamp == filePrior->timestamp)
                    {
                        LOG_WARN_FMT(
                            "file '%s' has same timestamp as prior but different size, enabling delta checksum",
                            strPtr(manifestPathPg(file->name)));

                        this->data.backupOptionDelta = BOOL_TRUE_VAR;
                        break;
                    }
                }
            }
        }

        // Find files to reference in the prior manifest:
        // 1) that don't need to be copied because delta is disabled and the size and timestamp match or size matches and is zero
        // 2) where delta is enabled and size matches so checksum will be verified during backup and the file copied on mismatch
        bool delta = varBool(this->data.backupOptionDelta);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(this->fileList); fileIdx++)
        {
            const ManifestFile *file = manifestFile(this, fileIdx);
            const ManifestFile *filePrior = manifestFileFindDefault(manifestPrior, file->name, NULL);

            // Check if prior file can be used
            if (filePrior != NULL && file->size == filePrior->size &&
                (delta || file->size == 0 || file->timestamp == filePrior->timestamp))
            {
                manifestFileUpdate(
                    this, file->name, file->size, filePrior->sizeRepo, filePrior->checksumSha1,
                    VARSTR(filePrior->reference != NULL ? filePrior->reference : manifestPrior->data.backupLabel),
                    filePrior->checksumPage, filePrior->checksumPageError, filePrior->checksumPageErrorList);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
manifestBuildComplete(
    Manifest *this, time_t timestampStart, const String *lsnStart, const String *archiveStart, time_t timestampStop,
    const String *lsnStop, const String *archiveStop, unsigned int pgId, uint64_t pgSystemId, const VariantList *dbList,
    bool optionArchiveCheck, bool optionArchiveCopy, size_t optionBufferSize, unsigned int optionCompressLevel,
    unsigned int optionCompressLevelNetwork, bool optionHardLink, unsigned int optionProcessMax,
    bool optionStandby)
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
        FUNCTION_LOG_PARAM(VARIANT_LIST, dbList);
        FUNCTION_LOG_PARAM(BOOL, optionArchiveCheck);
        FUNCTION_LOG_PARAM(BOOL, optionArchiveCopy);
        FUNCTION_LOG_PARAM(SIZE, optionBufferSize);
        FUNCTION_LOG_PARAM(UINT, optionCompressLevel);
        FUNCTION_LOG_PARAM(UINT, optionCompressLevelNetwork);
        FUNCTION_LOG_PARAM(BOOL, optionHardLink);
        FUNCTION_LOG_PARAM(UINT, optionProcessMax);
        FUNCTION_LOG_PARAM(BOOL, optionStandby);
    FUNCTION_LOG_END();

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Save info
        this->data.backupTimestampStart = timestampStart;
        this->data.lsnStart = strDup(lsnStart);
        this->data.archiveStart = strDup(archiveStart);
        this->data.backupTimestampStop = timestampStop;
        this->data.lsnStop = strDup(lsnStop);
        this->data.archiveStop = strDup(archiveStop);
        this->data.pgId = pgId;
        this->data.pgSystemId = pgSystemId;

        // Save db list
        if (dbList != NULL)
        {
            for (unsigned int dbIdx = 0; dbIdx < varLstSize(dbList); dbIdx++)
            {
                const VariantList *dbRow = varVarLst(varLstGet(dbList, dbIdx));

                ManifestDb db =
                {
                    .id = varUIntForce(varLstGet(dbRow, 0)),
                    .name = varStr(varLstGet(dbRow, 1)),
                    .lastSystemId = varUIntForce(varLstGet(dbRow, 2)),
                };

                manifestDbAdd(this, &db);
            }

            lstSort(this->dbList, sortOrderAsc);
        }

        // Save options
        this->data.backupOptionArchiveCheck = optionArchiveCheck;
        this->data.backupOptionArchiveCopy = optionArchiveCopy;
        this->data.backupOptionBufferSize = varNewUInt64(optionBufferSize);
        this->data.backupOptionCompressLevel = varNewUInt(optionCompressLevel);
        this->data.backupOptionCompressLevelNetwork = varNewUInt(optionCompressLevelNetwork);
        this->data.backupOptionHardLink = optionHardLink;
        this->data.backupOptionProcessMax = varNewUInt(optionProcessMax);
        this->data.backupOptionStandby = varNewBool(optionStandby);
    }
    MEM_CONTEXT_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
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
        CHECK(!varBool(owner));
        FUNCTION_TEST_RETURN(NULL);
    }

    FUNCTION_TEST_RETURN(varStr(owner));
}

// Helper to convert default owner to a variant.  Input could be boolean false (meaning there is no owner) or a string (there is an
// owner).
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
            };

            // Timestamp is required so error if it is not present
            const Variant *timestamp = kvGet(fileKv, MANIFEST_KEY_TIMESTAMP_VAR);

            if (timestamp == NULL)
                THROW_FMT(FormatError, "missing timestamp for file '%s'", strPtr(key));

            file.timestamp = (time_t)varUInt64(timestamp);

            // Size is required so error if it is not present.  Older versions removed the size before the backup to ensure that the
            // manifest was updated during the backup, so size can be missing in partial manifests.  This error will prevent older
            // partials from being resumed.
            const Variant *size = kvGet(fileKv, MANIFEST_KEY_SIZE_VAR);

            if (size == NULL)
                THROW_FMT(FormatError, "missing size for file '%s'", strPtr(key));

            file.size = varUInt64(size);

            // If "repo-size" is not present in the manifest file, then it is the same as size (i.e. uncompressed) - to save space,
            // the repo-size is only stored in the manifest file if it is different than size.
            file.sizeRepo = varUInt64(kvGetDefault(fileKv, MANIFEST_KEY_SIZE_REPO_VAR, VARUINT64(file.size)));

            // If file size is zero then assign the static zero hash
            if (file.size == 0)
            {
                memcpy(file.checksumSha1, HASH_TYPE_SHA1_ZERO, HASH_TYPE_SHA1_SIZE_HEX + 1);
            }
            // Else if the key exists then load it.  The key might not exist if this is a partial save that was done during the
            // backup to preserve checksums for already backed up files.
            else if (kvKeyExists(fileKv, MANIFEST_KEY_CHECKSUM_VAR))
            {
                memcpy(
                    file.checksumSha1, strPtr(varStr(kvGet(fileKv, MANIFEST_KEY_CHECKSUM_VAR))), HASH_TYPE_SHA1_SIZE_HEX + 1);
            }

            const Variant *checksumPage = kvGetDefault(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_VAR, NULL);

            if (checksumPage != NULL)
            {
                file.checksumPage = true;
                file.checksumPageError = !varBool(checksumPage);

                const Variant *checksumPageErrorList = kvGetDefault(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, NULL);

                if (checksumPageErrorList != NULL)
                    file.checksumPageErrorList = varVarLst(checksumPageErrorList);
            }

            if (kvKeyExists(fileKv, MANIFEST_KEY_GROUP_VAR))
            {
                valueFound.group = true;
                file.group = manifestOwnerGet(kvGet(fileKv, MANIFEST_KEY_GROUP_VAR));
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
                file.user = manifestOwnerGet(kvGet(fileKv, MANIFEST_KEY_USER_VAR));
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
                path.group = manifestOwnerGet(kvGet(pathKv, MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(pathKv, MANIFEST_KEY_MODE_VAR))
            {
                valueFound.mode = true;
                path.mode = cvtZToMode(strPtr(varStr(kvGet(pathKv, MANIFEST_KEY_MODE_VAR))));
            }

            if (kvKeyExists(pathKv, MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                path.user = manifestOwnerGet(kvGet(pathKv, MANIFEST_KEY_USER_VAR));
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
                link.group = manifestOwnerGet(kvGet(linkKv, MANIFEST_KEY_GROUP_VAR));
            }

            if (kvKeyExists(linkKv, MANIFEST_KEY_USER_VAR))
            {
                valueFound.user = true;
                link.user = manifestOwnerGet(kvGet(linkKv, MANIFEST_KEY_USER_VAR));
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
            // Historically this option meant to add gz compression
            else if (strEq(key, MANIFEST_KEY_OPTION_COMPRESS_STR))
                manifest->data.backupOptionCompressType = jsonToBool(value) ? compressTypeGz : compressTypeNone;
            // This new option allows any type of compression to be specified.  It must be parsed after the option above so the
            // value does not get overwritten.  Since options are stored in alpha order this should always be true.
            else if (strEq(key, MANIFEST_KEY_OPTION_COMPRESS_TYPE_STR))
                manifest->data.backupOptionCompressType = compressTypeEnum(jsonToStr(value));
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
            loadData.fileFoundList = lstNewP(sizeof(ManifestLoadFound));
            loadData.linkFoundList = lstNewP(sizeof(ManifestLoadFound));
            loadData.pathFoundList = lstNewP(sizeof(ManifestLoadFound));
        }
        MEM_CONTEXT_END();

        this->info = infoNewLoad(read, manifestLoadCallback, &loadData);
        this->data.backrestVersion = infoBackrestVersion(this->info);

        // Process file defaults
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
        {
            ManifestFile *file = lstGet(this->fileList, fileIdx);
            ManifestLoadFound *found = lstGet(loadData.fileFoundList, fileIdx);

            if (!found->group)
                file->group = manifestOwnerCache(this, manifestOwnerGet(loadData.fileGroupDefault));

            if (!found->mode)
                file->mode = loadData.fileModeDefault;

            if (!found->primary)
                file->primary = loadData.filePrimaryDefault;

            if (!found->user)
                file->user = manifestOwnerCache(this, manifestOwnerGet(loadData.fileUserDefault));
        }

        // Process link defaults
        for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(this); linkIdx++)
        {
            ManifestLink *link = lstGet(this->linkList, linkIdx);
            ManifestLoadFound *found = lstGet(loadData.linkFoundList, linkIdx);

            if (!found->group)
                link->group = manifestOwnerCache(this, manifestOwnerGet(loadData.linkGroupDefault));

            if (!found->user)
                link->user = manifestOwnerCache(this, manifestOwnerGet(loadData.linkUserDefault));
        }

        // Process path defaults
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(this); pathIdx++)
        {
            ManifestPath *path = lstGet(this->pathList, pathIdx);
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
        lstSort(this->dbList, sortOrderAsc);
        lstSort(this->fileList, sortOrderAsc);
        lstSort(this->linkList, sortOrderAsc);
        lstSort(this->pathList, sortOrderAsc);
        lstSort(this->targetList, sortOrderAsc);

        // Make sure the base path exists
        manifestTargetBase(this);

        // Discard the context holding temporary load data
        memContextDiscard();
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(MANIFEST, this);
}

/**********************************************************************************************************************************/
typedef struct ManifestSaveData
{
    Manifest *manifest;                                             // Manifest object to be saved

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
manifestOwnerVar(const String *ownerDefault)
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
                jsonFromVar(manifest->data.backupOptionStandby));
        }

        if (manifest->data.backupOptionBufferSize != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_BUFFER_SIZE_STR,
                jsonFromVar(manifest->data.backupOptionBufferSize));
        }

        if (manifest->data.backupOptionChecksumPage != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_CHECKSUM_PAGE_STR,
                jsonFromVar(manifest->data.backupOptionChecksumPage));
        }

        // Set the option when compression is turned on.  In older versions this also implied gz compression but in newer versions
        // the type option must also be set if compression is not gz.
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_STR,
            jsonFromBool(manifest->data.backupOptionCompressType != compressTypeNone));

        if (manifest->data.backupOptionCompressLevel != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_STR,
                jsonFromVar(manifest->data.backupOptionCompressLevel));
        }

        if (manifest->data.backupOptionCompressLevelNetwork != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_LEVEL_NETWORK_STR,
                jsonFromVar(manifest->data.backupOptionCompressLevelNetwork));
        }

        // Set the compression type.  Older versions will ignore this and assume gz compression if the compress option is set.
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_COMPRESS_TYPE_STR,
            jsonFromStr(compressTypeStr(manifest->data.backupOptionCompressType)));

        if (manifest->data.backupOptionDelta != NULL)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_BACKUP_OPTION_STR, MANIFEST_KEY_OPTION_DELTA_STR,
                jsonFromVar(manifest->data.backupOptionDelta));
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
                jsonFromVar(manifest->data.backupOptionProcessMax));
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

                infoSaveValue(infoSaveData, MANIFEST_SECTION_BACKUP_TARGET_STR, target->name, jsonFromKv(targetKv));

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

                infoSaveValue(infoSaveData, MANIFEST_SECTION_DB_STR, db->name, jsonFromKv(dbKv));

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

                // Save if the file size is not zero and the checksum exists.  The checksum might not exist if this is a partial
                // save performed during a backup.
                if (file->size != 0 && file->checksumSha1[0] != 0)
                    kvPut(fileKv, MANIFEST_KEY_CHECKSUM_VAR, VARSTRZ(file->checksumSha1));

                if (file->checksumPage)
                {
                    kvPut(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_VAR, VARBOOL(!file->checksumPageError));

                    if (file->checksumPageErrorList != NULL)
                        kvPut(fileKv, MANIFEST_KEY_CHECKSUM_PAGE_ERROR_VAR, varNewVarLst(file->checksumPageErrorList));
                }

                if (!varEq(manifestOwnerVar(file->group), saveData->fileGroupDefault))
                    kvPut(fileKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerVar(file->group));

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

                if (!varEq(manifestOwnerVar(file->user), saveData->fileUserDefault))
                    kvPut(fileKv, MANIFEST_KEY_USER_VAR, manifestOwnerVar(file->user));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_FILE_STR, file->name, jsonFromKv(fileKv));

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
            jsonFromVar(saveData->fileGroupDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_PRIMARY_STR,
            jsonFromBool(saveData->filePrimaryDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->fileModeDefault)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_FILE_DEFAULT_STR, MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->fileUserDefault));
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

                if (!varEq(manifestOwnerVar(link->user), saveData->linkUserDefault))
                    kvPut(linkKv, MANIFEST_KEY_USER_VAR, manifestOwnerVar(link->user));

                if (!varEq(manifestOwnerVar(link->group), saveData->linkGroupDefault))
                    kvPut(linkKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerVar(link->group));

                kvPut(linkKv, MANIFEST_KEY_DESTINATION_VAR, VARSTR(link->destination));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_LINK_STR, link->name, jsonFromKv(linkKv));

                MEM_CONTEXT_TEMP_RESET(1000);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (infoSaveSection(infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, sectionNext))
    {
        if (manifestLinkTotal(manifest) > 0)
        {
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, MANIFEST_KEY_GROUP_STR,
                jsonFromVar(saveData->linkGroupDefault));
            infoSaveValue(
                infoSaveData, MANIFEST_SECTION_TARGET_LINK_DEFAULT_STR, MANIFEST_KEY_USER_STR,
                jsonFromVar(saveData->linkUserDefault));
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

                if (!varEq(manifestOwnerVar(path->group), saveData->pathGroupDefault))
                    kvPut(pathKv, MANIFEST_KEY_GROUP_VAR, manifestOwnerVar(path->group));

                if (path->mode != saveData->pathModeDefault)
                    kvPut(pathKv, MANIFEST_KEY_MODE_VAR, VARSTR(strNewFmt("%04o", path->mode)));

                if (!varEq(manifestOwnerVar(path->user), saveData->pathUserDefault))
                    kvPut(pathKv, MANIFEST_KEY_USER_VAR, manifestOwnerVar(path->user));

                infoSaveValue(infoSaveData, MANIFEST_SECTION_TARGET_PATH_STR, path->name, jsonFromKv(pathKv));

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
            jsonFromVar(saveData->pathGroupDefault));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, MANIFEST_KEY_MODE_STR,
            jsonFromStr(strNewFmt("%04o", saveData->pathModeDefault)));
        infoSaveValue(
            infoSaveData, MANIFEST_SECTION_TARGET_PATH_DEFAULT_STR, MANIFEST_KEY_USER_STR,
            jsonFromVar(saveData->pathUserDefault));
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
        lstSort(this->fileList, sortOrderAsc);

        ManifestSaveData saveData =
        {
            .manifest = this,
        };

        // Get default file values
        MostCommonValue *fileGroupMcv = mcvNew();
        MostCommonValue *fileModeMcv = mcvNew();
        MostCommonValue *filePrimaryMcv = mcvNew();
        MostCommonValue *fileUserMcv = mcvNew();

        ASSERT(manifestFileTotal(this) > 0);

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
        {
            const ManifestFile *file = manifestFile(this, fileIdx);

            mcvUpdate(fileGroupMcv, VARSTR(file->group));
            mcvUpdate(fileModeMcv, VARUINT(file->mode));
            mcvUpdate(filePrimaryMcv, VARBOOL(file->primary));
            mcvUpdate(fileUserMcv, VARSTR(file->user));
        }

        saveData.fileGroupDefault = manifestOwnerVar(varStr(mcvResult(fileGroupMcv)));
        saveData.fileModeDefault = varUInt(mcvResult(fileModeMcv));
        saveData.filePrimaryDefault = varBool(mcvResult(filePrimaryMcv));
        saveData.fileUserDefault = manifestOwnerVar(varStr(mcvResult(fileUserMcv)));

        // Get default link values
        if (manifestLinkTotal(this) > 0)
        {
            MostCommonValue *linkGroupMcv = mcvNew();
            MostCommonValue *linkUserMcv = mcvNew();

            for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(this); linkIdx++)
            {
                const ManifestLink *link = manifestLink(this, linkIdx);

                mcvUpdate(linkGroupMcv, VARSTR(link->group));
                mcvUpdate(linkUserMcv, VARSTR(link->user));
            }

            saveData.linkGroupDefault = manifestOwnerVar(varStr(mcvResult(linkGroupMcv)));
            saveData.linkUserDefault = manifestOwnerVar(varStr(mcvResult(linkUserMcv)));
        }

        // Get default path values
        MostCommonValue *pathGroupMcv = mcvNew();
        MostCommonValue *pathModeMcv = mcvNew();
        MostCommonValue *pathUserMcv = mcvNew();

        ASSERT(manifestPathTotal(this) > 0);

        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(this); pathIdx++)
        {
            const ManifestPath *path = manifestPath(this, pathIdx);

            mcvUpdate(pathGroupMcv, VARSTR(path->group));
            mcvUpdate(pathModeMcv, VARUINT(path->mode));
            mcvUpdate(pathUserMcv, VARSTR(path->user));
        }

        saveData.pathGroupDefault = manifestOwnerVar(varStr(mcvResult(pathGroupMcv)));
        saveData.pathModeDefault = varUInt(mcvResult(pathModeMcv));
        saveData.pathUserDefault = manifestOwnerVar(varStr(mcvResult(pathUserMcv)));

        // Save manifest
        infoSave(this->info, write, manifestSaveCallback, &saveData);
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
        String *error = strNew("");

        // Validate files
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(this); fileIdx++)
        {
            const ManifestFile *file = manifestFile(this, fileIdx);

            // All files must have a checksum
            if (file->checksumSha1[0] == '\0')
                strCatFmt(error, "\nmissing checksum for file '%s'", strPtr(file->name));

            // These are strict checks to be performed only after a backup and before the final manifest save
            if (strict)
            {
                // Zero-length files must have a specific checksum
                if (file->size == 0 && !strEqZ(HASH_TYPE_SHA1_ZERO_STR, file->checksumSha1))
                    strCatFmt(error, "\ninvalid checksum '%s' for zero size file '%s'", file->checksumSha1, strPtr(file->name));

                // Non-zero size files must have non-zero repo size
                if (file->sizeRepo == 0 && file->size != 0)
                    strCatFmt(error, "\nrepo size must be > 0 for file '%s'", strPtr(file->name));
            }
        }

        // Throw exception when there are errors
        if (strSize(error) > 0)
            THROW_FMT(FormatError, "manifest validation failed:%s", strPtr(error));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
manifestLinkCheck(const Manifest *this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    // Base path used for link checks
    const ManifestTarget *base = manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR);

    for (unsigned int linkIdx1 = 0; linkIdx1 < manifestTargetTotal(this); linkIdx1++)
    {
        const ManifestTarget *link1 = manifestTarget(this, linkIdx1);

        // Only compare links
        if (link1->type == manifestTargetTypeLink)
        {
            // Check that the link is not inside the base data path
            if (strBeginsWith(
                    strNewFmt("%s/", strPtr(manifestTargetPath(this, link1))),
                    strNewFmt("%s/", strPtr(manifestTargetPath(this, base)))))
            {
                THROW_FMT(
                    LinkDestinationError,
                    "link '%s' destination '%s' is in PGDATA",
                    strPtr(manifestPathPg(link1->name)), strPtr(manifestTargetPath(this, link1)));
            }

            // Check that no link is a subpath of another link
            for (unsigned int linkIdx2 = 0; linkIdx2 < manifestTargetTotal(this); linkIdx2++)
            {
                const ManifestTarget *link2 = manifestTarget(this, linkIdx2);

                if (link2->type == manifestTargetTypeLink && link1 != link2)
                {
                    // Don't check link1 against a file link. If link1 is a file there's no need to compare because they cannot have
                    // conflicting paths. If link1 is a path we want to make sure that the file link is not located within it but to
                    // do that we need to wait until the file link is link1 and the path link is link2, which happens on another
                    // interation of the outer loop.
                    if (link2->file != NULL)
                        continue;

                    if (strBeginsWith(
                            strNewFmt("%s/", strPtr(manifestTargetPath(this, link1))),
                            strNewFmt("%s/", strPtr(manifestTargetPath(this, link2)))))
                    {
                        THROW_FMT(
                            LinkDestinationError,
                            "link '%s' (%s) destination is a subdirectory of or the same directory as link '%s' (%s)",
                            strPtr(manifestPathPg(link1->name)), strPtr(manifestTargetPath(this, link1)),
                            strPtr(manifestPathPg(link2->name)), strPtr(manifestTargetPath(this, link2)));
                    }
                }
            }
        }
    }

    FUNCTION_LOG_RETURN_VOID();
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

// If the database requested is not found in the list, return the default passed rather than throw an error
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

// If the file requested is not found in the list, return the default passed rather than throw an error
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

void
manifestFileRemove(const Manifest *this, const String *name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    if (!lstRemove(this->fileList, &name))
        THROW_FMT(AssertError, "unable to remove '%s' from manifest file list", strPtr(name));

    FUNCTION_TEST_RETURN_VOID();
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

void
manifestFileUpdate(
    Manifest *this, const String *name, uint64_t size, uint64_t sizeRepo, const char *checksumSha1, const Variant *reference,
    bool checksumPage, bool checksumPageError, const VariantList *checksumPageErrorList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(UINT64, size);
        FUNCTION_TEST_PARAM(UINT64, sizeRepo);
        FUNCTION_TEST_PARAM(STRINGZ, checksumSha1);
        FUNCTION_TEST_PARAM(VARIANT, reference);
        FUNCTION_TEST_PARAM(BOOL, checksumPage);
        FUNCTION_TEST_PARAM(BOOL, checksumPageError);
        FUNCTION_TEST_PARAM(VARIANT_LIST, checksumPageErrorList);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);
    ASSERT(
        (!checksumPage && !checksumPageError && checksumPageErrorList == NULL) ||
        (checksumPage && !checksumPageError && checksumPageErrorList == NULL) || (checksumPage && checksumPageError));

    ManifestFile *file = (ManifestFile *)manifestFileFind(this, name);

    MEM_CONTEXT_BEGIN(lstMemContext(this->fileList))
    {
        // Update reference if set
        if (reference != NULL)
        {
            if (varStr(reference) == NULL)
                file->reference = NULL;
            else
                file->reference = strLstAddIfMissing(this->referenceList, varStr(reference));
        }

        // Update checksum if set
        if (checksumSha1 != NULL)
            memcpy(file->checksumSha1, checksumSha1, HASH_TYPE_SHA1_SIZE_HEX + 1);

        // Update repo size
        file->size = size;
        file->sizeRepo = sizeRepo;

        // Update checksum page info
        file->checksumPage = checksumPage;
        file->checksumPageError = checksumPageError;
        file->checksumPageErrorList = varLstDup(checksumPageErrorList);
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN_VOID();
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

// If the link requested is not found in the list, return the default passed rather than throw an error
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

// If the path requested is not found in the list, return the default passed rather than throw an error
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
        FUNCTION_TEST_RETURN(strNew(strPtr(manifestPath) + sizeof(MANIFEST_TARGET_PGDATA)));
    }
    // Else not pg_data (this is faster since the length of everything else will be different than pg_data)
    else if (!strEq(manifestPath, MANIFEST_TARGET_PGDATA_STR))
    {
        // A tablespace target is the only valid option if not pg_data or pg_data/
        ASSERT(
            strEq(manifestPath, MANIFEST_TARGET_PGTBLSPC_STR) || strBeginsWith(manifestPath, STRDEF(MANIFEST_TARGET_PGTBLSPC "/")));

        FUNCTION_TEST_RETURN(strDup(manifestPath));
    }

    FUNCTION_TEST_RETURN(NULL);
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
manifestTargetBase(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR));
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

    // Construct it from the base pg path and a relative path
    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *pgPath = strPath(manifestPathPg(target->name));

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
Getters/Setters
***********************************************************************************************************************************/
const String *
manifestCipherSubPass(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoCipherPass(this->info));
}

void
manifestCipherSubPassSet(Manifest *this, const String *cipherSubPass)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, cipherSubPass);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    infoCipherPassSet(this->info, cipherSubPass);

    FUNCTION_TEST_RETURN_VOID();
}

const ManifestData *
manifestData(const Manifest *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(&this->data);
}

void
manifestBackupLabelSet(Manifest *this, const String *backupLabel)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, this);
        FUNCTION_TEST_PARAM(STRING, backupLabel);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        this->data.backupLabel = strDup(backupLabel);
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
        IoRead *read = storageReadIo(storageNewReadP(loadData->storage, fileName));
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
        const char *fileNamePath = strPtr(storagePathP(storage, fileName));

        infoLoad(
            strNewFmt("unable to load backup manifest file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
            manifestLoadFileCallback, &data);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, data.manifest);
}
