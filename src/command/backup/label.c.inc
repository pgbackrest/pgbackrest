/**********************************************************************************************************************************
Generate a unique backup label that does not contain a timestamp from a previous backup
***********************************************************************************************************************************/
static String *
backupLabelCreate(const BackupType type, const String *const backupLabelPrior, const time_t timestamp)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, backupLabelPrior);
        FUNCTION_LOG_PARAM(TIME, timestamp);
    FUNCTION_LOG_END();

    ASSERT((type == backupTypeFull && backupLabelPrior == NULL) || (type != backupTypeFull && backupLabelPrior != NULL));
    ASSERT(timestamp > 0);

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *backupLabelLatest = NULL;

        // Get the newest backup
        const StringList *const backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        if (!strLstEmpty(backupList))
            backupLabelLatest = strLstGet(backupList, 0);

        // Get the newest history
        const StringList *const historyYearList = strLstSort(
            storageListP(storageRepo(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY), .expression = STRDEF("^2[0-9]{3}$")),
            sortOrderDesc);

        if (!strLstEmpty(historyYearList))
        {
            // For full backup compare against all backups in the history. For other backup types find backups whose name begins
            // with full backup part of backupLabelLatest. This prevents label generation from failing when the last full backup is
            // removed and then an diff/incr is generated from the last remaining full backup.
            const String *const fileNameRegExp =
                (type == backupTypeFull) ?
                    backupRegExpP(.full = true, .differential = true, .incremental = true, .noAnchorEnd = true) :
                    strNewFmt("^%.*sF\\_" DATE_TIME_REGEX "(D|I)", DATE_TIME_LEN, strZ(backupLabelLatest));
            const StringList *const historyList = strLstSort(
                storageListP(
                    storageRepo(),
                    strNewFmt(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY "/%s", strZ(strLstGet(historyYearList, 0))),
                    .expression = strNewFmt("%s\\.manifest\\.%s$", strZ(fileNameRegExp), strZ(compressTypeStr(compressTypeGz)))),
                sortOrderDesc);

            if (!strLstEmpty(historyList))
            {
                const String *const historyLabelLatest = strLstGet(historyList, 0);

                if (backupLabelLatest == NULL || strCmp(historyLabelLatest, backupLabelLatest) > 0)
                {
                    // Strip off the compression and manifest extensions in case this ends up in an error message
                    backupLabelLatest = compressExtStrip(historyLabelLatest, compressTypeFromName(historyLabelLatest));
                    backupLabelLatest = strSubN(backupLabelLatest, 0, strSize(backupLabelLatest) - sizeof(BACKUP_MANIFEST_EXT) + 1);
                }
            }
        }

        // Now that we have the latest label check if the provided timestamp will give us an even later label
        result = backupLabelFormat(type, backupLabelPrior, timestamp);

        if (backupLabelLatest != NULL && strCmp(result, backupLabelLatest) <= 0)
        {
            // If that didn't give us a later label then add one second. It's possible that two backups (they would need to be
            // offline or halted online) have run very close together.
            result = backupLabelFormat(type, backupLabelPrior, timestamp + 1);

            // If the label is still not latest then error. There is probably a timezone change or massive clock skew.
            if (strCmp(result, backupLabelLatest) <= 0)
            {
                THROW_FMT(
                    ClockError,
                    "new backup label '%s' is not later than latest backup label '%s'\n"
                    "HINT: has the timezone changed?\n"
                    "HINT: is there clock skew?",
                    strZ(result), strZ(backupLabelLatest));
            }

            // If adding a second worked then sleep the remainder of the current second so we don't start early
            sleepMSec(MSEC_PER_SEC - (timeMSec() % MSEC_PER_SEC));
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(result);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
