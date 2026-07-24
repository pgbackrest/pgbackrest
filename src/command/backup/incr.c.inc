/***********************************************************************************************************************************
Create an incremental backup if type is not full and a compatible prior backup exists
***********************************************************************************************************************************/
// Helper to find a compatible prior backup
static Manifest *
backupBuildIncrPrior(const InfoBackup *const infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    Manifest *result = NULL;

    // No incremental if backup type is full
    const BackupType type = (BackupType)cfgOptionStrId(cfgOptType);

    if (type != backupTypeFull)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
            const String *backupLabelPrior = NULL;
            const unsigned int backupTotal = infoBackupDataTotal(infoBackup);

            for (unsigned int backupIdx = backupTotal - 1; backupIdx < backupTotal; backupIdx--)
            {
                const InfoBackupData backupPrior = infoBackupData(infoBackup, backupIdx);

                // The prior backup for a diff must be full
                if (type == backupTypeDiff && backupPrior.backupType != backupTypeFull)
                    continue;

                // The backups must come from the same cluster ??? This should enable delta instead
                if (infoPg.id != backupPrior.backupPgId)
                    continue;

                // This backup is a candidate for prior
                backupLabelPrior = strDup(backupPrior.backupLabel);
                break;
            }

            // If there is a prior backup then check that options for the new backup are compatible
            if (backupLabelPrior != NULL)
            {
                result = manifestLoadFile(
                    storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabelPrior)),
                    cfgOptionStrId(cfgOptRepoCipherType), infoPgCipherPass(infoBackupPg(infoBackup)));
                const ManifestData *const manifestPriorData = manifestData(result);

                LOG_INFO_FMT(
                    "last backup label = %s, version = %s", strZ(manifestData(result)->backupLabel),
                    strZ(manifestData(result)->backrestVersion));

                // Warn if compress-type option changed
                if (compressTypeEnum(cfgOptionStrId(cfgOptCompressType)) != manifestPriorData->backupOptionCompressType)
                {
                    LOG_WARN_FMT(
                        "%s backup cannot alter " CFGOPT_COMPRESS_TYPE " option to '%s', reset to value in %s",
                        strZ(cfgOptionDisplay(cfgOptType)), strZ(cfgOptionDisplay(cfgOptCompressType)), strZ(backupLabelPrior));

                    // Set the compression type back to whatever was in the prior backup. This is not strictly needed since we could
                    // store compression type on a per file basis, but it seems simplest and safest for now.
                    cfgOptionSet(
                        cfgOptCompressType, cfgSourceParam, VARSTR(compressTypeStr(manifestPriorData->backupOptionCompressType)));

                    // There's a small chance that the prior manifest is old enough that backupOptionCompressLevel was not recorded.
                    // There's an even smaller chance that the user will also alter compression-type in this scenario right after
                    // upgrading to a newer version. Because we judge this combination of events to be nearly impossible just check
                    // here so no test coverage is needed.
                    CHECK(
                        FormatError, manifestPriorData->backupOptionCompressLevel != NULL,
                        "compress-level missing in prior manifest");

                    // Set the compression level back to whatever was in the prior backup
                    cfgOptionSet(
                        cfgOptCompressLevel, cfgSourceParam, VARINT64(varUInt(manifestPriorData->backupOptionCompressLevel)));
                }

                // If not defined this backup was done in a version prior to page checksums being introduced. Just set checksum-page
                // to false and move on without a warning. Page checksums will start on the next full backup.
                if (manifestData(result)->backupOptionChecksumPage == NULL)
                {
                    cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
                }
                // Don't allow the checksum-page option to change in a diff or incr backup. This could be confusing as only certain
                // files would be checksummed and the list could be incomplete during reporting.
                else
                {
                    const bool checksumPagePrior = varBool(manifestData(result)->backupOptionChecksumPage);

                    // Warn if an incompatible setting was explicitly requested
                    if (checksumPagePrior != cfgOptionBool(cfgOptChecksumPage))
                    {
                        LOG_WARN_FMT(
                            "%s backup cannot alter '" CFGOPT_CHECKSUM_PAGE "' option to '%s', reset to '%s' from %s",
                            strZ(cfgOptionDisplay(cfgOptType)), strZ(cfgOptionDisplay(cfgOptChecksumPage)),
                            cvtBoolToConstZ(checksumPagePrior), strZ(manifestData(result)->backupLabel));
                    }

                    cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, VARBOOL(checksumPagePrior));
                }

                manifestMove(result, memContextPrior());
            }
            else
            {
                LOG_WARN_FMT("no prior backup exists, %s backup has been changed to full", strZ(cfgOptionDisplay(cfgOptType)));
                cfgOptionSet(cfgOptType, cfgSourceParam, VARUINT64(backupTypeFull));
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupBuildIncr(
    const InfoBackup *const infoBackup, Manifest *const manifest, Manifest *const manifestPrior, const String *const archiveStart)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST, manifestPrior);
        FUNCTION_LOG_PARAM(STRING, archiveStart);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    bool result = false;

    // No incremental if no prior manifest
    if (manifestPrior != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Move the manifest to this context so it will be freed when we are done
            manifestMove(manifestPrior, MEM_CONTEXT_TEMP());

            // Build incremental manifest
            manifestBuildIncr(manifest, manifestPrior, (BackupType)cfgOptionStrId(cfgOptType), archiveStart);

            // Set the cipher subpass from prior manifest since we want a single subpass for the entire backup set
            manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestPrior));

            // Incremental was built
            result = true;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}
