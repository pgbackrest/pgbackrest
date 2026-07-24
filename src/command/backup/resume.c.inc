/***********************************************************************************************************************************
Check for a backup that can be resumed and merge into the manifest if found
***********************************************************************************************************************************/
// Helper to clean invalid paths/files/links out of the resumable backup path
static void
backupResumeClean(
    StorageIterator *const storageItr, Manifest *const manifest, const Manifest *const manifestResume,
    const CompressType compressType, const bool delta, const String *const backupParentPath, const String *const manifestParentName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, storageItr);           // Storage info
        FUNCTION_LOG_PARAM(MANIFEST, manifest);                     // New manifest
        FUNCTION_LOG_PARAM(MANIFEST, manifestResume);               // Resumed manifest
        FUNCTION_LOG_PARAM(ENUM, compressType);                     // Backup compression type
        FUNCTION_LOG_PARAM(BOOL, delta);                            // Is this a delta backup?
        FUNCTION_LOG_PARAM(STRING, backupParentPath);               // Path to the current level of the backup being cleaned
        FUNCTION_LOG_PARAM(STRING, manifestParentName);             // Parent manifest name used to construct manifest name
    FUNCTION_LOG_END();

    ASSERT(storageItr != NULL);
    ASSERT(manifest != NULL);
    ASSERT(manifestResume != NULL);
    ASSERT(backupParentPath != NULL);

    MEM_CONTEXT_TEMP_RESET_BEGIN()
    {
        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Skip backup.manifest.copy -- it must be preserved to allow resume again if this process throws an error before
            // writing the manifest for the first time
            if (manifestParentName == NULL && strEqZ(info.name, BACKUP_MANIFEST_FILE INFO_COPY_EXT))
                continue;

            // Build the name used to lookup files in the manifest
            const String *manifestName =
                manifestParentName != NULL ? strNewFmt("%s/%s", strZ(manifestParentName), strZ(info.name)) : info.name;

            // Build the backup path used to remove files/links/paths that are invalid
            const String *const backupPath = strNewFmt("%s/%s", strZ(backupParentPath), strZ(info.name));

            // Process file types
            switch (info.type)
            {
                // Check paths
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypePath:
                {
                    // If the path was not found in the new manifest then remove it
                    if (manifestPathFindDefault(manifest, manifestName, NULL) == NULL)
                    {
                        LOG_DETAIL_FMT("remove path '%s' from resumed backup", strZ(storagePathP(storageRepo(), backupPath)));
                        storagePathRemoveP(storageRepoWrite(), backupPath, .recurse = true);
                    }
                    // Else recurse into the path
                    else
                    {
                        backupResumeClean(
                            storageNewItrP(storageRepo(), backupPath, .sortOrder = sortOrderAsc), manifest, manifestResume,
                            compressType, delta, backupPath, manifestName);
                    }

                    break;
                }

                // Check files
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeFile:
                {
                    // If the file is compressed then strip off the extension before doing the lookup
                    const CompressType fileCompressType = compressTypeFromName(manifestName);

                    if (fileCompressType != compressTypeNone)
                        manifestName = compressExtStrip(manifestName, fileCompressType);

                    // If the file is block incremental then strip off the extension before doing the lookup
                    const bool blockIncr = strEndsWithZ(manifestName, BACKUP_BLOCK_INCR_EXT);

                    if (blockIncr)
                        manifestName = strSubN(manifestName, 0, strSize(manifestName) - (sizeof(BACKUP_BLOCK_INCR_EXT) - 1));

                    // Check if the file can be resumed or must be removed
                    const char *removeReason = NULL;

                    if (fileCompressType != compressType && !blockIncr)
                        removeReason = "mismatched compression type";
                    else if (!manifestFileExists(manifest, manifestName))
                        removeReason = "missing in manifest";
                    else
                    {
                        ManifestFile file = manifestFileFind(manifest, manifestName);
                        ASSERT(file.reference == NULL);

                        if (!manifestFileExists(manifestResume, manifestName))
                            removeReason = "missing in resumed manifest";
                        else
                        {
                            const ManifestFile fileResume = manifestFileFind(manifestResume, manifestName);
                            ASSERT(fileResume.reference == NULL);

                            if (fileResume.checksumSha1 == NULL)
                                removeReason = "no checksum in resumed manifest";
                            else if (file.size != fileResume.size)
                                removeReason = "mismatched size";
                            else if (!delta && file.timestamp != fileResume.timestamp)
                                removeReason = "mismatched timestamp";
                            else if (file.size == 0)
                                // ??? don't resume zero size files because Perl wouldn't -- can be removed after the migration)
                                removeReason = "zero size";
                            else
                            {
                                ASSERT(file.copy);
                                ASSERT(file.bundleId == 0);
                                ASSERT(file.blockIncrMapSize == 0);

                                file.sizeRepo = fileResume.sizeRepo;
                                file.checksumSha1 = fileResume.checksumSha1;
                                file.checksumRepoSha1 = fileResume.checksumRepoSha1;
                                file.blockIncrSize = fileResume.blockIncrSize;
                                file.blockIncrChecksumSize = fileResume.blockIncrChecksumSize;
                                file.blockIncrMapSize = fileResume.blockIncrMapSize;
                                file.checksumPage = fileResume.checksumPage;
                                file.checksumPageError = fileResume.checksumPageError;
                                file.checksumPageErrorList = fileResume.checksumPageErrorList;
                                file.resume = true;
                                file.delta = delta;

                                manifestFileUpdate(manifest, &file);
                            }
                        }
                    }

                    // Remove the file if it could not be resumed
                    if (removeReason != NULL)
                    {
                        LOG_DETAIL_FMT(
                            "remove file '%s' from resumed backup (%s)", strZ(storagePathP(storageRepo(), backupPath)),
                            removeReason);
                        storageRemoveP(storageRepoWrite(), backupPath);
                    }

                    break;
                }

                // Remove links. We could check that the link has not changed and preserve it but it doesn't seem worth the extra
                // testing. The link will be recreated during the backup if needed.
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeLink:
                    storageRemoveP(storageRepoWrite(), backupPath);
                    break;

                // Remove special files
                // -----------------------------------------------------------------------------------------------------------------
                case storageTypeSpecial:
                    LOG_WARN_FMT("remove special file '%s' from resumed backup", strZ(storagePathP(storageRepo(), backupPath)));
                    storageRemoveP(storageRepoWrite(), backupPath);
                    break;
            }
        }

        // Reset the memory context occasionally so we don't use too much memory or slow down processing
        MEM_CONTEXT_TEMP_RESET(1000);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

