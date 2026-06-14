/***********************************************************************************************************************************
Recovery constants
***********************************************************************************************************************************/
#define RESTORE_COMMAND                                             "restore_command"
STRING_STATIC(RESTORE_COMMAND_STR,                                  RESTORE_COMMAND);

#define RECOVERY_TARGET                                             "recovery_target"
#define RECOVERY_TARGET_LSN                                         "recovery_target_lsn"
#define RECOVERY_TARGET_NAME                                        "recovery_target_name"
#define RECOVERY_TARGET_TIME                                        "recovery_target_time"
#define RECOVERY_TARGET_XID                                         "recovery_target_xid"

#define RECOVERY_TARGET_ACTION                                      "recovery_target_action"
#define RECOVERY_TARGET_INCLUSIVE                                   "recovery_target_inclusive"

#define RECOVERY_TARGET_TIMELINE                                    "recovery_target_timeline"
#define RECOVERY_TARGET_TIMELINE_CURRENT                            "current"

#define STANDBY_MODE                                                "standby_mode"
STRING_STATIC(STANDBY_MODE_STR,                                     STANDBY_MODE);

#define ARCHIVE_MODE                                                "archive_mode"

/***********************************************************************************************************************************
Generate the recovery file
***********************************************************************************************************************************/
// Helper to generate recovery options
static KeyValue *
restoreRecoveryOption(const unsigned int pgVersion)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
    FUNCTION_LOG_END();

    KeyValue *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = kvNew();

        StringList *recoveryOptionKey = strLstNew();

        if (cfgOptionTest(cfgOptRecoveryOption))
        {
            const KeyValue *const recoveryOption = cfgOptionKv(cfgOptRecoveryOption);
            recoveryOptionKey = strLstSort(strLstNewVarLst(kvKeyList(recoveryOption)), sortOrderAsc);

            for (unsigned int keyIdx = 0; keyIdx < strLstSize(recoveryOptionKey); keyIdx++)
            {
                // Get the key and value
                String *const key = strLstGet(recoveryOptionKey, keyIdx);
                const String *const value = varStr(kvGet(recoveryOption, VARSTR(key)));

                // Replace - in key with _. Since we use - users naturally will as well.
                strReplaceChr(key, '-', '_');

                kvPut(result, VARSTR(key), VARSTR(value));
            }

            strLstSort(recoveryOptionKey, sortOrderAsc);
        }

        // If archive-mode is not preserve
        if (cfgOptionSeq(cfgOptArchiveMode) != CFGOPTVAL_ARCHIVE_MODE_PRESERVE)
        {
            if (pgVersion < PG_VERSION_12)
            {
                THROW_FMT(
                    OptionInvalidError,
                    "option '" CFGOPT_ARCHIVE_MODE "' is not supported on " PG_NAME " < " PG_VERSION_12_Z "\n"
                    "HINT: 'archive_mode' should be manually set to 'off' in postgresql.conf.");
            }

            // The only other valid option is off
            ASSERT(cfgOptionSeq(cfgOptArchiveMode) == CFGOPTVAL_ARCHIVE_MODE_OFF);

            // If archive-mode=off then set archive_mode=off
            kvPut(result, VARSTRDEF(ARCHIVE_MODE), VARSTRDEF(CFGOPTVAL_ARCHIVE_MODE_OFF_Z));
        }

        // Write restore_command
        if (!strLstExists(recoveryOptionKey, RESTORE_COMMAND_STR))
        {
            // Null out options that it does not make sense to pass from the restore command to archive-get. All of these have
            // reasonable defaults so there is no danger of an error -- they just might not be optimal. In any case, it seems better
            // than, for example, passing --process-max=32 to archive-get because it was specified for restore.
            KeyValue *const optionReplace = kvNew();

            kvPut(optionReplace, VARSTRDEF(CFGOPT_EXEC_ID), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_JOB_RETRY), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_JOB_RETRY_INTERVAL), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_CONSOLE), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_FILE), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_STDERR), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_SUBPROCESS), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_TIMESTAMP), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_PROCESS_MAX), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_CMD), NULL);

            kvPut(
                result, VARSTRDEF(RESTORE_COMMAND),
                VARSTR(
                    strNewFmt(
                        "%s %s %%f \"%%p\"", strZ(cfgOptionStr(cfgOptCmd)),
                        strZ(strLstJoin(cfgExecParam(cfgCmdArchiveGet, cfgCmdRoleMain, optionReplace, true, true), " ")))));
        }

        // If recovery type is immediate
        if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_IMMEDIATE)
        {
            kvPut(result, VARSTRDEF(RECOVERY_TARGET), VARSTRDEF(CFGOPTVAL_RESTORE_TYPE_IMMEDIATE_Z));
        }
        // Else recovery type is standby
        else if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_STANDBY)
        {
            // Write standby_mode for PostgreSQL versions that support it
            if (pgVersion < PG_VERSION_RECOVERY_GUC)
                kvPut(result, VARSTR(STANDBY_MODE_STR), VARSTRDEF("on"));
        }
        // Else recovery type is not default so write target options
        else if (cfgOptionSeq(cfgOptType) != CFGOPTVAL_RESTORE_TYPE_DEFAULT)
        {
            // Write the recovery target
            kvPut(
                result, VARSTR(strNewFmt(RECOVERY_TARGET "_%s", strZ(cfgOptionDisplay(cfgOptType)))),
                VARSTR(cfgOptionStr(cfgOptTarget)));

            // Write recovery_target_inclusive
            if (cfgOptionTest(cfgOptTargetExclusive) && cfgOptionBool(cfgOptTargetExclusive))
                kvPut(result, VARSTRDEF(RECOVERY_TARGET_INCLUSIVE), VARSTR(FALSE_STR));
        }

        // Write recovery_target_action
        if (cfgOptionTest(cfgOptTargetAction))
        {
            const StringId targetAction = cfgOptionStrId(cfgOptTargetAction);

            if (targetAction != CFGOPTVAL_TARGET_ACTION_PAUSE)
            {
                kvPut(result, VARSTRDEF(RECOVERY_TARGET_ACTION), VARSTR(strNewStrId(targetAction)));
            }
        }

        // Write recovery_target_timeline if set
        if (cfgOptionTest(cfgOptTargetTimeline))
        {
            // Do not set current when PostgreSQL < 12 since this is the default and if current is explicitly set it acts as latest
            if (pgVersion >= PG_VERSION_12 || !strEqZ(cfgOptionStr(cfgOptTargetTimeline), RECOVERY_TARGET_TIMELINE_CURRENT))
                kvPut(result, VARSTRDEF(RECOVERY_TARGET_TIMELINE), VARSTR(cfgOptionStr(cfgOptTargetTimeline)));
        }
        // Else explicitly set target timeline to "current" when type=immediate and PostgreSQL >= 12. We do this because
        // type=immediate means there won't be any actual attempt to change timelines, but if we leave the target timeline as the
        // default of "latest" then PostgreSQL might fail to restore because it can't reach the "latest" timeline in the repository
        // from this backup.
        //
        // This is really a PostgreSQL bug and will hopefully be addressed there, but we'll handle it here for older versions, at
        // least until they aren't really seen in the wild any longer.
        //
        // PostgreSQL < 12 defaults to "current" (but does not accept "current" as a parameter) so no need set it explicitly.
        else if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_IMMEDIATE && pgVersion >= PG_VERSION_12)
            kvPut(result, VARSTRDEF(RECOVERY_TARGET_TIMELINE), VARSTRDEF(RECOVERY_TARGET_TIMELINE_CURRENT));

        // Move to prior context
        kvMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, result);
}

