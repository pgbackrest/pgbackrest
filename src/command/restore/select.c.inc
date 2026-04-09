/***********************************************************************************************************************************
Get the backup set to restore
***********************************************************************************************************************************/
typedef struct RestoreBackupData
{
    unsigned int repoIdx;                                           // Internal repo index
    CipherType repoCipherType;                                      // Repo encryption type (0 = none)
    const String *backupCipherPass;                                 // Passphrase of backup files if repo is encrypted (else NULL)
    const String *backupSet;                                        // Backup set to restore
} RestoreBackupData;

#define FUNCTION_LOG_RESTORE_BACKUP_DATA_TYPE                                                                                      \
    RestoreBackupData
#define FUNCTION_LOG_RESTORE_BACKUP_DATA_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(&value, "RestoreBackupData", buffer, bufferSize)

// Helper function for restoreBackupSet
static RestoreBackupData
restoreBackupData(const String *const backupLabel, const unsigned int repoIdx, const String *const backupCipherPass)
{
    ASSERT(backupLabel != NULL);

    RestoreBackupData restoreBackup = {0};

    MEM_CONTEXT_PRIOR_BEGIN()
    {
        restoreBackup.backupSet = strDup(backupLabel);
        restoreBackup.repoIdx = repoIdx;
        restoreBackup.repoCipherType = cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx);
        restoreBackup.backupCipherPass = strDup(backupCipherPass);
    }
    MEM_CONTEXT_PRIOR_END();

    return restoreBackup;
}

