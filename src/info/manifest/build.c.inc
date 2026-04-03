/***********************************************************************************************************************************
Build manifest from PGDATA
***********************************************************************************************************************************/
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
    const ManifestBlockIncrMap *blockIncrMap;                       // Block incremental maps
} ManifestBuildData;

// Calculate block incremental size for a file. The block size is based on the size and age of the file. Larger files get larger
// block sizes to reduce the cost of the map. Older files also get larger block sizes under the assumption that they are unlikely to
// be modified if they have not been modified in a while. Very old and very small files skip block incremental entirely.
//
// Smaller blocks will be compressed/encrypted together in a larger super block for efficiency.
static size_t
manifestBuildBlockIncrSize(const ManifestBuildData *const buildData, const ManifestFile *const file)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buildData);
        FUNCTION_TEST_PARAM(MANIFEST_FILE, file);
    FUNCTION_TEST_END();

    size_t result = 0;

    // Search size map for the appropriate block size
    for (unsigned int sizeIdx = 0; sizeIdx < lstSize(buildData->blockIncrMap->sizeMap); sizeIdx++)
    {
        const ManifestBlockIncrSizeMap *const sizeMap = lstGet(buildData->blockIncrMap->sizeMap, sizeIdx);

        if (file->size >= sizeMap->fileSize)
        {
            result = sizeMap->blockSize;
            break;
        }
    }

    // If block size > 0 then search age map for a multiplier
    if (result != 0)
    {
        const time_t fileAge = buildData->manifest->pub.data.backupTimestampStart - file->timestamp;

        for (unsigned int ageIdx = 0; ageIdx < lstSize(buildData->blockIncrMap->ageMap); ageIdx++)
        {
            const ManifestBlockIncrAgeMap *const ageMap = lstGet(buildData->blockIncrMap->ageMap, ageIdx);

            if (fileAge >= (time_t)ageMap->fileAge)
            {
                result *= ageMap->blockMultiplier;
                break;
            }
        }
    }

    FUNCTION_TEST_RETURN(SIZE, result);
}

// Get checksum size for a block size. Since these checksums are used to determine if a block has changed (not for corruption) they
// should be set larger than usual. For example, it is common to use 32-bit checksums to check for corruption in blocks up to 4MiB
// but here we used more bits to ensure block changes are correctly detected, since valid changes may look a lot different than
// corruption.
static size_t
manifestBuildBlockIncrChecksumSize(const ManifestBuildData *const buildData, const size_t blockSize)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, buildData);
        FUNCTION_TEST_PARAM(SIZE, blockSize);
    FUNCTION_TEST_END();

    size_t result = BLOCK_INCR_CHECKSUM_SIZE_MIN;

    // Search checksum size map for larger value
    for (unsigned int sizeIdx = 0; sizeIdx < lstSize(buildData->blockIncrMap->checksumSizeMap); sizeIdx++)
    {
        const ManifestBlockIncrChecksumSizeMap *const sizeMap = lstGet(buildData->blockIncrMap->checksumSizeMap, sizeIdx);

        if (blockSize >= sizeMap->blockSize)
        {
            result = sizeMap->checksumSize;
            break;
        }
    }

    FUNCTION_TEST_RETURN(SIZE, result);
}

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

    FUNCTION_AUDIT_HELPER();

    ASSERT(buildData != NULL);
    ASSERT(manifestParentName != NULL);
    ASSERT(pgPath != NULL);
    ASSERT(info != NULL);

    // Skip any path/file/link that begins with pgsql_tmp. The files are removed when the server is restarted and the directories
    // are recreated.
    if (strBeginsWithZ(info->name, PG_PREFIX_PGSQLTMP))
        FUNCTION_TEST_RETURN_VOID();

    // Get build data
    const unsigned int pgVersion = buildData->manifest->pub.data.pgVersion;

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
            const ManifestPath path =
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
                if (strEqZ(info->name, PG_PATH_PGDYNSHMEM))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_notify/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGNOTIFY))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_replslot/* since these files are generally not useful after a restore
                if (strEqZ(info->name, PG_PATH_PGREPLSLOT))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_serial/* since these files are reset
                if (strEqZ(info->name, PG_PATH_PGSERIAL))
                    FUNCTION_TEST_RETURN_VOID();

                // Skip pg_snapshots/* since these files cannot be reused on recovery
                if (strEqZ(info->name, PG_PATH_PGSNAPSHOTS))
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

            // Skip pg_internal.init since it is recreated on startup. It's also possible, (though unlikely) that a temp file with
            // the creating process id as the extension can exist so skip that as well. This seems to be a bug in PostgreSQL since
            // the temp file should be removed on startup. Use regExpMatchOne() here instead of preparing a regexp in advance since
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
                    (strEqZ(info->name, PG_FILE_POSTGRESQLAUTOCONFTMP)) ||
                    // Skip obsolete backup_label/backup_label.old
                    strEqZ(info->name, PG_FILE_BACKUPLABEL) || strEqZ(info->name, PG_FILE_BACKUPLABELOLD) ||
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
                .sizeOriginal = info->size,
                .sizeRepo = info->size,
                .timestamp = info->timeModified,
            };

            // When bundling zero-length files do not need to be copied
            if (info->size == 0 && buildData->manifest->pub.data.bundle)
            {
                file.copy = false;
                file.checksumSha1 = bufPtrConst(HASH_TYPE_SHA1_ZERO_BUF);
            }

            // Get block incremental size
            if (info->size != 0 && buildData->manifest->pub.data.blockIncr)
            {
                file.blockIncrSize = manifestBuildBlockIncrSize(buildData, &file);
                file.blockIncrChecksumSize = manifestBuildBlockIncrChecksumSize(buildData, file.blockIncrSize);
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
            // If the destination is another link then error. In the future we'll allow this by following the link chain to the
            // eventual destination but for now we are trying to maintain compatibility during the migration. To do this check we
            // need to read outside of the data directory but it is a read-only operation so is considered safe.
            const String *const linkDestinationAbsolute = strPathAbsolute(info->linkDestination, pgPath);
            const StorageInfo linkedCheck = storageInfoP(
                buildData->storagePg, linkDestinationAbsolute, .ignoreMissing = true, .noPathEnforce = true);

            if (linkedCheck.exists && linkedCheck.type == storageTypeLink)
            {
                THROW_FMT(
                    LinkDestinationError, "link '%s/%s' cannot reference another link '%s'", strZ(pgPath), strZ(info->name),
                    strZ(linkDestinationAbsolute));
            }

            // Initialize link and target
            const ManifestLink link =
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
                // Strip pg_data off the manifest name so it begins with pg_tblspc instead. This reflects how the files are stored
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

                    // Error if the tablespace could not be found. ??? This seems excessive, perhaps just warn here?
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

                // Add a dummy pg_tblspc path entry if it does not already exist. This entry will be ignored by restore but it is
                // part of the original manifest format so we need to have it.
                lstSort(buildData->manifest->pub.pathList, sortOrderAsc);
                const ManifestPath *const pathBase = manifestPathFind(buildData->manifest, MANIFEST_TARGET_PGDATA_STR);

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
                const ManifestPath *const pathTblSpc = manifestPathFind(
                    buildData->manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" MANIFEST_TARGET_PGTBLSPC));

                const ManifestPath path =
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
            const String *const linkPgPath = strNewFmt("%s/%s", strZ(pgPath), strZ(linkName));
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
            // Else dummy up the target with a destination so manifestLinkCheck() can be run. This is so errors about links with
            // destinations in PGDATA will take precedence over missing a destination. We will probably simplify this once the
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
