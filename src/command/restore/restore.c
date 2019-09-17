/***********************************************************************************************************************************
Restore Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <sys/stat.h>
#include <unistd.h>

#include "command/restore/restore.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/io/bufferWrite.h" // !!! REMOVE WITH MANIFEST TEST CODE
#include "common/log.h"
#include "common/regExp.h"
#include "common/user.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Recovery type constants
***********************************************************************************************************************************/
STRING_STATIC(RECOVERY_TYPE_PRESERVE_STR,                           "preserve");

/***********************************************************************************************************************************
Validate restore path
***********************************************************************************************************************************/
static void
restorePathValidate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // The PGDATA directory must exist
        // ??? We should remove this requirement in a separate commit.  What's the harm in creating the dir assuming we have perms?
        if (!storagePathExistsNP(storagePg(), NULL))
            THROW_FMT(PathMissingError, "$PGDATA directory '%s' does not exist", strPtr(cfgOptionStr(cfgOptPgPath)));

        // PostgreSQL must not be running
        if (storageExistsNP(storagePg(), PG_FILE_POSTMASTERPID_STR))
        {
            THROW_FMT(
                PostmasterRunningError,
                "unable to restore while PostgreSQL is running\n"
                    "HINT: presence of '" PG_FILE_POSTMASTERPID "' in '%s' indicates PostgreSQL is running.\n"
                    "HINT: remove '" PG_FILE_POSTMASTERPID "' only if PostgreSQL is not running.",
                strPtr(cfgOptionStr(cfgOptPgPath)));
        }

        // If the restore will be destructive attempt to verify that PGDATA looks like a valid PostgreSQL directory
        if ((cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce)) &&
            !storageExistsNP(storagePg(), PG_FILE_PGVERSION_STR) && !storageExistsNP(storagePg(), MANIFEST_FILE_STR))
        {
            LOG_WARN(
                "--delta or --force specified but unable to find '" PG_FILE_PGVERSION "' or '" MANIFEST_FILE "' in '%s' to"
                    " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                    " exist in the destination directories the restore will be aborted.",
               strPtr(cfgOptionStr(cfgOptPgPath)));

            cfgOptionSet(cfgOptDelta, cfgSourceDefault, VARBOOL(false));
            cfgOptionSet(cfgOptForce, cfgSourceDefault, VARBOOL(false));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the backup set to restore
***********************************************************************************************************************************/
static String *
restoreBackupSet(InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If backup set to restore is default (i.e. latest) then get the actual set
        const String *backupSet = NULL;

        if (cfgOptionSource(cfgOptSet) == cfgSourceDefault)
        {
            if (infoBackupDataTotal(infoBackup) == 0)
                THROW(BackupSetInvalidError, "no backup sets to restore");

            backupSet = infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1).backupLabel;
        }
        // Otherwise check to make sure the specified backup set is valid
        else
        {
            bool found = false;
            backupSet = cfgOptionStr(cfgOptSet);

            for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
            {
                if (strEq(infoBackupData(infoBackup, backupIdx).backupLabel, backupSet))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                THROW_FMT(BackupSetInvalidError, "backup set %s is not valid", strPtr(backupSet));
        }

        memContextSwitch(MEM_CONTEXT_OLD());
        result = strDup(backupSet);
        memContextSwitch(MEM_CONTEXT_TEMP());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Validate the manifest
***********************************************************************************************************************************/
static void
restoreManifestValidate(Manifest *manifest, const String *backupSet)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, backupSet);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Sanity check to ensure the manifest has not been moved to a new directory
        const ManifestData *data = manifestData(manifest);

        if (!strEq(data->backupLabel, backupSet))
        {
            THROW_FMT(
                FormatError,
                "requested backup '%s' and manifest label '%s' do not match\n"
                "HINT: this indicates some sort of corruption (at the very least paths have been renamed).",
                strPtr(backupSet), strPtr(data->backupLabel));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestMap(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Remap the data directory
        // -------------------------------------------------------------------------------------------------------------------------
        const String *pgPath = cfgOptionStr(cfgOptPgPath);
        const ManifestTarget *targetBase = manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR);

        if (!strEq(targetBase->path, pgPath))
        {
            LOG_INFO("remap data directory to '%s'", strPtr(pgPath));
            manifestTargetUpdate(manifest, targetBase->name, pgPath, NULL);
        }

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *tablespaceMap = varKv(cfgOption(cfgOptTablespaceMap));
        const String *tablespaceMapAllPath = cfgOptionStr(cfgOptTablespaceMapAll);

        if (tablespaceMap != NULL || tablespaceMapAllPath != NULL)
        {
            StringList *tablespaceRemapped = strLstNew();

            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                // Is this a tablespace?
                if (target->tablespaceId != 0)
                {
                    const String *tablespacePath = NULL;

                    // Check for an individual mapping for this tablespace
                    if (tablespaceMap != NULL)
                    {
                        // Attempt to get the tablespace by name
                        const String *tablespacePathByName = varStr(kvGet(tablespaceMap, VARSTR(target->tablespaceName)));

                        if (tablespacePathByName != NULL)
                            strLstAdd(tablespaceRemapped, target->tablespaceName);

                        // Attempt to get the tablespace by id
                        const String *tablespacePathById = varStr(
                            kvGet(tablespaceMap, VARSTR(varStrForce(VARUINT(target->tablespaceId)))));

                        if (tablespacePathById != NULL)
                            strLstAdd(tablespaceRemapped, varStrForce(VARUINT(target->tablespaceId)));

                        // Error when both are set but the paths are different
                        if (tablespacePathByName != NULL && tablespacePathById != NULL && !
                            strEq(tablespacePathByName, tablespacePathById))
                        {
                            THROW_FMT(
                                TablespaceMapError, "tablespace remapped by name '%s' and id %u with different paths",
                                strPtr(target->tablespaceName), target->tablespaceId);
                        }
                        // Else set the path by name
                        else if (tablespacePathByName != NULL)
                        {
                            tablespacePath = tablespacePathByName;
                        }
                        // Else set the path by id
                        else if (tablespacePathById != NULL)
                            tablespacePath = tablespacePathById;
                    }

                    // If not individual mapping check if all tablespaces are being remapped
                    if (tablespacePath == NULL && tablespaceMapAllPath != NULL)
                        tablespacePath = strNewFmt("%s/%s", strPtr(tablespaceMapAllPath), strPtr(target->tablespaceName));

                    // Remap tablespace if a mapping was found
                    if (tablespacePath != NULL)
                    {
                        LOG_INFO("map tablespace '%s' to '%s'", strPtr(target->name), strPtr(tablespacePath));

                        manifestTargetUpdate(manifest, target->name, tablespacePath, NULL);
                        manifestLinkUpdate(manifest, strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strPtr(target->name)), tablespacePath);
                    }
                }
            }

            // Error on invalid tablespaces
            if (tablespaceMap != NULL)
            {
                const VariantList *tablespaceMapList = kvKeyList(tablespaceMap);

                for (unsigned int tablespaceMapIdx = 0; tablespaceMapIdx < varLstSize(tablespaceMapList); tablespaceMapIdx++)
                {
                    const String *tablespace = varStr(varLstGet(tablespaceMapList, tablespaceMapIdx));

                    if (!strLstExists(tablespaceRemapped, tablespace))
                        THROW_FMT(TablespaceMapError, "unable to remap invalid tablespace '%s'", strPtr(tablespace));
                }
            }

            // Issue a warning message when remapping tablespaces in postgre < 9.2
            if (manifestData(manifest)->pgVersion <= PG_VERSION_92)
                LOG_WARN("update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= " PG_VERSION_92_STR);
        }

        // Remap links
        // -------------------------------------------------------------------------------------------------------------------------
        KeyValue *linkMap = varKv(cfgOption(cfgOptLinkMap));
        bool linkAll = cfgOptionBool(cfgOptLinkAll);

        StringList *linkRemapped = strLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);

            // Is this a link?
            if (target->type == manifestTargetTypeLink && target->tablespaceId == 0)
            {
                const String *link = strSub(target->name, strSize(MANIFEST_TARGET_PGDATA_STR) + 1);
                const String *linkPath = linkMap == NULL ? NULL : varStr(kvGet(linkMap, VARSTR(link)));

                // Remap link if a mapping was found
                if (linkPath != NULL)
                {
                    LOG_INFO("map link '%s' to '%s'", strPtr(link), strPtr(linkPath));
                    manifestLinkUpdate(manifest, target->name, linkPath);

                    // If the link is a file separate the file name from the path to update the target
                    const String *linkFile = NULL;

                    if (target->file != NULL)
                    {
                        linkFile = strBase(linkPath);
                        linkPath = strPath(linkPath);
                    }

                    manifestTargetUpdate(manifest, target->name, linkPath, linkFile);

                    // Add to remapped list for later validation that all links were valid
                    strLstAdd(linkRemapped, link);
                }
                // If all links are not being restored then remove the target and link
                else if (!linkAll)
                {
                    if (target->file != NULL)
                        LOG_WARN("file link '%s' will be restored as a file at the same location", strPtr(link));
                    else
                    {
                        LOG_WARN(
                            "contents of directory link '%s' will be restored in a directory at the same location",
                            strPtr(link));
                    }

                    manifestLinkRemove(manifest, target->name);
                    manifestTargetRemove(manifest, target->name);
                    targetIdx--;
                }
            }
        }

        // Error on invalid links
        if (linkMap != NULL)
        {
            const VariantList *linkMapList = kvKeyList(linkMap);

            for (unsigned int linkMapIdx = 0; linkMapIdx < varLstSize(linkMapList); linkMapIdx++)
            {
                const String *link = varStr(varLstGet(linkMapList, linkMapIdx));

                if (!strLstExists(linkRemapped, link))
                    THROW_FMT(LinkMapError, "unable to remap invalid link '%s'", strPtr(link));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check ownership of items in the manifest
***********************************************************************************************************************************/
#define RESTORE_MANIFEST_OWNER_GET(type)                                                                                           \
    for (unsigned int itemIdx = 0; itemIdx < manifest##type##Total(manifest); itemIdx++)                                           \
    {                                                                                                                              \
        Manifest##type *item = (Manifest##type *)manifest##type(manifest, itemIdx);                                                \
                                                                                                                                   \
        if (item->user == NULL)                                                                                                    \
            userNull = true;                                                                                                       \
        else                                                                                                                       \
            strLstAddIfMissing(userList, item->user);                                                                              \
                                                                                                                                   \
        if (item->group == NULL)                                                                                                   \
            groupNull = true;                                                                                                      \
        else                                                                                                                       \
            strLstAddIfMissing(groupList, item->group);                                                                            \
                                                                                                                                   \
        if (!userRoot())                                                                                                           \
        {                                                                                                                          \
            item->user = NULL;                                                                                                     \
            item->group = NULL;                                                                                                    \
        }                                                                                                                          \
    }

#define RESTORE_MANIFEST_OWNER_NULL_UPDATE(type, user, group)                                                                      \
    for (unsigned int itemIdx = 0; itemIdx < manifest##type##Total(manifest); itemIdx++)                                           \
    {                                                                                                                              \
        Manifest##type *item = (Manifest##type *)manifest##type(manifest, itemIdx);                                                \
                                                                                                                                   \
        if (item->user == NULL)                                                                                                    \
            item->user = user;                                                                                                     \
                                                                                                                                   \
        if (item->group == NULL)                                                                                                   \
            item->group = group;                                                                                                   \
    }

#define RESTORE_MANIFEST_OWNER_WARN(type)                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        if (type##Null)                                                                                                            \
            LOG_WARN("unknown " #type " in backup manifest mapped to current " #type);                                             \
                                                                                                                                   \
        for (unsigned int ownerIdx = 0; ownerIdx < strLstSize(type##List); ownerIdx++)                                             \
        {                                                                                                                          \
            const String *owner = strLstGet(type##List, ownerIdx);                                                                 \
                                                                                                                                   \
            if (type##Name() == NULL ||  !strEq(type##Name(), owner))                                                              \
                LOG_WARN("unknown " #type " '%s' in backup manifest mapped to current " #type, strPtr(owner));                     \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

static void
restoreManifestOwner(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build a list of users and groups in the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        bool userNull = false;
        StringList *userList = strLstNew();
        bool groupNull = false;
        StringList *groupList = strLstNew();

        RESTORE_MANIFEST_OWNER_GET(File);
        RESTORE_MANIFEST_OWNER_GET(Link);
        RESTORE_MANIFEST_OWNER_GET(Path);

        // Build a list of users and groups in the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        if (userRoot())
        {
            // Get user/group info from data directory to use for invalid user/groups
            StorageInfo pathInfo = storageInfoNP(storagePg(), manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path);

            // If user/group is null then set it to root
            if (pathInfo.user == NULL)
                pathInfo.user = userName();

            if (pathInfo.group == NULL)
                pathInfo.group = groupName();

            if (userNull || groupNull)
            {
                if (userNull)
                    LOG_WARN("unknown user in backup manifest mapped to '%s'", strPtr(pathInfo.user));

                if (groupNull)
                    LOG_WARN("unknown group in backup manifest mapped to '%s'", strPtr(pathInfo.group));

                memContextSwitch(MEM_CONTEXT_OLD());

                const String *user = strDup(pathInfo.user);
                const String *group = strDup(pathInfo.group);

                RESTORE_MANIFEST_OWNER_NULL_UPDATE(File, user, group)
                RESTORE_MANIFEST_OWNER_NULL_UPDATE(Link, user, group)
                RESTORE_MANIFEST_OWNER_NULL_UPDATE(Path, user, group)

                memContextSwitch(MEM_CONTEXT_TEMP());
            }
        }
        // Else map everything to the current user/group
        // -------------------------------------------------------------------------------------------------------------------------
        else
        {
            RESTORE_MANIFEST_OWNER_WARN(user);
            RESTORE_MANIFEST_OWNER_WARN(group);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Clean the data directory of any paths/files/links that are not in the manifest and create missing links/paths
***********************************************************************************************************************************/
typedef struct RestoreCleanCallbackData
{
    const Manifest *manifest;                                       // Manifest to compare against
    const ManifestTarget *target;                                   // Current target being compared
    const String *targetName;                                       // Name to use when finding files/paths/links
    const String *targetPath;                                       // Path of target currently being compared
    const String *subPath;                                          // Subpath in target currently being compared
    bool basePath;                                                  // Is this the base path?
    bool exists;                                                    // Does the target path exist?
    bool delta;                                                     // Is this a delta restore?
    bool preserveRecoveryConf;                                      // Should the recovery.conf file be preserved?
} RestoreCleanCallbackData;

static void
restoreCleanOwnership(
    const String *pgPath, const String *manifestUserName, const String *manifestGroupName, uid_t actualUserId, gid_t actualGroupId,
    bool new)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pgPath);
        FUNCTION_TEST_PARAM(STRING, manifestUserName);
        FUNCTION_TEST_PARAM(STRING, manifestGroupName);
        FUNCTION_TEST_PARAM(UINT, actualUserId);
        FUNCTION_TEST_PARAM(UINT, actualGroupId);
        FUNCTION_TEST_PARAM(BOOL, new);
    FUNCTION_TEST_END();

    ASSERT(pgPath != NULL);

    // Get the expected user id
    uid_t expectedUserId = userId();

    if (manifestUserName != NULL)
    {
        uid_t manifestUserId = userIdFromName(manifestUserName);

        if (manifestUserId != (uid_t)-1)
            expectedUserId = manifestUserId;
    }

    // Get the expected group id
    gid_t expectedGroupId = groupId();

    if (manifestGroupName != NULL)
    {
        uid_t manifestGroupId = groupIdFromName(manifestGroupName);

        if (manifestGroupId != (uid_t)-1)
            expectedGroupId = manifestGroupId;
    }

    // Update ownership if not as expected
    if (actualUserId != expectedUserId || actualGroupId != expectedGroupId)
    {
        // If this is a newly created file/link/path then there's no need to log updated permissions
        if (!new)
            LOG_DETAIL("update ownership for '%s'", strPtr(pgPath));

        THROW_ON_SYS_ERROR_FMT(
            lchown(strPtr(pgPath), expectedUserId, expectedGroupId) == -1, FileOwnerError, "unable to set ownership for '%s'",
            strPtr(pgPath));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreCleanMode(const String *pgPath, mode_t manifestMode, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pgPath);
        FUNCTION_TEST_PARAM(MODE, manifestMode);
        FUNCTION_TEST_PARAM(INFO, info);
    FUNCTION_TEST_END();

    // Update mode if not as expected
    if (manifestMode != info->mode)
    {
        LOG_DETAIL("update mode for '%s' to %04o", strPtr(pgPath), manifestMode);

        THROW_ON_SYS_ERROR_FMT(
            chmod(strPtr(pgPath), manifestMode) == -1, FileOwnerError, "unable to set mode for '%s'", strPtr(pgPath));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreCleanInfoListCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    RestoreCleanCallbackData *cleanData = (RestoreCleanCallbackData *)data;

    // Don't include backup.manifest or recovery.conf (when preserved) in the comparison or empty directory check
    if (cleanData->basePath && info->type == storageTypeFile &&
        (strEq(info->name, MANIFEST_FILE_STR) || (cleanData->preserveRecoveryConf && strEq(info->name, PG_FILE_RECOVERYCONF_STR))))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Is this the . path, i.e. the root path for this list?
    bool dotPath = info->type == storageTypePath && strEq(info->name, DOT_STR);

    // If this is not a delta then error because the directory is expected to be empty.  Ignore the . path.
    if (!cleanData->delta)
    {
        if (!dotPath)
        {
            THROW_FMT(
                PathNotEmptyError,
                "unable to restore to path '%s' because it contains files\n"
                "HINT: try using --delta if this is what you intended.",
                strPtr(cleanData->targetPath));
        }

        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Construct the name used to find this file/link/path in the manifest
    const String *manifestName = dotPath ?
        cleanData->targetName : strNewFmt("%s/%s", strPtr(cleanData->targetName), strPtr(info->name));

    // Construct the path of this file/link/path in the PostgreSQL data directory
    const String *pgPath = dotPath ?
        cleanData->targetPath : strNewFmt("%s/%s", strPtr(cleanData->targetPath), strPtr(info->name));

    switch (info->type)
    {
        case storageTypeFile:
        {
            const ManifestFile *manifestFile = manifestFileFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestFile != NULL)
            {
                restoreCleanOwnership(pgPath, manifestFile->user, manifestFile->group, info->userId, info->groupId, false);
                restoreCleanMode(pgPath, manifestFile->mode, info);
            }
            else
            {
                LOG_DETAIL("remove invalid file '%s'", strPtr(pgPath));
                storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
            }

            break;
        }

        case storageTypeLink:
        {
            const ManifestLink *manifestLink = manifestLinkFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestLink != NULL)
            {
                if (!strEq(manifestLink->destination, info->linkDestination))
                {
                    LOG_DETAIL("remove link '%s' because destination changed", strPtr(pgPath));
                    storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                }
                else
                    restoreCleanOwnership(pgPath, manifestLink->user, manifestLink->group, info->userId, info->groupId, false);
            }
            else
            {
                LOG_DETAIL("remove invalid link '%s'", strPtr(pgPath));
                storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
            }

            break;
        }

        case storageTypePath:
        {
            const ManifestPath *manifestPath = manifestPathFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestPath != NULL)
            {
                // Check ownership/permissions
                if (dotPath)
                {
                    restoreCleanOwnership(pgPath, manifestPath->user, manifestPath->group, info->userId, info->groupId, false);
                    restoreCleanMode(pgPath, manifestPath->mode, info);
                }
                // Recurse into the path
                else
                {
                    RestoreCleanCallbackData cleanDataSub = *cleanData;
                    cleanDataSub.targetName = strNewFmt("%s/%s", strPtr(cleanData->targetName), strPtr(info->name));
                    cleanDataSub.targetPath = strNewFmt("%s/%s", strPtr(cleanData->targetPath), strPtr(info->name));
                    cleanDataSub.basePath = false;

                    storageInfoListP(
                        storageLocalWrite(), cleanDataSub.targetPath, restoreCleanInfoListCallback, &cleanDataSub,
                        .errorOnMissing = true, .sortOrder = sortOrderAsc);
                }
            }
            else
            {
                LOG_DETAIL("remove invalid path '%s'", strPtr(pgPath));
                storagePathRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true, .recurse = true);
            }

            break;
        }

        // Special file types cannot exist in the manifest so just delete them
        case storageTypeSpecial:
        {
            storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreClean(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a delta restore?
        bool delta = cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce);

        // Allocate data for each target
        RestoreCleanCallbackData *cleanDataList = memNew(sizeof(RestoreCleanCallbackData) * manifestTargetTotal(manifest));

        // Check permissions and validity (is the directory empty without delta?) if the target directory exists
        // -------------------------------------------------------------------------------------------------------------------------
        const String *basePath = manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path;

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *cleanData = &cleanDataList[targetIdx];

            cleanData->manifest = manifest;
            cleanData->target = manifestTarget(manifest, targetIdx);
            cleanData->targetName = cleanData->target->name;
            cleanData->targetPath = cleanData->target->path;
            cleanData->basePath = strEq(cleanData->targetName, MANIFEST_TARGET_PGDATA_STR);
            cleanData->delta = delta;
            cleanData->preserveRecoveryConf = strEq(cfgOptionStr(cfgOptType), RECOVERY_TYPE_PRESERVE_STR);

            // If the target path is relative update it
            if (!strBeginsWith(cleanData->targetPath, FSLASH_STR))
            {
                const String *linkPath = strPath(manifestPgPath(cleanData->target->name));

                cleanData->targetPath = strNewFmt(
                    "%s/%s", strPtr(basePath),
                    strSize(linkPath) > 0 ?
                        strPtr(strNewFmt("%s/%s", strPtr(linkPath), strPtr(cleanData->target->path))) :
                        strPtr(cleanData->target->path));
            }

            // If this is a tablespace append the tablespace identifier
            if (cleanData->target->type == manifestTargetTypeLink && cleanData->target->tablespaceId != 0)
            {
                const String *tablespaceId = pgTablespaceId(manifestData(manifest)->pgVersion);

                // Only PostgreSQL >= 9.0 has tablespace indentifiers
                if (tablespaceId != NULL)
                {
                    cleanData->targetName = strNewFmt("%s/%s", strPtr(cleanData->targetName), strPtr(tablespaceId));
                    cleanData->targetPath = strNewFmt("%s/%s", strPtr(cleanData->targetPath), strPtr(tablespaceId));
                }
            }

            // Check that the path exists.  If not, there's no need to do any cleaning and we'll attempt to create it later.
            // ??? Note that a path may be checked multiple times if more than one file link points to the same path.  This is
            // harmless but creates noise in the log so it may be worth de-duplicating.
            LOG_DETAIL("check '%s' exists", strPtr(cleanData->targetPath));
            StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .ignoreMissing = true, .followLink = true);

            if (info.exists)
            {
                // Make sure our uid will be able to write to this directory
                if (!userRoot() && userId() != info.userId)
                {
                    THROW_FMT(
                        PathOpenError, "unable to restore to path '%s' not owned by current user",
                        strPtr(cleanData->targetPath));
                }

                if ((info.mode & 0700) != 0700)
                {
                    THROW_FMT(
                        PathOpenError, "unable to restore to path '%s' without rwx permissions", strPtr(cleanData->targetPath));
                }

                // If not a delta restore then check that the directories are empty
                if (!cleanData->delta)
                {
                    if (cleanData->target->file == NULL)
                    {
                        storageInfoListP(
                            storageLocal(), cleanData->targetPath, restoreCleanInfoListCallback, cleanData,
                            .errorOnMissing = true);
                    }
                    else
                    {
                        // !!! JUST CHECK TO SEE IF THE FILE IS PRESENT
                    }

                    // Now that we know there are no files in this target enable delta to process the next pass
                    cleanData->delta = true;
                }

                // The target directory exists and is valid and will need to be cleaned
                cleanData->exists = true;
            }
        }

        // Clean target directories
        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *cleanData = &cleanDataList[targetIdx];

            // Only clean if the target exists
            if (cleanData->exists)
            {
                // Don't clean file links.  It doesn't matter whether the file exists or not since we know it is in the manifest.
                if (cleanData->target->file == NULL)
                {
                    // Only log when doing a delta restore because otherwise the targets should be empty.  We'll still run the clean
                    // to fix permissions/ownership on the target paths.
                    if (delta)
                        LOG_INFO("remove invalid files/links/paths from '%s'", strPtr(cleanData->targetPath));

                    // Clean the target
                    storageInfoListP(
                        storageLocalWrite(), cleanData->targetPath, restoreCleanInfoListCallback, cleanData, .errorOnMissing = true,
                        .sortOrder = sortOrderAsc);
                }
            }
            // If the target does not exist we'll attempt to create it
            else
            {
                const ManifestPath *path = NULL;

                // There is no path information for a file link so we'll need to use the data directory
                if (cleanData->target->file != NULL)
                    path = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);
                else
                    path = manifestPathFind(manifest, cleanData->target->name);

                storagePathCreateP(storageLocalWrite(), cleanData->targetPath, .mode = path->mode);
                restoreCleanOwnership(cleanData->targetPath, path->user, path->group, userId(), groupId(), true);
            }
        }

        // Create missing paths and path links
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        {
            const ManifestPath *path = manifestPath(manifest, pathIdx);

            // Skip the pg_tblspc path because it only maps to the manifest.  We should remove this in a future release but not much
            // can be done about it for now.
            if (strEqZ(path->name, MANIFEST_TARGET_PGTBLSPC))
                continue;

            // If this path has been mapped as a link then create a link
            const ManifestLink *link = manifestLinkFindDefault(
                manifest,
                strBeginsWithZ(path->name, MANIFEST_TARGET_PGTBLSPC) ?
                    strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strPtr(path->name)) : path->name,
                NULL);

            if (link != NULL)
            {
                const String *pgPath = storagePathNP(storagePg(), manifestPgPath(link->name));
                StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the link if it is missing.  If it exists it should already have the correct ownership and destination.
                if (!linkInfo.exists)
                {
                    LOG_DETAIL("create symlink '%s' to '%s'", strPtr(pgPath), strPtr(link->destination));

                    THROW_ON_SYS_ERROR_FMT(
                        symlink(strPtr(link->destination), strPtr(pgPath)) == -1, FileOpenError,
                        "unable to create symlink '%s' to '%s'", strPtr(pgPath), strPtr(link->destination));
                    restoreCleanOwnership(pgPath, link->user, link->group, userId(), groupId(), true);
                }
            }
            // Create the path normally
            else
            {
                const String *pgPath = storagePathNP(storagePg(), manifestPgPath(path->name));
                StorageInfo pathInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the path if it is missing  If it exists it should already have the correct ownership and mode.
                if (!pathInfo.exists)
                {
                    LOG_DETAIL("create path '%s'", strPtr(pgPath));

                    storagePathCreateP(storagePgWrite(), pgPath, .mode = path->mode, .noParentCreate = true, .errorOnExists = true);
                    restoreCleanOwnership(storagePathNP(storagePg(), pgPath), path->user, path->group, userId(), groupId(), true);
                }
            }
        }

        // Create file links
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(manifest); linkIdx++)
        {
            const ManifestLink *link = manifestLink(manifest, linkIdx);

            const String *pgPath = storagePathNP(storagePg(), manifestPgPath(link->name));
            StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

            // Create the link if it is missing.  If it exists it should already have the correct ownership and destination.
            if (!linkInfo.exists)
            {
                LOG_DETAIL("create symlink '%s' to '%s'", strPtr(pgPath), strPtr(link->destination));

                THROW_ON_SYS_ERROR_FMT(
                    symlink(strPtr(link->destination), strPtr(pgPath)) == -1, FileOpenError,
                    "unable to create symlink '%s' to '%s'", strPtr(pgPath), strPtr(link->destination));
                restoreCleanOwnership(pgPath, link->user, link->group, userId(), groupId(), true);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Generate the expression to zero files that are not needed for selective restore
***********************************************************************************************************************************/
static String *
restoreSelectiveExpression(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    String *result = NULL;

    // Continue if db-include is specified
    if (cfgOptionTest(cfgOptDbInclude))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Generate base expression
            RegExp *baseRegExp = regExpNew(STRDEF("^" MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/[0-9]+/" PG_FILE_PGVERSION));

            // Generate tablespace expression
            RegExp *tablespaceRegExp = NULL;
            const String *tablespaceId = pgTablespaceId(manifestData(manifest)->pgVersion);

            if (tablespaceId == NULL)
                tablespaceRegExp = regExpNew(STRDEF("^" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/[0-9]+/" PG_FILE_PGVERSION));
            else
            {
                tablespaceRegExp = regExpNew(
                    strNewFmt("^" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+/" PG_FILE_PGVERSION, strPtr(tablespaceId)));
            }

            // Generate a list of databases in base and or in a tablespace
            StringList *dbList = strLstNew();

            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile *file = manifestFile(manifest, fileIdx);

                if (regExpMatch(baseRegExp, file->name) || regExpMatch(tablespaceRegExp, file->name))
                    strLstAdd(dbList, strBase(strPath(file->name)));
            }

            // If no databases where found then this backup is not a valid cluster
            if (strLstSize(dbList) == 0)
            {
                THROW(
                    FormatError,
                    "no databases found for selective restore\n"
                    "HINT: is this a valid cluster?");
            }

            // Log databases found
            LOG_DETAIL("databases found for selective restore (%s)", strPtr(strLstJoin(dbList, ", ")));

            // Remove included databases from the list
            const StringList *includeList = strLstNewVarLst(cfgOptionLst(cfgOptDbInclude));

            for (unsigned int includeIdx = 0; includeIdx < strLstSize(includeList); includeIdx++)
            {
                const String *includeDb = strLstGet(includeList, includeIdx);

                // If the db to include is not in the list as an id then search by name
                if (!strLstExists(dbList, includeDb))
                {
                    const ManifestDb *db = manifestDbFindDefault(manifest, includeDb, NULL);

                    if (db == NULL || !strLstExists(dbList, varStrForce(VARUINT(db->id))))
                        THROW_FMT(DbMissingError, "database to include '%s' does not exist", strPtr(includeDb));

                    // Set the include db to the id if the name mapping was successful
                    includeDb = varStrForce(VARUINT(db->id));
                }

                // Error if the db is a built-in db
                // if (cvtZToUInt64(strPtr(includeDb)) < PG_USER_OBJECT_MIN_ID)
                //     THROW(DbInvalidError, "system databases (template0, postgres, etc.) are included by default");

                // Remove from list of DBs to zero
                strLstRemove(dbList, includeDb);
            }

            // Build regular expression to identify files that will be zeroed
            strLstSort(dbList, sortOrderAsc);
            String *expression = NULL;

            for (unsigned int dbIdx = 0; dbIdx < strLstSize(dbList); dbIdx++)
            {
                const String *db = strLstGet(dbList, dbIdx);

                // Only user created databases can be zeroed, never built-in databases
                if (cvtZToUInt64(strPtr(db)) >= PG_USER_OBJECT_MIN_ID)
                {
                    // Create expression string or add |
                    if (expression == NULL)
                        expression = strNew("");
                    else
                        strCat(expression, "|");

                    // Filter files in base directory
                    strCatFmt(expression, "(^" MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/%s/)", strPtr(db));

                    // Filter files in tablespace directories
                    for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
                    {
                        const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                        if (target->tablespaceId != 0)
                        {
                            if (tablespaceId == NULL)
                                strCatFmt(expression, "|(^%s/%s/)", strPtr(target->name), strPtr(db));
                            else
                                strCatFmt(expression, "|(^%s/%s/%s/)", strPtr(target->name), strPtr(tablespaceId), strPtr(db));
                        }
                    }
                }
            }

            if (expression == NULL)
                LOG_INFO("nothing to filter - all user databases have been selected");
            else
            {
                memContextSwitch(MEM_CONTEXT_OLD());
                result = strDup(expression);
                memContextSwitch(MEM_CONTEXT_TEMP());
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Generate a list of queues that determine the order of file processing
***********************************************************************************************************************************/
static List *
restoreProcessQueue(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    List *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create list of process queue
        result = lstNew(sizeof(List *));

        // Generate the list of processing queues (there is always at least one)
        StringList *targetList = strLstNew();
        strLstAdd(targetList, MANIFEST_TARGET_PGDATA_STR);

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);

            if (target->tablespaceId != 0)
                strLstAdd(targetList, target->name);
        }

        // Generate the processing queues
        for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
        {
            List *queue = lstNew(sizeof(ManifestFile *));
            lstAdd(result, &queue);
        }

        // Now put all files into the processing queues
        // for (unsigned int fileIdx = 0; fileIdx < manifestTargetTotal(manifest); fileIdx++)
        // {
        //     const ManifestFile *file = manifestFile(manifest, fileIdx);
        //
        //     for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
        //     {
        //         if (
        //         String *target =
        //
        //         const ManifestTarget *target = manifestTarget(manifest, targetIdx);
        //
        // }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}

/***********************************************************************************************************************************
Restore a backup
***********************************************************************************************************************************/
typedef struct RestoreData
{
    List *queueList;                                                // List of processing queues
} RestoreData;

void
cmdRestore(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get information for the current user
        userInit();

        // PostgreSQL must be local
        if (!pgIsLocal(1))
            THROW(HostInvalidError, CFGCMD_RESTORE " command must be run on the " PG_NAME " host");

        // Validate restore path
        restorePathValidate();

        // Get the repo storage in case it is remote and encryption settings need to be pulled down
        storageRepo();

        // Load backup.info
        InfoBackup *infoBackup = infoBackupLoadFile(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        // Get the backup set
        const String *backupSet = restoreBackupSet(infoBackup);

        // Load manifest
        Manifest *manifest = manifestLoadFile(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" MANIFEST_FILE, strPtr(backupSet)),
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));

        // !!! THIS IS TEMPORARY TO DOUBLE-CHECK THE C MANIFEST CODE.  LOAD THE ORIGINAL MANIFEST AND COMPARE IT TO WHAT WE WOULD
        // SAVE TO DISK IF WE SAVED NOW.  THE LATER SAVE MAY HAVE MADE MODIFICATIONS BASED ON USER INPUT SO WE CAN'T USE THAT.
        // -------------------------------------------------------------------------------------------------------------------------
        if (cipherType(cfgOptionStr(cfgOptRepoCipherType)) == cipherTypeNone)                       // {uncovered_branch - !!! TEST}
        {
            Buffer *manifestTestPerlBuffer = storageGetNP(
                storageNewReadNP(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" MANIFEST_FILE, strPtr(backupSet))));

            Buffer *manifestTestCBuffer = bufNew(0);
            manifestSave(manifest, ioBufferWriteNew(manifestTestCBuffer));

            if (!bufEq(manifestTestPerlBuffer, manifestTestCBuffer))                                // {uncovered_branch - !!! TEST}
            {
                // Dump manifests to disk so we can check them with diff
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(MANIFEST_FILE ".expected")), manifestTestPerlBuffer);
                storagePutNP(                                                                       // {uncovered - !!! TEST}
                    storageNewWriteNP(storagePgWrite(), STRDEF(MANIFEST_FILE ".actual")), manifestTestCBuffer);

                THROW_FMT(                                                                          // {uncovered - !!! TEST}
                    AssertError, "C and Perl manifests are not equal, files saved to '%s'",
                    strPtr(storagePathNP(storagePgWrite(), NULL)));
            }
        }

        // Validate the manifest
        restoreManifestValidate(manifest, backupSet);

        // Log the backup set to restore
        LOG_INFO("restore backup set %s", strPtr(backupSet));

        // Map manifest
        restoreManifestMap(manifest);

        // Update ownership
        restoreManifestOwner(manifest);

        // Generate the selective restore expression
        String *expression = restoreSelectiveExpression(manifest);
        RegExp *excludeExp = expression == NULL ? NULL : regExpNew(expression);
        (void)excludeExp; // !!! REMOVE

        // Clean the data directory
        restoreClean(manifest);

        // Save manifest before any modifications are made to PGDATA
        manifestSave(manifest, storageWriteIo(storageNewWriteNP(storagePgWrite(), MANIFEST_FILE_STR)));

        // Generate processing queues
        (void)restoreProcessQueue;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
