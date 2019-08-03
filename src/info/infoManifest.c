/***********************************************************************************************************************************
Manifest Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/list.h"
#include "info/infoManifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
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

STRING_STATIC(INFO_MANIFEST_PATH_PGDATA_STR,                        "pg_data");

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoManifest
{
    MemContext *memContext;                                         // Context that contains the InfoManifest
    unsigned int pgVersion;                                         // PostgreSQL version

    List *pathList;                                                 // List of paths
    List *fileList;                                                 // List of files
    List *linkList;                                                 // List of links
};

/***********************************************************************************************************************************
Build a new manifest for a PostgreSQL path
***********************************************************************************************************************************/
typedef struct InfoManifestBuildData
{
    InfoManifest *manifest;
    const Storage *storagePg;
    const InfoManifestPath *parentPathInfo;
    const String *pgPath;
} InfoManifestBuildData;

void infoManifestBuildCallback(InfoManifestBuildData *buildData, const StorageInfo *info)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, buildData);
        FUNCTION_LOG_PARAM(STORAGE_INFO, *storageInfo);
    FUNCTION_LOG_END();

    ASSERT(buildData != NULL);
    ASSERT(info != NULL);

    LOG_DETAIL("FOUND repoPath = '%s', name '%s'", strPtr(buildData->parentPathInfo->name), strPtr(info->name));

    // Skip all . paths because they have already been recorded on the previous level of recursion
    if (strEqZ(info->name, "."))
        return;

    // Skip any path/file/link that begins with pgsql_tmp.  The files are removed when the server is restarted and the directories
    // are recreated.
    if (strBeginsWithZ(info->name, PG_PREFIX_PGSQLTMP))
        return;

    // Process file types
    switch (info->type)
    {
        case storageTypePath:
        {
            // Add the path
            InfoManifestPath *pathInfo = NULL;

            MEM_CONTEXT_BEGIN(buildData->manifest->memContext)
            {
                pathInfo = memNew(sizeof(InfoManifestPath));
                pathInfo->name = strNewFmt("%s/%s", strPtr(buildData->parentPathInfo->name), strPtr(info->name));
                pathInfo->mode = info->mode;
                pathInfo->user = strDup(info->user);
                pathInfo->group = strDup(info->group);
            }
            MEM_CONTEXT_END();

            lstAdd(buildData->manifest->pathList, &pathInfo);

            LOG_DETAIL("    PATH name '%s'", strPtr(info->name));

            // Skip the contents of these paths if they exist in the base path since they won't be reused after recovery
            if (buildData->parentPathInfo->base)
            {
                if (strEqZ(info->name, PG_PATH_PGDYNSHMEM) && buildData->manifest->pgVersion >= PG_VERSION_94)
                    return;

                if (strEqZ(info->name, PG_PATH_PGNOTIFY))
                    return;

                if (strEqZ(info->name, PG_PATH_PGREPLSLOT) && buildData->manifest->pgVersion >= PG_VERSION_94)
                    return;

                if (strEqZ(info->name, PG_PATH_PGSERIAL) && buildData->manifest->pgVersion >= PG_VERSION_91)
                    return;

                if (strEqZ(info->name, PG_PATH_PGSNAPSHOTS) && buildData->manifest->pgVersion >= PG_VERSION_92)
                    return;

                if (strEqZ(info->name, PG_PATH_PGSTATTMP))
                    return;

                if (strEqZ(info->name, PG_PATH_PGSUBTRANS))
                    return;
            }

            // Recurse into the path
            InfoManifestBuildData buildDataSub = *buildData;
            buildDataSub.parentPathInfo = pathInfo;
            buildDataSub.pgPath = strNewFmt("%s/%s", strPtr(buildData->pgPath), strPtr(info->name));

            storageInfoListNP(
                buildDataSub.storagePg, buildDataSub.pgPath, (StorageInfoListCallback)infoManifestBuildCallback, &buildDataSub);

            break;
        }

        case storageTypeFile:
        {
            LOG_DETAIL("    FILE name '%s'", strPtr(info->name));

            break;
        }

        case storageTypeLink:
        {
            break;
        }
    }

    FUNCTION_LOG_RETURN_VOID();
}

static void
infoManifestBuild(InfoManifest *this, const Storage *storagePg)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_MANIFEST, this);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(storagePg != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the root path
        const String *pgPath = storagePathNP(storagePg, NULL);

        // Get info about the root path
        InfoManifestPath *pathInfo = NULL;

        MEM_CONTEXT_BEGIN(this->memContext)
        {
            StorageInfo info = storageInfoNP(storagePg, pgPath);

            pathInfo = memNew(sizeof(InfoManifestPath));
            pathInfo->name = INFO_MANIFEST_PATH_PGDATA_STR;
            pathInfo->base = true;
            pathInfo->mode = info.mode;
            pathInfo->user = strDup(info.user);
            pathInfo->group = strDup(info.group);
        }
        MEM_CONTEXT_END();

        lstAdd(this->pathList, &pathInfo);

        InfoManifestBuildData buildData =
        {
            .manifest = this,
            .storagePg = storagePg,
            .parentPathInfo = pathInfo,
            .pgPath = pgPath,
        };

        storageInfoListP(storagePg, pgPath, (StorageInfoListCallback)infoManifestBuildCallback, &buildData, .errorOnMissing = true);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Create new object
***********************************************************************************************************************************/
InfoManifest *
infoManifestNew(const Storage *storagePg, unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storagePg);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    ASSERT(storagePg != NULL);
    ASSERT(pgVersion >= PG_VERSION_90);

    InfoManifest *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoManifest")
    {
        // Create object
        this = memNew(sizeof(InfoManifest));
        this->memContext = MEM_CONTEXT_NEW();
        this->pgVersion = pgVersion;

        // Create lists
        this->pathList = lstNew(sizeof(InfoManifestPath));

        // Build the manifest
        infoManifestBuild(this, storagePg);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_MANIFEST, this);
}

/***********************************************************************************************************************************
Get a path from the list
***********************************************************************************************************************************/
// const InfoManifestPath *
// infoManifestPath(InfoManifest *this, unsigned int index)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
//         FUNCTION_TEST_PARAM(UINT, index);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//
//     FUNCTION_TEST_RETURN(*(InfoManifestPath **)lstGet(this->pathList, index));
// }


/***********************************************************************************************************************************
Get a path from the list
***********************************************************************************************************************************/
// unsigned int
// infoManifestPathSize(InfoManifest *this)
// {
//     FUNCTION_TEST_BEGIN();
//         FUNCTION_TEST_PARAM(INFO_MANIFEST, this);
//     FUNCTION_TEST_END();
//
//     ASSERT(this != NULL);
//
//     FUNCTION_TEST_RETURN(lstSize(this->pathList));
// }