// Helper to find a resumable backup
static const Manifest *
backupResumeFind(const Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Only the last backup can be resumed
        const StringList *const backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        if (!strLstEmpty(backupList))
        {
            const String *const backupLabel = strLstGet(backupList, 0);
            const String *const manifestFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));

            // Resumable backups do not have backup.manifest
            if (!storageExistsP(storageRepo(), manifestFile))
            {
                const bool resume = cfgOptionBool(cfgOptResume) && cfgOptionStrId(cfgOptType) == backupTypeFull;
                bool usable = false;
                const char *reason = "resume only valid for full backup";
                Manifest *manifestResume = NULL;

                if (cfgOptionStrId(cfgOptType) == backupTypeFull)
                {
                    reason = "partially deleted by prior resume or invalid";

                    // Resumable backups must have backup.manifest.copy
                    if (storageExistsP(storageRepo(), strNewFmt("%s" INFO_COPY_EXT, strZ(manifestFile))))
                    {
                        reason = "resume is disabled";

                        // Attempt to read the manifest file in the resumable backup to see if it can be used. If any error at all
                        // occurs then the backup will be considered unusable and a resume will not be attempted.
                        if (resume)
                        {
                            TRY_BEGIN()
                            {
                                manifestResume = manifestLoadFile(
                                    storageRepo(), manifestFile, cfgOptionStrId(cfgOptRepoCipherType), cipherPassBackup);
                            }
                            CATCH_ANY()
                            {
                                reason = zNewFmt("unable to read %s" INFO_COPY_EXT, strZ(manifestFile));
                            }
                            TRY_END();

                            if (manifestResume != NULL)
                            {
                                const ManifestData *manifestResumeData = manifestData(manifestResume);

                                // Check pgBackRest version. This allows the resume implementation to be changed with each version
                                // of pgBackRest at the expense of users losing a resumable back after an upgrade, which seems worth
                                // the cost.
                                if (!strEq(manifestResumeData->backrestVersion, manifestData(manifest)->backrestVersion))
                                {
                                    reason = zNewFmt(
                                        "new " PROJECT_NAME " version '%s' does not match resumable " PROJECT_NAME " version '%s'",
                                        strZ(manifestData(manifest)->backrestVersion), strZ(manifestResumeData->backrestVersion));
                                }
                                // Check backup type because new backup label must be the same type as resume backup label
                                else if (manifestResumeData->backupType != cfgOptionStrId(cfgOptType))
                                {
                                    reason = zNewFmt(
                                        "new backup type '%s' does not match resumable backup type '%s'",
                                        strZ(cfgOptionDisplay(cfgOptType)), zNewStrId(manifestResumeData->backupType));
                                }
                                // Check prior backup label ??? Do we really care about the prior backup label?
                                else if (!strEq(manifestResumeData->backupLabelPrior, manifestData(manifest)->backupLabelPrior))
                                {
                                    reason = zNewFmt(
                                        "new prior backup label '%s' does not match resumable prior backup label '%s'",
                                        manifestResumeData->backupLabelPrior ?
                                            strZ(manifestResumeData->backupLabelPrior) : "<undef>",
                                        manifestData(manifest)->backupLabelPrior ?
                                            strZ(manifestData(manifest)->backupLabelPrior) : "<undef>");
                                }
                                // Check compression. Compression can't be changed between backups so resume won't work either.
                                else if (
                                    manifestResumeData->backupOptionCompressType !=
                                    compressTypeEnum(cfgOptionStrId(cfgOptCompressType)))
                                {
                                    reason = zNewFmt(
                                        "new compression '%s' does not match resumable compression '%s'",
                                        strZ(cfgOptionDisplay(cfgOptCompressType)),
                                        strZ(compressTypeStr(manifestResumeData->backupOptionCompressType)));
                                }
                                else
                                    usable = true;
                            }
                        }
                    }
                }

                // If the backup is usable then return the manifest
                if (usable)
                {
                    result = manifestMove(manifestResume, memContextPrior());
                }
                // Else warn and remove the unusable backup
                else
                {
                    LOG_FMT(
                        resume ? logLevelWarn : logLevelInfo, 0, "backup '%s' cannot be resumed: %s", strZ(backupLabel), reason);

                    storagePathRemoveP(
                        storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(backupLabel)), .recurse = true);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupResume(Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const Manifest *const manifestResume = backupResumeFind(manifest, cipherPassBackup);

        // If a resumable backup was found set the label and cipher subpass
        if (manifestResume)
        {
            // Resuming
            result = true;

            // Set the backup label to the resumed backup
            manifestBackupLabelSet(manifest, manifestData(manifestResume)->backupLabel);

            LOG_WARN_FMT(
                "resumable backup %s of same type exists -- invalid files will be removed then the backup will resume",
                strZ(manifestData(manifest)->backupLabel));

            // Copy cipher subpass since it was used to encrypt the resumable files
            manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestResume));

            // Clean resumed backup
            const String *const backupPath = strNewFmt(STORAGE_REPO_BACKUP "/%s", strZ(manifestData(manifest)->backupLabel));

            backupResumeClean(
                storageNewItrP(storageRepo(), backupPath, .sortOrder = sortOrderAsc), manifest, manifestResume,
                compressTypeEnum(cfgOptionStrId(cfgOptCompressType)), cfgOptionBool(cfgOptDelta), backupPath, NULL);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}