static RestoreBackupData
restoreBackupSet(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    FUNCTION_AUDIT_STRUCT();

    RestoreBackupData result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Initialize the repo index
        unsigned int repoIdxMin = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxMin = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxMin;
        }

        // If the set option was not provided by the user but a target was set, then we will need to search for a backup set that
        // satisfies the target condition, else we will use the backup provided
        const String *backupSetRequested = NULL;
        const unsigned int targetType = cfgOptionSeq(cfgOptType);

        union
        {
            time_t time;
            uint64_t lsn;
        } target = {0};

        if (cfgOptionSource(cfgOptSet) == cfgSourceDefault)
        {
            if (targetType == CFGOPTVAL_RESTORE_TYPE_TIME)
            {
                TRY_BEGIN()
                {
                    target.time = cvtZToTime(strZ(cfgOptionStr(cfgOptTarget)));
                }
                CATCH_ANY()
                {
                    THROW_FMT(
                        FormatError,
                        "automatic backup set selection cannot be performed with provided time '%s'\n"
                        "HINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone (+/- HH or HHMM or"
                        " HH:MM) - if timezone is omitted, local time is assumed (for UTC use +00)",
                        strZ(cfgOptionStr(cfgOptTarget)));
                }
                TRY_END();
            }
            else if (targetType == CFGOPTVAL_RESTORE_TYPE_LSN)
                target.lsn = pgLsnFromStr(cfgOptionStr(cfgOptTarget));
        }
        else
            backupSetRequested = cfgOptionStr(cfgOptSet);

        // Search through the repo list for a backup set to use for recovery
        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepoIdx(repoIdx);

            const InfoBackup *infoBackup = NULL;

            // Attempt to load backup.info
            TRY_BEGIN()
            {
                infoBackup = infoBackupLoadFile(
                    storageRepoIdx(repoIdx), INFO_BACKUP_PATH_FILE_STR, cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx),
                    cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
            }
            CATCH_ANY()
            {
                LOG_WARN_FMT("%s: [%s] %s", cfgOptionGroupName(cfgOptGrpRepo, repoIdx), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();

            // If unable to load the backup info file, then move on to next repo
            if (infoBackup == NULL)
                continue;

            if (infoBackupDataTotal(infoBackup) == 0)
            {
                LOG_WARN_FMT(
                    "%s: [%s] no backup sets to restore", cfgOptionGroupName(cfgOptGrpRepo, repoIdx),
                    errorTypeName(&BackupSetInvalidError));
                continue;
            }

            // If a backup set was not specified, then see if a target was requested
            if (backupSetRequested == NULL)
            {
                // Get the latest backup
                const InfoBackupData latestBackup = infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1);

                // If a target was requested, attempt to determine the backup set
                if (targetType == CFGOPTVAL_RESTORE_TYPE_TIME || targetType == CFGOPTVAL_RESTORE_TYPE_LSN)
                {
                    bool found = false;

                    // Search current backups from newest to oldest
                    for (unsigned int keyIdx = infoBackupDataTotal(infoBackup) - 1; (int)keyIdx >= 0; keyIdx--)
                    {
                        // Get the backup data
                        const InfoBackupData backupData = infoBackupData(infoBackup, keyIdx);

                        // If target is lsn and no backupLsnStop exists, exit this repo and log that backup may be manually selected
                        if (targetType == CFGOPTVAL_RESTORE_TYPE_LSN && !backupData.backupLsnStop)
                        {
                            LOG_WARN_FMT(
                                "%s reached backup from prior version missing required LSN info before finding a match -- backup"
                                " auto-select has been disabled for this repo\n"
                                "HINT: you may specify a backup to restore using the --set option.",
                                cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

                            break;
                        }

                        // If the end of the backup is valid for the target, then select this backup
                        if ((targetType == CFGOPTVAL_RESTORE_TYPE_TIME && backupData.backupTimestampStop < target.time) ||
                            (targetType == CFGOPTVAL_RESTORE_TYPE_LSN && pgLsnFromStr(backupData.backupLsnStop) <= target.lsn))
                        {
                            found = true;

                            result = restoreBackupData(backupData.backupLabel, repoIdx, infoPgCipherPass(infoBackupPg(infoBackup)));
                            break;
                        }
                    }

                    // If a backup was found on this repo matching the criteria for time then exit
                    if (found)
                        break;
                }
                // Else use backup set found
                else
                {
                    // Is this backup part of the latest pg history?
                    const InfoPgData backupInfoPg = infoPgData(
                        infoBackupPg(infoBackup), infoPgDataCurrentId(infoBackupPg(infoBackup)));

                    if (latestBackup.backupPgId < backupInfoPg.id)
                    {
                        THROW_FMT(
                            BackupSetInvalidError,
                            "the latest backup set found '%s' is from a prior version of " PG_NAME "\n"
                            "HINT: was a backup created after the stanza-upgrade?\n"
                            "HINT: specify --" CFGOPT_SET " or --" CFGOPT_TYPE "=time/lsn to restore from a prior version of"
                            " " PG_NAME ".",
                            strZ(latestBackup.backupLabel));
                    }

                    result = restoreBackupData(latestBackup.backupLabel, repoIdx, infoPgCipherPass(infoBackupPg(infoBackup)));
                    break;
                }
            }
            // Otherwise check to see if the specified backup set is on this repo
            else
            {
                for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
                {
                    if (strEq(infoBackupData(infoBackup, backupIdx).backupLabel, backupSetRequested))
                    {
                        result = restoreBackupData(backupSetRequested, repoIdx, infoPgCipherPass(infoBackupPg(infoBackup)));
                        break;
                    }
                }

                // If the backup set is found, then exit, else continue to next repo
                if (result.backupSet != NULL)
                    break;
            }
        }

        // Still no backup set to use after checking all the repos required to be checked?
        if (result.backupSet == NULL)
        {
            if (backupSetRequested != NULL)
                THROW_FMT(BackupSetInvalidError, "backup set %s is not valid", strZ(backupSetRequested));
            else if (targetType == CFGOPTVAL_RESTORE_TYPE_TIME || targetType == CFGOPTVAL_RESTORE_TYPE_LSN)
            {
                THROW_FMT(
                    BackupSetInvalidError, "unable to find backup set with %s '%s'",
                    targetType == CFGOPTVAL_RESTORE_TYPE_LSN ? "lsn less than or equal to" : "stop time less than",
                    strZ(cfgOptionDisplay(cfgOptTarget)));
            }
            else
                THROW(BackupSetInvalidError, "no backup set found to restore");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Generate the expression to zero files that are not needed for selective restore
***********************************************************************************************************************************/
static String *
restoreSelectiveExpression(const Manifest *const manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    String *result = NULL;

    // Continue if databases to include or exclude have been specified
    if (cfgOptionTest(cfgOptDbExclude) || cfgOptionTest(cfgOptDbInclude))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Generate base expression
            RegExp *const baseRegExp = regExpNew(STRDEF("^" MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/[0-9]+/" PG_FILE_PGVERSION));

            // Generate tablespace expression
            const String *const tablespaceId = pgTablespaceId(
                manifestData(manifest)->pgVersion, manifestData(manifest)->pgCatalogVersion);
            RegExp *const tablespaceRegExp = regExpNew(
                strNewFmt("^" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+/" PG_FILE_PGVERSION, strZ(tablespaceId)));

            // Generate a list of databases in base or in a tablespace and get all standard system databases, even in cases where
            // users have recreated them
            StringList *systemDbIdList = strLstNew();
            StringList *dbList = strLstNew();

            for (unsigned int systemDbIdx = 0; systemDbIdx < manifestDbTotal(manifest); systemDbIdx++)
            {
                const ManifestDb *const systemDb = manifestDb(manifest, systemDbIdx);

                if (pgDbIsSystem(systemDb->name) || pgDbIsSystemId(systemDb->id))
                {
                    // Build the system id list and add to the dbList for logging and checking
                    const String *const systemDbId = varStrForce(VARUINT(systemDb->id));

                    strLstAdd(systemDbIdList, systemDbId);
                    strLstAdd(dbList, systemDbId);
                }
            }

            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const String *const fileName = manifestFileNameGet(manifest, fileIdx);

                if (regExpMatch(baseRegExp, fileName) || regExpMatch(tablespaceRegExp, fileName))
                {
                    const String *const dbId = strBase(strPath(fileName));

                    // In the highly unlikely event that a system database was somehow added after the backup began, it will only be
                    // found in the file list and not the manifest db section, so add it to the system database list
                    if (pgDbIsSystemId(cvtZToUInt(strZ(dbId))))
                        strLstAddIfMissing(systemDbIdList, dbId);

                    strLstAddIfMissing(dbList, dbId);
                }
            }

            strLstSort(dbList, sortOrderAsc);

            // If no databases were found then this backup is not a valid cluster
            if (strLstEmpty(dbList))
                THROW(FormatError, "no databases found for selective restore\nHINT: is this a valid cluster?");

            // Log databases found
            LOG_DETAIL_FMT("databases found for selective restore (%s)", strZ(strLstJoin(dbList, ", ")));

            // Generate list with ids of databases to exclude
            StringList *const excludeDbIdList = strLstNew();
            const StringList *const excludeList = strLstNewVarLst(cfgOptionLst(cfgOptDbExclude));

            for (unsigned int excludeIdx = 0; excludeIdx < strLstSize(excludeList); excludeIdx++)
            {
                const String *excludeDb = strLstGet(excludeList, excludeIdx);

                // If the db to exclude is not in the list as an id then search by name
                if (!strLstExists(dbList, excludeDb))
                {
                    const ManifestDb *const db = manifestDbFindDefault(manifest, excludeDb, NULL);

                    if (db == NULL || !strLstExists(dbList, varStrForce(VARUINT(db->id))))
                        THROW_FMT(DbMissingError, "database to exclude '%s' does not exist", strZ(excludeDb));

                    // Set the exclude db to the id if the name mapping was successful
                    excludeDb = varStrForce(VARUINT(db->id));
                }

                // Add to exclude list
                strLstAdd(excludeDbIdList, excludeDb);
            }

            // Remove included databases from the list
            const StringList *const includeList = strLstNewVarLst(cfgOptionLst(cfgOptDbInclude));

            for (unsigned int includeIdx = 0; includeIdx < strLstSize(includeList); includeIdx++)
            {
                const String *includeDb = strLstGet(includeList, includeIdx);

                // If the db to include is not in the list as an id then search by name
                if (!strLstExists(dbList, includeDb))
                {
                    const ManifestDb *const db = manifestDbFindDefault(manifest, includeDb, NULL);

                    if (db == NULL || !strLstExists(dbList, varStrForce(VARUINT(db->id))))
                        THROW_FMT(DbMissingError, "database to include '%s' does not exist", strZ(includeDb));

                    // Set the include db to the id if the name mapping was successful
                    includeDb = varStrForce(VARUINT(db->id));
                }

                // Error if the db is a system db
                if (strLstExists(systemDbIdList, includeDb))
                    THROW(DbInvalidError, "system databases (template0, postgres, etc.) are included by default");

                // Error if the db id is in the exclude list
                if (strLstExists(excludeDbIdList, includeDb))
                    THROW_FMT(DbInvalidError, "database to include '%s' is in the exclude list", strZ(includeDb));

                // Remove from list of DBs to zero
                strLstRemove(dbList, includeDb);
            }

            // Only exclude specified db in case no db to include has been provided
            if (strLstEmpty(includeList))
            {
                dbList = strLstDup(excludeDbIdList);
            }
            // Else, remove the system databases from list of DBs to zero unless they are excluded explicitly
            else
            {
                strLstSort(systemDbIdList, sortOrderAsc);
                strLstSort(excludeDbIdList, sortOrderAsc);
                systemDbIdList = strLstMergeAnti(systemDbIdList, excludeDbIdList);
                dbList = strLstMergeAnti(dbList, systemDbIdList);
            }

            // Build regular expression to identify files that will be zeroed
            String *expression = NULL;

            if (!strLstEmpty(dbList))
            {
                LOG_DETAIL_FMT("databases excluded (zeroed) from selective restore (%s)", strZ(strLstJoin(dbList, ", ")));

                // Generate the expression from the list of databases to be zeroed. Only user created databases can be zeroed, never
                // system databases.
                for (unsigned int dbIdx = 0; dbIdx < strLstSize(dbList); dbIdx++)
                {
                    const String *const db = strLstGet(dbList, dbIdx);

                    // Create expression string or append |
                    if (expression == NULL)
                        expression = strNew();
                    else
                        strCatZ(expression, "|");

                    // Filter files in base directory
                    strCatFmt(expression, "(^" MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/%s/)", strZ(db));

                    // Filter files in tablespace directories
                    for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
                    {
                        const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

                        if (target->tablespaceId != 0)
                            strCatFmt(expression, "|(^%s/%s/%s/)", strZ(target->name), strZ(tablespaceId), strZ(db));
                    }
                }
            }

            // If all user databases have been selected then nothing to do
            if (expression == NULL)
            {
                LOG_INFO_FMT("nothing to filter - all user databases have been selected");
            }
            // Else return the expression
            else
            {
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = strDup(expression);
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(STRING, result);
}