// Helper to convert recovery options to text format
static String *
restoreRecoveryConf(const unsigned int pgVersion, const String *const restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        strCatFmt(result, "# Recovery settings generated by " PROJECT_NAME " restore on %s\n", strZ(restoreLabel));

        // Output all recovery options
        const KeyValue *const optionKv = restoreRecoveryOption(pgVersion);
        const VariantList *const optionKeyList = kvKeyList(optionKv);

        for (unsigned int optionKeyIdx = 0; optionKeyIdx < varLstSize(optionKeyList); optionKeyIdx++)
        {
            const Variant *const optionKey = varLstGet(optionKeyList, optionKeyIdx);

            strCatFmt(result, "%s = '%s'\n", strZ(varStr(optionKey)), strZ(varStr(kvGet(optionKv, optionKey))));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

// Helper to write recovery options into recovery.conf
static void
restoreRecoveryWriteConf(
    const Manifest *const manifest, const StorageInfo *const fileInfo, const unsigned int pgVersion,
    const String *const restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STORAGE_INFO, fileInfo);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    // Only write recovery.conf if recovery type != none
    if (cfgOptionSeq(cfgOptType) != CFGOPTVAL_RESTORE_TYPE_NONE)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            LOG_INFO_FMT("write %s", strZ(storagePathP(storagePg(), PG_FILE_RECOVERYCONF_STR)));

            // Write recovery.conf
            storagePutP(
                storageNewWriteP(
                    storagePgWrite(), PG_FILE_RECOVERYCONF_STR, .noCreatePath = true, .modeFile = fileInfo->mode, .noAtomic = true,
                    .noSyncPath = true, .user = fileInfo->user, .group = fileInfo->group),
                BUFSTR(restoreRecoveryConf(pgVersion, restoreLabel)));
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}

// Helper to write recovery options into postgresql.auto.conf
static void
restoreRecoveryWriteAutoConf(
    const Manifest *const manifest, const StorageInfo *const fileInfo, const unsigned int pgVersion,
    const String *const restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STORAGE_INFO, fileInfo);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const content = strNew();

        // Load postgresql.auto.conf so we can preserve the existing contents
        const Buffer *const autoConf = storageGetP(
            storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR, .ignoreMissing = true));

        // It is unusual for the file not to exist, but we'll continue processing by creating a blank file
        if (autoConf == NULL)
        {
            LOG_WARN(PG_FILE_POSTGRESQLAUTOCONF " does not exist -- creating to contain recovery settings");
        }
        // Else the file does exist so comment out old recovery options that could interfere with the current recovery. Don't
        // comment out *all* recovery options because some should only be commented out if there is a new option to replace it, e.g.
        // primary_conninfo. If the option shouldn't be commented out all the time then it won't ever be commented out -- this may
        // not be ideal but it is what was decided. PostgreSQL will use the last value set so this is safe as long as the option
        // does not have dependencies on other options.
        else
        {
            // Generate a regexp that will match on all current recovery_target settings
            RegExp *const recoveryExp =
                regExpNew(
                    STRDEF(
                        "^[\t ]*(" RECOVERY_TARGET "|" RECOVERY_TARGET_ACTION "|" RECOVERY_TARGET_INCLUSIVE "|"
                        RECOVERY_TARGET_LSN "|" RECOVERY_TARGET_NAME "|" RECOVERY_TARGET_TIME "|" RECOVERY_TARGET_TIMELINE "|"
                        RECOVERY_TARGET_XID ")[\t ]*="));

            // Check each line for recovery settings
            const StringList *const contentList = strLstNewSplit(strNewBuf(autoConf), LF_STR);

            for (unsigned int contentIdx = 0; contentIdx < strLstSize(contentList); contentIdx++)
            {
                if (contentIdx != 0)
                    strCat(content, LF_STR);

                const String *const line = strLstGet(contentList, contentIdx);

                if (regExpMatch(recoveryExp, line))
                    strCatFmt(content, "# Removed by " PROJECT_NAME " restore on %s # ", strZ(restoreLabel));

                strCat(content, line);
            }

            // If settings will be appended then format the file so a blank line will be between old and new settings
            if (cfgOptionSeq(cfgOptType) != CFGOPTVAL_RESTORE_TYPE_NONE)
            {
                strTrim(content);
                strCatZ(content, "\n\n");
            }
        }

        // If recovery was requested then write the recovery options
        if (cfgOptionSeq(cfgOptType) != CFGOPTVAL_RESTORE_TYPE_NONE)
        {
            // If the user specified standby_mode as a recovery option then error. It's tempting to just set type=standby in this
            // case but since config parsing has already happened the target options could be in an invalid state.
            if (cfgOptionTest(cfgOptRecoveryOption))
            {
                const KeyValue *const recoveryOption = cfgOptionKv(cfgOptRecoveryOption);
                const StringList *const recoveryOptionKey = strLstNewVarLst(kvKeyList(recoveryOption));

                for (unsigned int keyIdx = 0; keyIdx < strLstSize(recoveryOptionKey); keyIdx++)
                {
                    // Get the key and value
                    String *const key = strLstGet(recoveryOptionKey, keyIdx);

                    // Replace - in key with _. Since we use - users naturally will as well.
                    strReplaceChr(key, '-', '_');

                    if (strEq(key, STANDBY_MODE_STR))
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            "'" STANDBY_MODE "' setting is not valid for " PG_NAME " >= %s\n"
                            "HINT: use --" CFGOPT_TYPE "=" CFGOPTVAL_RESTORE_TYPE_STANDBY_Z " instead of --" CFGOPT_RECOVERY_OPTION
                            "=" STANDBY_MODE "=on.",
                            strZ(pgVersionToStr(PG_VERSION_RECOVERY_GUC)));
                    }
                }
            }

            strCat(content, restoreRecoveryConf(pgVersion, restoreLabel));
        }

        LOG_INFO_FMT(
            "write %s%s", autoConf == NULL ? "" : "updated ", strZ(storagePathP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR)));

        // Write postgresql.auto.conf
        storagePutP(
            storageNewWriteP(
                storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR, .noCreatePath = true, .modeFile = fileInfo->mode,
                .noAtomic = true, .noSyncPath = true, .user = fileInfo->user, .group = fileInfo->group),
            BUFSTR(content));

        // The standby.signal file is required for standby mode
        if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_STANDBY)
        {
            storagePutP(
                storageNewWriteP(
                    storagePgWrite(), PG_FILE_STANDBYSIGNAL_STR, .noCreatePath = true, .modeFile = fileInfo->mode,
                    .noAtomic = true, .noSyncPath = true, .user = fileInfo->user, .group = fileInfo->group),
                NULL);
        }
        // Else the recovery.signal file is required for targeted recovery. Skip writing this file if the backup was offline and
        // recovery type is none since PostgreSQL will error in this case when wal_level=minimal.
        else if (cfgOptionSeq(cfgOptType) != CFGOPTVAL_RESTORE_TYPE_NONE)
        {
            storagePutP(
                storageNewWriteP(
                    storagePgWrite(), PG_FILE_RECOVERYSIGNAL_STR, .noCreatePath = true, .modeFile = fileInfo->mode,
                    .noAtomic = true, .noSyncPath = true, .user = fileInfo->user, .group = fileInfo->group),
                NULL);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

static void
restoreRecoveryWrite(const Manifest *const manifest, const StorageInfo *const fileInfo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STORAGE_INFO, fileInfo);
    FUNCTION_LOG_END();

    // Get PostgreSQL version to write recovery for
    const unsigned int pgVersion = manifestData(manifest)->pgVersion;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If recovery type is preserve then leave recovery file as it is
        if (cfgOptionSeq(cfgOptType) == CFGOPTVAL_RESTORE_TYPE_PRESERVE)
        {
            // Determine which file recovery settings will be written to
            const String *const recoveryFile =
                pgVersion >= PG_VERSION_RECOVERY_GUC ? PG_FILE_POSTGRESQLAUTOCONF_STR : PG_FILE_RECOVERYCONF_STR;

            if (!storageExistsP(storagePg(), recoveryFile))
            {
                LOG_WARN_FMT(
                    "recovery type is " CFGOPTVAL_RESTORE_TYPE_PRESERVE_Z " but recovery file does not exist at '%s'",
                    strZ(storagePathP(storagePg(), recoveryFile)));
            }
        }
        // Else write recovery file
        else
        {
            // Generate a label used to identify this restore in the recovery file
            const String *const restoreLabel = strNewTimeP("%Y-%m-%d %H:%M:%S", time(NULL));

            // Write recovery file based on PostgreSQL version
            if (pgVersion >= PG_VERSION_RECOVERY_GUC)
                restoreRecoveryWriteAutoConf(manifest, fileInfo, pgVersion, restoreLabel);
            else
                restoreRecoveryWriteConf(manifest, fileInfo, pgVersion, restoreLabel);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
