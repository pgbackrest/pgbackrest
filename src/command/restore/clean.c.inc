/***********************************************************************************************************************************
Check ownership of items in the manifest
***********************************************************************************************************************************/
// Helper to determine what the user/group of a path/file/link should be
static const String *
restoreManifestOwnerReplace(const String *const owner, const String *const ownerDefaultRoot)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, owner);
        FUNCTION_TEST_PARAM(STRING, ownerDefaultRoot);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN_CONST(STRING, userRoot() ? (owner == NULL ? ownerDefaultRoot : owner) : NULL);
}

// Helper to get list of owners from a file/link/path list
#define RESTORE_MANIFEST_OWNER_GET(type, deref)                                                                                    \
    for (unsigned int itemIdx = 0; itemIdx < manifest##type##Total(manifest); itemIdx++)                                           \
    {                                                                                                                              \
        const Manifest##type item = deref manifest##type(manifest, itemIdx);                                                       \
                                                                                                                                   \
        if (item.user == NULL)                                                                                                     \
            userNull = true;                                                                                                       \
        else                                                                                                                       \
            strLstAddIfMissing(userList, item.user);                                                                               \
                                                                                                                                   \
        if (item.group == NULL)                                                                                                    \
            groupNull = true;                                                                                                      \
        else                                                                                                                       \
            strLstAddIfMissing(groupList, item.group);                                                                             \
    }

// Helper to warn when an owner is missing and must be remapped
#define RESTORE_MANIFEST_OWNER_WARN(type)                                                                                          \
    do                                                                                                                             \
    {                                                                                                                              \
        if (type##Null)                                                                                                            \
            LOG_WARN("unknown " #type " in backup manifest mapped to current " #type);                                             \
                                                                                                                                   \
        for (unsigned int ownerIdx = 0; ownerIdx < strLstSize(type##List); ownerIdx++)                                             \
        {                                                                                                                          \
            const String *const owner = strLstGet(type##List, ownerIdx);                                                           \
                                                                                                                                   \
            if (type##Name() == NULL || !strEq(type##Name(), owner))                                                               \
                LOG_WARN_FMT("unknown " #type " '%s' in backup manifest mapped to current " #type, strZ(owner));                   \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

static void
restoreManifestOwner(const Manifest *const manifest, const String **const rootReplaceUser, const String **const rootReplaceGroup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(VOID, rootReplaceUser);
        FUNCTION_LOG_PARAM_P(VOID, rootReplaceGroup);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build a list of users and groups in the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        bool userNull = false;
        StringList *const userList = strLstNew();
        bool groupNull = false;
        StringList *const groupList = strLstNew();

        RESTORE_MANIFEST_OWNER_GET(File, );
        RESTORE_MANIFEST_OWNER_GET(Link, *(const ManifestLink *));
        RESTORE_MANIFEST_OWNER_GET(Path, *(const ManifestPath *));

        // Update users and groups in the manifest (this can only be done as root)
        // -------------------------------------------------------------------------------------------------------------------------
        if (userRoot())
        {
            // Get user/group info from data directory to use for invalid user/groups
            StorageInfo pathInfo = storageInfoP(storagePg(), manifestTargetBase(manifest)->path, .ignoreMissing = true);

            // If user/group is null then set it to root
            if (pathInfo.user == NULL)                                                                              // {vm_covered}
                pathInfo.user = userName();                                                                         // {vm_covered}

            if (pathInfo.group == NULL)                                                                             // {vm_covered}
                pathInfo.group = groupName();                                                                       // {vm_covered}

            if (userNull || groupNull)
            {
                if (userNull)
                    LOG_WARN_FMT("unknown user in backup manifest mapped to '%s'", strZ(pathInfo.user));

                if (groupNull)
                    LOG_WARN_FMT("unknown group in backup manifest mapped to '%s'", strZ(pathInfo.group));

                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    *rootReplaceUser = strDup(pathInfo.user);
                    *rootReplaceGroup = strDup(pathInfo.group);
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        // Else set owners to NULL. This means we won't make any attempt to update ownership and will just leave it as written by
        // the current user/group. If there are existing files that are not owned by the current user/group then we will attempt to
        // update them, which will generally cause an error, though some systems allow updates to the group ownership.
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
    StringList *fileIgnore;                                         // Files to ignore during clean
    const String *rootReplaceUser;                                  // User to replace invalid users when root
    const String *rootReplaceGroup;                                 // Group to replace invalid group when root
} RestoreCleanCallbackData;

// Helper to update ownership on a file/link/path
static void
restoreCleanOwnership(
    const String *const pgPath, const String *manifestUserName, const String *const rootReplaceUser,
    const String *manifestGroupName, const String *const rootReplaceGroup, const uid_t actualUserId, const gid_t actualGroupId,
    const bool new)
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

    manifestUserName = restoreManifestOwnerReplace(manifestUserName, rootReplaceUser);

    if (manifestUserName != NULL)
    {
        const uid_t manifestUserId = userIdFromName(manifestUserName);

        if (manifestUserId != (uid_t)-1)
            expectedUserId = manifestUserId;
    }

    // Get the expected group id
    gid_t expectedGroupId = groupId();

    manifestGroupName = restoreManifestOwnerReplace(manifestGroupName, rootReplaceGroup);

    if (manifestGroupName != NULL)
    {
        const uid_t manifestGroupId = groupIdFromName(manifestGroupName);

        if (manifestGroupId != (uid_t)-1)
            expectedGroupId = manifestGroupId;
    }

    // Update ownership if not as expected
    if (actualUserId != expectedUserId || actualGroupId != expectedGroupId)
    {
        // If this is a newly created file/link/path then there's no need to log updated permissions
        if (!new)
            LOG_DETAIL_FMT("update ownership for '%s'", strZ(pgPath));

        THROW_ON_SYS_ERROR_FMT(
            lchown(strZ(pgPath), expectedUserId, expectedGroupId) == -1, FileOwnerError, "unable to set ownership for '%s'",
            strZ(pgPath));
    }

    FUNCTION_TEST_RETURN_VOID();
}

// Helper to update mode on a file/path
static void
restoreCleanMode(const String *const pgPath, const mode_t manifestMode, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, pgPath);
        FUNCTION_TEST_PARAM(MODE, manifestMode);
        FUNCTION_TEST_PARAM(INFO, info);
    FUNCTION_TEST_END();

    ASSERT(pgPath != NULL);
    ASSERT(info != NULL);

    // Update mode if not as expected
    if (manifestMode != info->mode)
    {
        LOG_DETAIL_FMT("update mode for '%s' to %04o", strZ(pgPath), manifestMode);

        THROW_ON_SYS_ERROR_FMT(
            chmod(strZ(pgPath), manifestMode) == -1, FileModeError, "unable to set mode for '%s'", strZ(pgPath));
    }

    FUNCTION_TEST_RETURN_VOID();
}

// Recurse paths
static void
restoreCleanBuildRecurse(StorageIterator *const storageItr, const RestoreCleanCallbackData *const cleanData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_ITERATOR, storageItr);
        FUNCTION_TEST_PARAM_P(VOID, cleanData);
    FUNCTION_TEST_END();

    ASSERT(storageItr != NULL);
    ASSERT(cleanData != NULL);

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Don't include backup.manifest or recovery.conf (when preserved) in the comparison or empty directory check
            if (cleanData->basePath && info.type == storageTypeFile && strLstExists(cleanData->fileIgnore, info.name))
                continue;

            // If this is not a delta then error because the directory is expected to be empty. Ignore the . path.
            if (!cleanData->delta)
            {
                THROW_FMT(
                    PathNotEmptyError,
                    "unable to restore to path '%s' because it contains files\n"
                    "HINT: try using --delta if this is what you intended.",
                    strZ(cleanData->targetPath));
            }

            // Construct the name used to find this file/link/path in the manifest
            const String *const manifestName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(info.name));

            // Construct the path of this file/link/path in the PostgreSQL data directory
            const String *const pgPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(info.name));

            switch (info.type)
            {
                case storageTypeFile:
                {
                    if (manifestFileExists(cleanData->manifest, manifestName) &&
                        manifestLinkFindDefault(cleanData->manifest, manifestName, NULL) == NULL)
                    {
                        const ManifestFile manifestFile = manifestFileFind(cleanData->manifest, manifestName);

                        restoreCleanOwnership(
                            pgPath, manifestFile.user, cleanData->rootReplaceUser, manifestFile.group, cleanData->rootReplaceGroup,
                            info.userId, info.groupId, false);
                        restoreCleanMode(pgPath, manifestFile.mode, &info);
                    }
                    else
                    {
                        LOG_DETAIL_FMT("remove invalid file '%s'", strZ(pgPath));
                        storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                    }

                    break;
                }

                case storageTypeLink:
                {
                    const ManifestLink *const manifestLink = manifestLinkFindDefault(cleanData->manifest, manifestName, NULL);

                    if (manifestLink != NULL)
                    {
                        if (!strEq(manifestLink->destination, info.linkDestination))
                        {
                            LOG_DETAIL_FMT("remove link '%s' because destination changed", strZ(pgPath));
                            storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                        }
                        else
                        {
                            restoreCleanOwnership(
                                pgPath, manifestLink->user, cleanData->rootReplaceUser, manifestLink->group,
                                cleanData->rootReplaceGroup, info.userId, info.groupId, false);
                        }
                    }
                    else
                    {
                        LOG_DETAIL_FMT("remove invalid link '%s'", strZ(pgPath));
                        storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                    }

                    break;
                }

                case storageTypePath:
                {
                    const ManifestPath *const manifestPath = manifestPathFindDefault(cleanData->manifest, manifestName, NULL);

                    if (manifestPath != NULL && manifestLinkFindDefault(cleanData->manifest, manifestName, NULL) == NULL)
                    {
                        // Check ownership/permissions
                        restoreCleanOwnership(
                            pgPath, manifestPath->user, cleanData->rootReplaceUser, manifestPath->group,
                            cleanData->rootReplaceGroup, info.userId, info.groupId, false);
                        restoreCleanMode(pgPath, manifestPath->mode, &info);

                        // Recurse into the path
                        RestoreCleanCallbackData cleanDataSub = *cleanData;
                        cleanDataSub.targetName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(info.name));
                        cleanDataSub.targetPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(info.name));
                        cleanDataSub.basePath = false;

                        restoreCleanBuildRecurse(
                            storageNewItrP(
                                storageLocalWrite(), cleanDataSub.targetPath, .errorOnMissing = true, .sortOrder = sortOrderAsc),
                            &cleanDataSub);
                    }
                    else
                    {
                        LOG_DETAIL_FMT("remove invalid path '%s'", strZ(pgPath));
                        storagePathRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true, .recurse = true);
                    }

                    break;
                }

                // Special file types cannot exist in the manifest so just delete them
                case storageTypeSpecial:
                    LOG_DETAIL_FMT("remove special file '%s'", strZ(pgPath));
                    storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                    break;
            }

            // Reset the memory context occasionally so we don't use too much memory or slow down processing
            MEM_CONTEXT_TEMP_RESET(1000);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreCleanBuild(const Manifest *const manifest, const String *const rootReplaceUser, const String *const rootReplaceGroup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, rootReplaceUser);
        FUNCTION_LOG_PARAM(STRING, rootReplaceGroup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a delta restore?
        const bool delta = cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce);

        // Allocate data for each target
        RestoreCleanCallbackData *const cleanDataList = memNew(sizeof(RestoreCleanCallbackData) * manifestTargetTotal(manifest));

        // Step 1: Check permissions and validity (is the directory empty without delta?) if the target directory exists
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *const pathChecked = strLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *const cleanData = &cleanDataList[targetIdx];

            *cleanData = (RestoreCleanCallbackData)
            {
                .manifest = manifest,
                .target = manifestTarget(manifest, targetIdx),
                .delta = delta,
                .fileIgnore = strLstNew(),
                .rootReplaceUser = rootReplaceUser,
                .rootReplaceGroup = rootReplaceGroup,
            };

            cleanData->targetName = cleanData->target->name;
            cleanData->targetPath = manifestTargetPath(manifest, cleanData->target);
            cleanData->basePath = strEq(cleanData->targetName, MANIFEST_TARGET_PGDATA_STR);

            // Ignore backup.manifest while cleaning since it may exist from an prior incomplete restore
            strLstAdd(cleanData->fileIgnore, BACKUP_MANIFEST_FILE_STR);

            // Also ignore recovery files when recovery type = preserve
            if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_PRESERVE)
            {
                // If recovery GUCs then three files must be preserved
                if (manifestData(manifest)->pgVersion >= PG_VERSION_RECOVERY_GUC)
                {
                    strLstAdd(cleanData->fileIgnore, PG_FILE_POSTGRESQLAUTOCONF_STR);
                    strLstAdd(cleanData->fileIgnore, PG_FILE_RECOVERYSIGNAL_STR);
                    strLstAdd(cleanData->fileIgnore, PG_FILE_STANDBYSIGNAL_STR);
                }
                // Else just recovery.conf
                else
                    strLstAdd(cleanData->fileIgnore, PG_FILE_RECOVERYCONF_STR);
            }

            // If this is a tablespace append the tablespace identifier
            if (cleanData->target->type == manifestTargetTypeLink && cleanData->target->tablespaceId != 0)
            {
                const String *const tablespaceId = pgTablespaceId(
                    manifestData(manifest)->pgVersion, manifestData(manifest)->pgCatalogVersion);

                cleanData->targetName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(tablespaceId));
                cleanData->targetPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(tablespaceId));
            }

            strLstSort(cleanData->fileIgnore, sortOrderAsc);

            // Check that the path exists. If not, there's no need to do any cleaning and we'll attempt to create it later. Don't
            // log check for the same path twice. There can be multiple links to files in the same path, but logging it more than
            // once makes the logs noisy and looks like a bug.
            if (!strLstExists(pathChecked, cleanData->targetPath))
                LOG_DETAIL_FMT("check '%s' exists", strZ(cleanData->targetPath));

            const StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .ignoreMissing = true, .followLink = true);
            strLstAdd(pathChecked, cleanData->targetPath);

            if (info.exists)
            {
                // Make sure our uid will be able to write to this directory
                if (!userRoot() && userId() != info.userId)
                {
                    THROW_FMT(
                        PathOpenError, "unable to restore to path '%s' not owned by current user", strZ(cleanData->targetPath));
                }

                if ((info.mode & 0700) != 0700)
                {
                    THROW_FMT(
                        PathOpenError, "unable to restore to path '%s' without rwx permissions", strZ(cleanData->targetPath));
                }

                // If not a delta restore then check that the directories are empty, or if a file link, that the file doesn't exist
                if (!cleanData->delta)
                {
                    if (cleanData->target->file == NULL)
                    {
                        restoreCleanBuildRecurse(
                            storageNewItrP(storageLocal(), cleanData->targetPath, .errorOnMissing = true), cleanData);
                    }
                    else
                    {
                        const String *const file = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(cleanData->target->file));

                        if (storageExistsP(storageLocal(), file))
                        {
                            THROW_FMT(
                                FileExistsError,
                                "unable to restore file '%s' because it already exists\n"
                                "HINT: try using --delta if this is what you intended.",
                                strZ(file));
                        }
                    }

                    // Now that we know there are no files in this target enable delta for processing in step 2
                    cleanData->delta = true;
                }

                // The target directory exists and is valid and will need to be cleaned
                cleanData->exists = true;
            }
        }

        // Skip the tablespace_map file when present so PostgreSQL does not rewrite links in pg_tblspc. The tablespace links will be
        // created after paths are cleaned.
        if (manifestFileExists(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_TABLESPACEMAP)))
        {
            LOG_DETAIL_FMT("skip '" PG_FILE_TABLESPACEMAP "' -- tablespace links will be created based on mappings");
            manifestFileRemove(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_TABLESPACEMAP));
        }

        // Skip postgresql.auto.conf if preserve is set and the PostgreSQL version supports recovery GUCs
        if (manifestData(manifest)->pgVersion >= PG_VERSION_RECOVERY_GUC &&
            cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_PRESERVE &&
            manifestFileExists(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF)))
        {
            LOG_DETAIL_FMT("skip '" PG_FILE_POSTGRESQLAUTOCONF "' -- recovery type is preserve");
            manifestFileRemove(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF));
        }

        // Step 2: Clean target directories
        // -------------------------------------------------------------------------------------------------------------------------
        // Delete the pg_control file (if it exists) so the cluster cannot be started if restore does not complete. Sync the path so
        // the file does not return, zombie-like, in the case of a host crash.
        if (storageExistsP(storagePg(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)))
        {
            LOG_DETAIL_FMT(
                "remove '" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "' so cluster will not start if restore does not complete");
            storageRemoveP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL));
            storagePathSyncP(storagePgWrite(), PG_PATH_GLOBAL_STR);
        }

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const RestoreCleanCallbackData *const cleanData = &cleanDataList[targetIdx];

            // Only clean if the target exists
            if (cleanData->exists)
            {
                // Don't clean file links. It doesn't matter whether the file exists or not since we know it is in the manifest.
                if (cleanData->target->file == NULL)
                {
                    // Only log when doing a delta restore because otherwise the targets should be empty. We'll still run the clean
                    // to fix permissions/ownership on the target paths.
                    if (delta)
                        LOG_INFO_FMT("remove invalid files/links/paths from '%s'", strZ(cleanData->targetPath));

                    // Check target ownership/permissions
                    const ManifestPath *const manifestPath = manifestPathFind(cleanData->manifest, cleanData->targetName);
                    const StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .followLink = true);

                    restoreCleanOwnership(
                        cleanData->targetPath, manifestPath->user, rootReplaceUser, manifestPath->group, rootReplaceGroup,
                        info.userId, info.groupId, false);
                    restoreCleanMode(cleanData->targetPath, manifestPath->mode, &info);

                    // Clean the target
                    restoreCleanBuildRecurse(
                        storageNewItrP(
                            storageLocalWrite(), cleanData->targetPath, .errorOnMissing = true, .sortOrder = sortOrderAsc),
                        cleanData);
                }
            }
            // If the target does not exist we'll attempt to create it
            else
            {
                const ManifestPath *path = NULL;

                // There is no path information for a file link so we'll need to use the data directory
                if (cleanData->target->file != NULL)
                {
                    path = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);
                }
                // Else grab the info for the path that matches the link name
                else
                    path = manifestPathFind(manifest, cleanData->target->name);

                storagePathCreateP(storageLocalWrite(), cleanData->targetPath, .mode = path->mode);
                restoreCleanOwnership(
                    cleanData->targetPath, path->user, rootReplaceUser, path->group, rootReplaceGroup, userId(), groupId(), true);
            }
        }

        // Step 3: Create missing paths and path links
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        {
            const ManifestPath *const path = manifestPath(manifest, pathIdx);

            // Skip the pg_tblspc path because it only maps to the manifest. We should remove this in a future release but not much
            // can be done about it for now.
            if (strEq(path->name, MANIFEST_TARGET_PGTBLSPC_STR))
                continue;

            // If this path has been mapped as a link then create a link. The path has already been created as part of target
            // creation (or it might have already existed).
            const ManifestLink *const link = manifestLinkFindDefault(
                manifest,
                strBeginsWith(path->name, MANIFEST_TARGET_PGTBLSPC_STR) ?
                    strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(path->name)) : path->name,
                NULL);

            if (link != NULL)
            {
                const String *const pgPath = storagePathP(storagePg(), manifestPathPg(link->name));
                const StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the link if it is missing. If it exists it should already have the correct ownership and destination.
                if (!linkInfo.exists)
                {
                    LOG_DETAIL_FMT("create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));

                    storageLinkCreateP(storagePgWrite(), link->destination, pgPath);
                    restoreCleanOwnership(
                        pgPath, link->user, rootReplaceUser, link->group, rootReplaceGroup, userId(), groupId(), true);
                }
            }
            // Create the path normally
            else
            {
                const String *const pgPath = storagePathP(storagePg(), manifestPathPg(path->name));
                const StorageInfo pathInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the path if it is missing. If it exists it should already have the correct ownership and mode.
                if (!pathInfo.exists)
                {
                    LOG_DETAIL_FMT("create path '%s'", strZ(pgPath));

                    storagePathCreateP(storagePgWrite(), pgPath, .mode = path->mode, .noParentCreate = true, .errorOnExists = true);
                    restoreCleanOwnership(
                        storagePathP(storagePg(), pgPath), path->user, rootReplaceUser, path->group, rootReplaceGroup, userId(),
                        groupId(), true);
                }
            }
        }

        // Step 4: Create file links. These don't get created during path creation because they do not have a matching path entry.
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(manifest); linkIdx++)
        {
            const ManifestLink *const link = manifestLink(manifest, linkIdx);
            const String *const pgPath = storagePathP(storagePg(), manifestPathPg(link->name));
            const StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

            // Create the link if it is missing. If it exists it should already have the correct ownership and destination.
            if (!linkInfo.exists)
            {
                LOG_DETAIL_FMT("create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));

                storageLinkCreateP(storagePgWrite(), link->destination, pgPath);
                restoreCleanOwnership(
                    pgPath, link->user, rootReplaceUser, link->group, rootReplaceGroup, userId(), groupId(), true);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
