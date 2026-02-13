/***********************************************************************************************************************************
Validate restore path
***********************************************************************************************************************************/
static void
restorePathValidate(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // PostgreSQL must not be running
        if (storageExistsP(storagePg(), PG_FILE_POSTMTRPID_STR))
        {
            THROW_FMT(
                PgRunningError,
                "unable to restore while PostgreSQL is running\n"
                "HINT: presence of '" PG_FILE_POSTMTRPID "' in '%s' indicates PostgreSQL is running.\n"
                "HINT: remove '" PG_FILE_POSTMTRPID "' only if PostgreSQL is not running.",
                strZ(cfgOptionDisplay(cfgOptPgPath)));
        }

        // If the restore will be destructive attempt to verify that PGDATA looks like a valid PostgreSQL directory
        if ((cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce)) &&
            !storageExistsP(storagePg(), PG_FILE_PGVERSION_STR) && !storageExistsP(storagePg(), BACKUP_MANIFEST_FILE_STR))
        {
            LOG_WARN_FMT(
                "--delta or --force specified but unable to find '" PG_FILE_PGVERSION "' or '" BACKUP_MANIFEST_FILE "' in '%s' to"
                " confirm that this is a valid $PGDATA directory. --delta and --force have been disabled and if any files exist"
                " in the destination directories the restore will be aborted.",
                strZ(cfgOptionDisplay(cfgOptPgPath)));

            // Disable delta and force so restore will fail if the directories are not empty
            cfgOptionSet(cfgOptDelta, cfgSourceDefault, BOOL_FALSE_VAR);
            cfgOptionSet(cfgOptForce, cfgSourceDefault, BOOL_FALSE_VAR);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Validate the manifest
***********************************************************************************************************************************/
static void
restoreManifestValidate(const Manifest *const manifest, const String *const backupSet)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, backupSet);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);
    ASSERT(backupSet != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If there are no files in the manifest then something has gone horribly wrong
        CHECK(FormatError, manifestFileTotal(manifest) > 0, "manifest missing files");

        // Sanity check to ensure the manifest has not been moved to a new directory
        const ManifestData *const data = manifestData(manifest);

        if (!strEq(data->backupLabel, backupSet))
        {
            THROW_FMT(
                FormatError,
                "requested backup '%s' and manifest label '%s' do not match\n"
                "HINT: this indicates some sort of corruption (at the very least paths have been renamed).",
                strZ(backupSet), strZ(data->backupLabel));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
