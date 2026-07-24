/***********************************************************************************************************************************
Save a copy of the backup manifest during processing to preserve checksums for a possible resume. Only save the final copy when
resume is disabled since an incremental copy will not be used in a future backup unless resume is enabled beforehand.
***********************************************************************************************************************************/
static void
backupManifestSaveCopy(Manifest *const manifest, const String *const cipherPassBackup, const bool final)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
        FUNCTION_LOG_PARAM(BOOL, final);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    if (final || cfgOptionBool(cfgOptResume))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Open file for write
            IoWrite *const write = storageWriteIo(
                storageNewWriteP(
                    storageRepoWrite(),
                    strNewFmt(
                        STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(manifestData(manifest)->backupLabel))));

            // Add encryption filter if required
            cipherBlockFilterGroupAdd(
                ioWriteFilterGroup(write), cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt, cipherPassBackup);

            // Save file
            manifestSave(manifest, write);
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check and copy WAL segments required to make the backup consistent
***********************************************************************************************************************************/
static void
backupArchiveCheckCopy(const BackupData *const backupData, Manifest *const manifest, const String *const cipherPassBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_DATA, backupData);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, cipherPassBackup);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    // If archive logs are required to complete the backup, then check them. This is the default, but can be overridden if the
    // archive logs are going to a different server. Be careful of disabling this option because there is no way to verify that the
    // backup will be consistent - at least not here.
    if (cfgOptionBool(cfgOptArchiveCheck))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const uint64_t lsnStart = pgLsnFromStr(manifestData(manifest)->lsnStart);
            const uint64_t lsnStop = pgLsnFromStr(manifestData(manifest)->lsnStop);

            LOG_INFO_FMT(
                "check archive for segment(s) %s:%s",
                strZ(pgLsnToWalSegment(backupData->timeline, lsnStart, backupData->walSegmentSize)),
                strZ(pgLsnToWalSegment(backupData->timeline, lsnStop, backupData->walSegmentSize)));

            // Save the backup manifest before getting archive logs in case of failure
            backupManifestSaveCopy(manifest, cipherPassBackup, false);

            // Use base path to set ownership and mode
            const ManifestPath *const basePath = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);

            // Loop through all the segments in the lsn range
            const StringList *const walSegmentList = pgLsnRangeToWalSegmentList(
                backupData->timeline, lsnStart, lsnStop, backupData->walSegmentSize);
            WalSegmentFind *const find = walSegmentFindNew(
                storageRepo(), backupData->archiveId, strLstSize(walSegmentList) == 1, cfgOptionUInt64(cfgOptArchiveTimeout));

            for (unsigned int walSegmentIdx = 0; walSegmentIdx < strLstSize(walSegmentList); walSegmentIdx++)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    // Find the actual wal segment file (including checksum compression extension) in the archive
                    const String *const walSegment = strLstGet(walSegmentList, walSegmentIdx);
                    const String *const archiveFile = walSegmentFind(find, walSegment);

                    if (cfgOptionBool(cfgOptArchiveCopy))
                    {
                        // Copy can be a pretty expensive operation so log it
                        LOG_DETAIL_FMT("copy segment %s to backup", strZ(walSegment));

                        // Get compression type of the WAL segment and backup
                        const CompressType archiveCompressType = compressTypeFromName(archiveFile);
                        const CompressType backupCompressType = compressTypeEnum(cfgOptionStrId(cfgOptCompressType));

                        // Open the archive file
                        StorageRead *const read = storageNewReadP(
                            storageRepo(),
                            strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(backupData->archiveId), strZ(archiveFile)));
                        IoFilterGroup *const filterGroup = ioReadFilterGroup(storageReadIo(read));

                        // Decrypt with archive key if encrypted
                        cipherBlockFilterGroupAdd(
                            filterGroup, cfgOptionStrId(cfgOptRepoCipherType), cipherModeDecrypt,
                            infoArchiveCipherPass(backupData->archiveInfo));

                        // Compress/decompress if archive and backup do not have the same compression settings
                        if (archiveCompressType != backupCompressType)
                        {
                            if (archiveCompressType != compressTypeNone)
                                ioFilterGroupAdd(filterGroup, decompressFilterP(archiveCompressType));

                            if (backupCompressType != compressTypeNone)
                            {
                                ioFilterGroupAdd(
                                    filterGroup, compressFilterP(backupCompressType, cfgOptionInt(cfgOptCompressLevel)));
                            }
                        }

                        // Encrypt with backup key if encrypted
                        cipherBlockFilterGroupAdd(
                            filterGroup, cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt, manifestCipherSubPass(manifest));

                        // Add size filter last to calculate repo size
                        ioFilterGroupAdd(filterGroup, ioSizeNew());

                        // Copy the file
                        const String *const manifestName = strNewFmt(
                            MANIFEST_TARGET_PGDATA "/%s/%s", strZ(pgWalPath(manifestData(manifest)->pgVersion)), strZ(walSegment));

                        storageCopyP(
                            read,
                            storageNewWriteP(
                                storageRepoWrite(),
                                backupFileRepoPathP(
                                    manifestData(manifest)->backupLabel, manifestName, 0,
                                    compressTypeEnum(cfgOptionStrId(cfgOptCompressType)), false)));

                        // Add to manifest
                        ManifestFile file =
                        {
                            .name = manifestName,
                            .mode = basePath->mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
                            .user = basePath->user,
                            .group = basePath->group,
                            .size = backupData->walSegmentSize,
                            .sizeOriginal = backupData->walSegmentSize,
                            .sizeRepo = pckReadU64P(ioFilterGroupResultP(filterGroup, SIZE_FILTER_TYPE)),
                            .timestamp = manifestData(manifest)->backupTimestampStop,
                            .checksumSha1 = bufPtr(bufNewDecode(encodingHex, strSubN(archiveFile, 25, 40))),
                        };

                        manifestFileAdd(manifest, &file);
                    }
                }
                MEM_CONTEXT_TEMP_END();

                // A keep-alive is required here for the remote holding the backup lock
                protocolKeepAlive();
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Save and update all files required to complete the backup
***********************************************************************************************************************************/
static void
backupComplete(InfoBackup *const infoBackup, Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const backupLabel = manifestData(manifest)->backupLabel;

        // Validation and final save of the backup manifest. Validate in strict mode to catch as many potential issues as possible.
        // -------------------------------------------------------------------------------------------------------------------------
        manifestValidate(manifest, true);

        backupManifestSaveCopy(manifest, infoPgCipherPass(infoBackupPg(infoBackup)), true);

        storageCopy(
            storageNewReadP(
                storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strZ(backupLabel))),
            storageNewWriteP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel))));

        // Copy a compressed version of the manifest to history. If the repo is encrypted then the passphrase to open the manifest
        // is required. We can't just do a straight copy since the destination needs to be compressed and that must happen before
        // encryption in order to be efficient. Compression will always be gz for compatibility and since it is always available.
        // -------------------------------------------------------------------------------------------------------------------------
        StorageRead *const manifestRead = storageNewReadP(
            storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel)));

        cipherBlockFilterGroupAdd(
            ioReadFilterGroup(storageReadIo(manifestRead)), cfgOptionStrId(cfgOptRepoCipherType), cipherModeDecrypt,
            infoPgCipherPass(infoBackupPg(infoBackup)));

        StorageWrite *const manifestWrite = storageNewWriteP(
            storageRepoWrite(),
            strNewFmt(
                STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY "/%s/%s.manifest%s", strZ(strSubN(backupLabel, 0, 4)),
                strZ(backupLabel), strZ(compressExtStr(compressTypeGz))));

        ioFilterGroupAdd(ioWriteFilterGroup(storageWriteIo(manifestWrite)), compressFilterP(compressTypeGz, 9));

        cipherBlockFilterGroupAdd(
            ioWriteFilterGroup(storageWriteIo(manifestWrite)), cfgOptionStrId(cfgOptRepoCipherType), cipherModeEncrypt,
            infoPgCipherPass(infoBackupPg(infoBackup)));

        storageCopyP(manifestRead, manifestWrite);

        // Sync history path if required
        if (storageFeature(storageRepoWrite(), storageFeaturePathSync))
            storagePathSyncP(storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY));

        // Create a symlink to the most recent backup if supported. This link is purely informational for the user and is never used
        // by us since symlinks are not supported on all storage types.
        // -------------------------------------------------------------------------------------------------------------------------
        backupLinkLatest(backupLabel, cfgOptionGroupIdxDefault(cfgOptGrpRepo));

        // Add manifest and save backup.info (infoBackupSaveFile() is responsible for proper syncing)
        // -------------------------------------------------------------------------------------------------------------------------
        infoBackupDataAdd(infoBackup, manifest);

        infoBackupSaveFile(
            infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));

        // Save archive.info/copy so the timestamps will be updated to prevent lifecycle settings from removing the files early
        // -------------------------------------------------------------------------------------------------------------------------
        infoArchiveSaveFile(
            infoArchiveLoadFile(
                storageRepo(), INFO_ARCHIVE_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
                cfgOptionStrNull(cfgOptRepoCipherPass)),
            storageRepoWrite(), INFO_ARCHIVE_PATH_FILE_STR, cfgOptionStrId(cfgOptRepoCipherType),
            cfgOptionStrNull(cfgOptRepoCipherPass));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
