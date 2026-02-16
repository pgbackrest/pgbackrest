/***********************************************************************************************************************************
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestMap(Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Remap the data directory
        // -------------------------------------------------------------------------------------------------------------------------
        const String *const pgPath = cfgOptionStr(cfgOptPgPath);
        const ManifestTarget *const targetBase = manifestTargetBase(manifest);

        if (!strEq(targetBase->path, pgPath))
        {
            LOG_INFO_FMT("remap data directory to '%s'", strZ(pgPath));
            manifestTargetUpdate(manifest, targetBase->name, pgPath, NULL);
        }

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        const KeyValue *const tablespaceMap = cfgOptionKvNull(cfgOptTablespaceMap);
        const String *const tablespaceMapAllPath = cfgOptionStrNull(cfgOptTablespaceMapAll);

        if (tablespaceMap != NULL || tablespaceMapAllPath != NULL)
        {
            StringList *const tablespaceRemapped = strLstNew();

            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

                // Is this a tablespace?
                if (target->tablespaceId != 0)
                {
                    const String *tablespacePath = NULL;

                    // Check for an individual mapping for this tablespace
                    if (tablespaceMap != NULL)
                    {
                        // Attempt to get the tablespace by name
                        const String *const tablespacePathByName = varStr(kvGet(tablespaceMap, VARSTR(target->tablespaceName)));

                        if (tablespacePathByName != NULL)
                            strLstAdd(tablespaceRemapped, target->tablespaceName);

                        // Attempt to get the tablespace by id
                        const String *const tablespacePathById = varStr(
                            kvGet(tablespaceMap, VARSTR(varStrForce(VARUINT(target->tablespaceId)))));

                        if (tablespacePathById != NULL)
                            strLstAdd(tablespaceRemapped, varStrForce(VARUINT(target->tablespaceId)));

                        // Error when both are set but the paths are different
                        if (tablespacePathByName != NULL && tablespacePathById != NULL && !
                            strEq(tablespacePathByName, tablespacePathById))
                        {
                            THROW_FMT(
                                TablespaceMapError, "tablespace remapped by name '%s' and id %u with different paths",
                                strZ(target->tablespaceName), target->tablespaceId);
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
                        tablespacePath = strNewFmt("%s/%s", strZ(tablespaceMapAllPath), strZ(target->tablespaceName));

                    // Remap tablespace if a mapping was found
                    if (tablespacePath != NULL)
                    {
                        LOG_INFO_FMT("map tablespace '%s' to '%s'", strZ(target->name), strZ(tablespacePath));

                        manifestTargetUpdate(manifest, target->name, tablespacePath, NULL);
                        manifestLinkUpdate(manifest, strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(target->name)), tablespacePath);
                    }
                }
            }

            // Error on invalid tablespaces
            if (tablespaceMap != NULL)
            {
                const VariantList *const tablespaceMapList = kvKeyList(tablespaceMap);
                strLstSort(tablespaceRemapped, sortOrderAsc);

                for (unsigned int tablespaceMapIdx = 0; tablespaceMapIdx < varLstSize(tablespaceMapList); tablespaceMapIdx++)
                {
                    const String *const tablespace = varStr(varLstGet(tablespaceMapList, tablespaceMapIdx));

                    if (!strLstExists(tablespaceRemapped, tablespace))
                        THROW_FMT(TablespaceMapError, "unable to remap invalid tablespace '%s'", strZ(tablespace));
                }
            }
        }

        // Remap links
        // -------------------------------------------------------------------------------------------------------------------------
        const KeyValue *const linkMap = cfgOptionKvNull(cfgOptLinkMap);

        if (linkMap != NULL)
        {
            const StringList *const linkMapList = strLstSort(strLstNewVarLst(kvKeyList(linkMap)), sortOrderAsc);

            for (unsigned int linkMapIdx = 0; linkMapIdx < strLstSize(linkMapList); linkMapIdx++)
            {
                const String *const link = strLstGet(linkMapList, linkMapIdx);
                const String *const linkPath = varStr(kvGet(linkMap, VARSTR(link)));
                const String *const manifestName = strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(link));

                // Attempt to find the link target
                ManifestTarget target = {0};

                if (manifestTargetFindDefault(manifest, manifestName, NULL) != NULL)
                    target = *manifestTargetFind(manifest, manifestName);

                // If the target was not found then check if the link is a valid file or path
                bool create = false;

                if (target.name == NULL)
                {
                    // Is the specified link a file or a path? Error if they both match.
                    const bool pathExists = manifestPathFindDefault(manifest, manifestName, NULL) != NULL;
                    const bool fileExists = manifestFileExists(manifest, manifestName);

                    CHECK(FormatError, !pathExists || !fileExists, "link may not be both file and path");

                    target = (ManifestTarget){.name = manifestName, .path = linkPath, .type = manifestTargetTypeLink};

                    // If a file
                    if (fileExists)
                    {
                        // File needs to be set so the file/path is updated later but set it to something invalid just in case it
                        // it does not get updated due to a regression
                        target.file = DOT_STR;
                    }
                    // Else error if not a path
                    else if (!pathExists)
                    {
                        THROW_FMT(
                            LinkMapError,
                            "unable to map link '%s'\n"
                            "HINT: Does the link reference a valid backup path or file?",
                            strZ(link));
                    }

                    // Add the link. Copy user/group from the base data directory.
                    const ManifestPath *const pathBase = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);
                    const ManifestLink manifestLink =
                    {
                        .name = manifestName,
                        .destination = linkPath,
                        .group = pathBase->group,
                        .user = pathBase->user,
                    };

                    manifestLinkAdd(manifest, &manifestLink);
                    create = true;
                }
                // Else update target to new path
                else
                    target.path = linkPath;

                // The target must be a link since pg_data/ was prepended and pgdata is the only allowed path
                CHECK(FormatError, target.type == manifestTargetTypeLink, "target must be a link");

                // Error if the target is a tablespace
                if (target.tablespaceId != 0)
                {
                    THROW_FMT(
                        LinkMapError,
                        "unable to remap tablespace '%s'\n"
                        "HINT: use '" CFGOPT_TABLESPACE_MAP "' option to remap tablespaces.",
                        strZ(link));
                }

                LOG_INFO_FMT("%slink '%s' to '%s'", create ? "" : "map ", strZ(link), strZ(target.path));

                // If the link was not created update to the new destination
                if (!create)
                    manifestLinkUpdate(manifest, target.name, target.path);

                // If the link is a file separate the file name from the path
                if (target.file != NULL)
                {
                    // The link destination must have at least one path component in addition to the file part. So '..' would
                    // not be a valid destination but '../file' or '/file' is.
                    if (strSize(strPath(target.path)) == 0)
                    {
                        THROW_FMT(
                            LinkMapError, "'%s' is not long enough to be the destination for file link '%s'", strZ(target.path),
                            strZ(link));
                    }

                    target.file = strBase(target.path);
                    target.path = strPath(target.path);
                }

                // Create a new target or update the existing target file/path
                if (create)
                    manifestTargetAdd(manifest, &target);
                else
                    manifestTargetUpdate(manifest, target.name, target.path, target.file);
            }
        }

        // If all links are not being restored then check for links that were not remapped and remove them
        if (!cfgOptionBool(cfgOptLinkAll))
        {
            unsigned int targetIdx = 0;

            while (targetIdx < manifestTargetTotal(manifest))
            {
                const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

                // Is this a non-tablespace link?
                if (target->type == manifestTargetTypeLink && target->tablespaceId == 0)
                {
                    const String *const link = strSub(target->name, strSize(MANIFEST_TARGET_PGDATA_STR) + 1);

                    // If the link was not remapped then remove it
                    if (linkMap == NULL || kvGet(linkMap, VARSTR(link)) == NULL)
                    {
                        if (target->file != NULL)
                            LOG_WARN_FMT("file link '%s' will be restored as a file at the same location", strZ(link));
                        else
                        {
                            LOG_WARN_FMT(
                                "contents of directory link '%s' will be restored in a directory at the same location", strZ(link));
                        }

                        manifestLinkRemove(manifest, target->name);
                        manifestTargetRemove(manifest, target->name);
                        continue;
                    }
                }

                targetIdx++;
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
