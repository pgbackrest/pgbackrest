/***********************************************************************************************************************************
Restore Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/restore/protocol.h"
#include "command/restore/restore.h"
#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/user.h"
#include "config/config.h"
#include "config/exec.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"
#include "storage/write.intern.h"
#include "version.h"

/***********************************************************************************************************************************
Recovery constants
***********************************************************************************************************************************/
#define RESTORE_COMMAND                                             "restore_command"
    STRING_STATIC(RESTORE_COMMAND_STR,                              RESTORE_COMMAND);

#define RECOVERY_TARGET                                             "recovery_target"
#define RECOVERY_TARGET_LSN                                         "recovery_target_lsn"
#define RECOVERY_TARGET_NAME                                        "recovery_target_name"
#define RECOVERY_TARGET_TIME                                        "recovery_target_time"
#define RECOVERY_TARGET_XID                                         "recovery_target_xid"

#define RECOVERY_TARGET_ACTION                                      "recovery_target_action"

#define RECOVERY_TARGET_INCLUSIVE                                   "recovery_target_inclusive"
#define RECOVERY_TARGET_TIMELINE                                    "recovery_target_timeline"
#define PAUSE_AT_RECOVERY_TARGET                                    "pause_at_recovery_target"
#define STANDBY_MODE                                                "standby_mode"
    STRING_STATIC(STANDBY_MODE_STR,                                 STANDBY_MODE);

#define ARCHIVE_MODE                                                "archive_mode"

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
                    " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                    " exist in the destination directories the restore will be aborted.",
               strZ(cfgOptionDisplay(cfgOptPgPath)));

            // Disable delta and force so restore will fail if the directories are not empty
            cfgOptionSet(cfgOptDelta, cfgSourceDefault, VARBOOL(false));
            cfgOptionSet(cfgOptForce, cfgSourceDefault, VARBOOL(false));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Get epoch time from formatted string
***********************************************************************************************************************************/
static time_t
getEpoch(const String *targetTime)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, targetTime);
    FUNCTION_LOG_END();

    ASSERT(targetTime != NULL);

    time_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build the regex to accept formats: YYYY-MM-DD HH:MM:SS with optional msec (up to 6 digits and separated from minutes by
        // a comma or period), optional timezone offset +/- HH or HHMM or HH:MM, where offset boundaries are UTC-12 to UTC+14
        const String *expression = STRDEF(
            "^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}((\\,|\\.)[0-9]{1,6})?((\\+|\\-)[0-9]{2}(:?)([0-9]{2})?)?$");

        RegExp *regExp = regExpNew(expression);

        // If the target-recovery time matches the regular expression then validate it
        if (regExpMatch(regExp, targetTime))
        {
            // Strip off the date and time and put the remainder into another string
            String *datetime = strSubN(targetTime, 0, 19);

            int dtYear = cvtZToInt(strZ(strSubN(datetime, 0, 4)));
            int dtMonth = cvtZToInt(strZ(strSubN(datetime, 5, 2)));
            int dtDay = cvtZToInt(strZ(strSubN(datetime, 8, 2)));
            int dtHour = cvtZToInt(strZ(strSubN(datetime, 11, 2)));
            int dtMinute = cvtZToInt(strZ(strSubN(datetime, 14, 2)));
            int dtSecond = cvtZToInt(strZ(strSubN(datetime, 17, 2)));

            // Confirm date and time parts are valid
            datePartsValid(dtYear, dtMonth, dtDay);
            timePartsValid(dtHour, dtMinute, dtSecond);

            String *timeTargetZone = strSub(targetTime, 19);

            // Find the + or - indicating a timezone offset was provided (there may be milliseconds before the timezone, so need to
            // skip). If a timezone offset was not provided, then local time is assumed.
            int idxSign = strChr(timeTargetZone, '+');

            if (idxSign == -1)
                idxSign = strChr(timeTargetZone, '-');

            if (idxSign != -1)
            {
                String *timezoneOffset = strSub(timeTargetZone, (size_t)idxSign);

                // Include the sign with the hour
                int tzHour = cvtZToInt(strZ(strSubN(timezoneOffset, 0, 3)));
                int tzMinute = 0;

                // If minutes are included in timezone offset then extract the minutes based on whether a colon separates them from
                // the hour
                if (strSize(timezoneOffset) > 3)
                    tzMinute = cvtZToInt(strZ(strSubN(timezoneOffset, 3 + (strChr(timezoneOffset, ':') == -1 ? 0 : 1), 2)));

                result = epochFromParts(dtYear, dtMonth, dtDay, dtHour, dtMinute, dtSecond, tzOffsetSeconds(tzHour, tzMinute));
            }
            // If there is no timezone offset, then assume it is local time
            else
            {
                // Set tm_isdst to -1 to force mktime to consider if DST. For example, if system time is America/New_York then
                // 2019-09-14 20:02:49 was a time in DST so the Epoch value should be 1568505769 (and not 1568509369 which would be
                // 2019-09-14 21:02:49 - an hour too late)
                result = mktime(
                    &(struct tm){.tm_sec = dtSecond, .tm_min = dtMinute, .tm_hour = dtHour, .tm_mday = dtDay, .tm_mon = dtMonth - 1,
                    .tm_year = dtYear - 1900, .tm_isdst = -1});
            }
        }
        else
        {
            LOG_WARN_FMT(
                "automatic backup set selection cannot be performed with provided time '%s', latest backup set will be used"
                "\nHINT: time format must be YYYY-MM-DD HH:MM:SS with optional msec and optional timezone (+/- HH or HHMM or HH:MM)"
                " - if timezone is omitted, local time is assumed (for UTC use +00)",
                strZ(targetTime));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(TIME, result);
}

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
    objToLog(&value, "RestoreBackupData", buffer, bufferSize)

// Helper function for restoreBackupSet
static RestoreBackupData
restoreBackupData(const String *backupLabel, unsigned int repoIdx, const String *backupCipherPass)
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

        // Initialize a backup candidate list
        List *backupCandidateList = lstNewP(sizeof(RestoreBackupData));

        const String *backupSetRequested = NULL;
        time_t timeTargetEpoch = 0;

        // If the set option was not provided by the user but a time to recover was set, then we will need to search for a backup
        // set that satisfies the time condition, else we will use the backup provided
        if (cfgOptionSource(cfgOptSet) == cfgSourceDefault)
        {
            if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_TIME)
                timeTargetEpoch = getEpoch(cfgOptionStr(cfgOptTarget));
        }
        else
            backupSetRequested = cfgOptionStr(cfgOptSet);

        // Search through the repo list for a backup set to use for recovery
        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            storageRepoIdx(repoIdx);

            InfoBackup *infoBackup = NULL;

            // Attempt to load backup.info
            TRY_BEGIN()
            {
                infoBackup = infoBackupLoadFile(
                    storageRepoIdx(repoIdx), INFO_BACKUP_PATH_FILE_STR,  cfgOptionIdxStrId(cfgOptRepoCipherType, repoIdx),
                    cfgOptionIdxStrNull(cfgOptRepoCipherPass, repoIdx));
            }
            CATCH_ANY()
            {
                LOG_WARN_FMT(
                    "repo%u: [%s] %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx), errorTypeName(errorType()), errorMessage());
            }
            TRY_END();

            // If unable to load the backup info file, then move on to next repo
            if (infoBackup == NULL)
                continue;

            if (infoBackupDataTotal(infoBackup) == 0)
            {
                LOG_WARN_FMT(
                    "repo%u: [%s] no backup sets to restore", cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
                    errorTypeName(&BackupSetInvalidError));
                continue;
            }

            // If a backup set was not specified, then see if a time to recover was requested
            if (backupSetRequested == NULL)
            {
                // Get the latest backup
                InfoBackupData latestBackup = infoBackupData(infoBackup, infoBackupDataTotal(infoBackup) - 1);

                // If the recovery type is time, attempt to determine the backup set
                if (timeTargetEpoch != 0)
                {
                    bool found = false;

                    // Search current backups from newest to oldest
                    for (unsigned int keyIdx = infoBackupDataTotal(infoBackup) - 1; (int)keyIdx >= 0; keyIdx--)
                    {
                        // Get the backup data
                        InfoBackupData backupData = infoBackupData(infoBackup, keyIdx);

                        // If the end of the backup is before the target time, then select this backup
                        if (backupData.backupTimestampStop < timeTargetEpoch)
                        {
                            found = true;

                            result = restoreBackupData(
                                backupData.backupLabel, repoIdx, infoPgCipherPass(infoBackupPg(infoBackup)));
                            break;
                        }
                    }

                    // If a backup was found on this repo matching the criteria for time then exit, else determine if the latest
                    // backup from this repo might be used
                    if (found)
                        break;
                    else
                    {
                        // If a backup was not yet found then set the latest from this repo as the backup that might be used
                        RestoreBackupData candidate = restoreBackupData(
                            latestBackup.backupLabel, repoIdx, infoPgCipherPass(infoBackupPg(infoBackup)));

                        lstAdd(backupCandidateList, &candidate);
                    }
                }
                else
                {
                    // If the recovery type was not time (or time provided was not valid), then use the latest backup from this repo
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
            else if (timeTargetEpoch != 0 && lstSize(backupCandidateList) > 0)
            {
                // Since the repos were scanned in priority order, use the first candidate found
                result = restoreBackupData(
                    ((RestoreBackupData *)lstGet(backupCandidateList, 0))->backupSet,
                    ((RestoreBackupData *)lstGet(backupCandidateList, 0))->repoIdx,
                    ((RestoreBackupData *)lstGet(backupCandidateList, 0))->backupCipherPass);

                LOG_WARN_FMT(
                    "unable to find backup set with stop time less than '%s', repo%u: latest backup set will be used",
                    strZ(cfgOptionDisplay(cfgOptTarget)), cfgOptionGroupIdxToKey(cfgOptGrpRepo, result.repoIdx));
            }
            else
                THROW(BackupSetInvalidError, "no backup set found to restore");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/***********************************************************************************************************************************
Validate the manifest
***********************************************************************************************************************************/
static void
restoreManifestValidate(Manifest *manifest, const String *backupSet)
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
        CHECK(manifestFileTotal(manifest) > 0);

        // Sanity check to ensure the manifest has not been moved to a new directory
        const ManifestData *data = manifestData(manifest);

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

/***********************************************************************************************************************************
Remap the manifest based on mappings provided by the user
***********************************************************************************************************************************/
static void
restoreManifestMap(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Remap the data directory
        // -------------------------------------------------------------------------------------------------------------------------
        const String *pgPath = cfgOptionStr(cfgOptPgPath);
        const ManifestTarget *targetBase = manifestTargetBase(manifest);

        if (!strEq(targetBase->path, pgPath))
        {
            LOG_INFO_FMT("remap data directory to '%s'", strZ(pgPath));
            manifestTargetUpdate(manifest, targetBase->name, pgPath, NULL);
        }

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        const KeyValue *const tablespaceMap = cfgOptionKvNull(cfgOptTablespaceMap);
        const String *tablespaceMapAllPath = cfgOptionStrNull(cfgOptTablespaceMapAll);

        if (tablespaceMap != NULL || tablespaceMapAllPath != NULL)
        {
            StringList *tablespaceRemapped = strLstNew();

            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
            {
                const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                // Is this a tablespace?
                if (target->tablespaceId != 0)
                {
                    const String *tablespacePath = NULL;

                    // Check for an individual mapping for this tablespace
                    if (tablespaceMap != NULL)
                    {
                        // Attempt to get the tablespace by name
                        const String *tablespacePathByName = varStr(kvGet(tablespaceMap, VARSTR(target->tablespaceName)));

                        if (tablespacePathByName != NULL)
                            strLstAdd(tablespaceRemapped, target->tablespaceName);

                        // Attempt to get the tablespace by id
                        const String *tablespacePathById = varStr(
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
                const VariantList *tablespaceMapList = kvKeyList(tablespaceMap);
                strLstSort(tablespaceRemapped, sortOrderAsc);

                for (unsigned int tablespaceMapIdx = 0; tablespaceMapIdx < varLstSize(tablespaceMapList); tablespaceMapIdx++)
                {
                    const String *tablespace = varStr(varLstGet(tablespaceMapList, tablespaceMapIdx));

                    if (!strLstExists(tablespaceRemapped, tablespace))
                        THROW_FMT(TablespaceMapError, "unable to remap invalid tablespace '%s'", strZ(tablespace));
                }
            }

            // Issue a warning message when remapping tablespaces in PostgreSQL < 9.2
            if (manifestData(manifest)->pgVersion <= PG_VERSION_92)
                LOG_WARN("update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= " PG_VERSION_92_STR);
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
                    const ManifestPath *const path = manifestPathFindDefault(manifest, manifestName, NULL);
                    const ManifestFile *const file = manifestFileFindDefault(manifest, manifestName, NULL);

                    CHECK(path == NULL || file == NULL);

                    target = (ManifestTarget){.name = manifestName, .path = linkPath, .type = manifestTargetTypeLink};

                    // If a file
                    if (file != NULL)
                    {
                        // File needs to be set so the file/path is updated later but set it to something invalid just in case it
                        // it does not get updated due to a regression
                        target.file = DOT_STR;
                    }
                    // Else error if not a path
                    else if (path == NULL)
                    {
                        THROW_FMT(
                            LinkMapError,
                            "unable to map link '%s'\n"
                            "HINT: Does the link reference a valid backup path or file?",
                            strZ(link));
                    }

                    // Add the link. Copy user/group from the base data directory.
                    const ManifestPath *const pathBase = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);

                    manifestLinkAdd(
                        manifest,
                        &(ManifestLink){
                            .name = manifestName, .destination = linkPath, .group = pathBase->group, .user = pathBase->user});

                    create = true;
                }
                // Else update target to new path
                else
                    target.path = linkPath;

                // The target must be a link since pg_data/ was prepended and pgdata is the only allowed path
                CHECK(target.type == manifestTargetTypeLink);

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
            for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
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
                        targetIdx--;
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Check ownership of items in the manifest
***********************************************************************************************************************************/
// Helper to get list of owners from a file/link/path list
#define RESTORE_MANIFEST_OWNER_GET(type)                                                                                           \
    for (unsigned int itemIdx = 0; itemIdx < manifest##type##Total(manifest); itemIdx++)                                           \
    {                                                                                                                              \
        Manifest##type *item = (Manifest##type *)manifest##type(manifest, itemIdx);                                                \
                                                                                                                                   \
        if (item->user == NULL)                                                                                                    \
            userNull = true;                                                                                                       \
        else                                                                                                                       \
            strLstAddIfMissing(userList, item->user);                                                                              \
                                                                                                                                   \
        if (item->group == NULL)                                                                                                   \
            groupNull = true;                                                                                                      \
        else                                                                                                                       \
            strLstAddIfMissing(groupList, item->group);                                                                            \
                                                                                                                                   \
        if (!userRoot())                                                                                                           \
        {                                                                                                                          \
            item->user = NULL;                                                                                                     \
            item->group = NULL;                                                                                                    \
        }                                                                                                                          \
    }

// Helper to update an owner in a file/link/path list
#define RESTORE_MANIFEST_OWNER_NULL_UPDATE(type, user, group)                                                                      \
    for (unsigned int itemIdx = 0; itemIdx < manifest##type##Total(manifest); itemIdx++)                                           \
    {                                                                                                                              \
        Manifest##type *item = (Manifest##type *)manifest##type(manifest, itemIdx);                                                \
                                                                                                                                   \
        if (item->user == NULL)                                                                                                    \
            item->user = user;                                                                                                     \
                                                                                                                                   \
        if (item->group == NULL)                                                                                                   \
            item->group = group;                                                                                                   \
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
            const String *owner = strLstGet(type##List, ownerIdx);                                                                 \
                                                                                                                                   \
            if (type##Name() == NULL ||  !strEq(type##Name(), owner))                                                              \
                LOG_WARN_FMT("unknown " #type " '%s' in backup manifest mapped to current " #type, strZ(owner));                   \
        }                                                                                                                          \
    }                                                                                                                              \
    while (0)

static void
restoreManifestOwner(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Build a list of users and groups in the manifest
        // -------------------------------------------------------------------------------------------------------------------------
        bool userNull = false;
        StringList *userList = strLstNew();
        bool groupNull = false;
        StringList *groupList = strLstNew();

        RESTORE_MANIFEST_OWNER_GET(File);
        RESTORE_MANIFEST_OWNER_GET(Link);
        RESTORE_MANIFEST_OWNER_GET(Path);

        // Update users and groups in the manifest (this can only be done as root)
        // -------------------------------------------------------------------------------------------------------------------------
        if (userRoot())
        {
            // Get user/group info from data directory to use for invalid user/groups
            StorageInfo pathInfo = storageInfoP(storagePg(), manifestTargetBase(manifest)->path);

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
                    const String *user = strDup(pathInfo.user);
                    const String *group = strDup(pathInfo.group);

                    RESTORE_MANIFEST_OWNER_NULL_UPDATE(File, user, group)
                    RESTORE_MANIFEST_OWNER_NULL_UPDATE(Link, user, group)
                    RESTORE_MANIFEST_OWNER_NULL_UPDATE(Path, user, group)
                }
                MEM_CONTEXT_PRIOR_END();
            }
        }
        // Else set owners to NULL.  This means we won't make any attempt to update ownership and will just leave it as written by
        // the current user/group.  If there are existing files that are not owned by the current user/group then we will attempt
        // to update them, which will generally cause an error, though some systems allow updates to the group ownership.
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
} RestoreCleanCallbackData;

// Helper to update ownership on a file/link/path
static void
restoreCleanOwnership(
    const String *pgPath, const String *manifestUserName, const String *manifestGroupName, uid_t actualUserId, gid_t actualGroupId,
    bool new)
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

    if (manifestUserName != NULL)
    {
        uid_t manifestUserId = userIdFromName(manifestUserName);

        if (manifestUserId != (uid_t)-1)
            expectedUserId = manifestUserId;
    }

    // Get the expected group id
    gid_t expectedGroupId = groupId();

    if (manifestGroupName != NULL)
    {
        uid_t manifestGroupId = groupIdFromName(manifestGroupName);

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
restoreCleanMode(const String *pgPath, mode_t manifestMode, const StorageInfo *info)
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
            chmod(strZ(pgPath), manifestMode) == -1, FileOwnerError, "unable to set mode for '%s'", strZ(pgPath));
    }

    FUNCTION_TEST_RETURN_VOID();
}

// storageInfoList() callback that cleans the paths
static void
restoreCleanInfoListCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    RestoreCleanCallbackData *cleanData = (RestoreCleanCallbackData *)data;

    // Don't include backup.manifest or recovery.conf (when preserved) in the comparison or empty directory check
    if (cleanData->basePath && info->type == storageTypeFile && strLstExists(cleanData->fileIgnore, info->name))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Skip all . paths because they have already been cleaned on the previous level of recursion
    if (strEq(info->name, DOT_STR))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // If this is not a delta then error because the directory is expected to be empty.  Ignore the . path.
    if (!cleanData->delta)
    {
        THROW_FMT(
            PathNotEmptyError,
            "unable to restore to path '%s' because it contains files\n"
            "HINT: try using --delta if this is what you intended.",
            strZ(cleanData->targetPath));
    }

    // Construct the name used to find this file/link/path in the manifest
    const String *manifestName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(info->name));

    // Construct the path of this file/link/path in the PostgreSQL data directory
    const String *pgPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(info->name));

    switch (info->type)
    {
        case storageTypeFile:
        {
            const ManifestFile *manifestFile = manifestFileFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestFile != NULL)
            {
                restoreCleanOwnership(pgPath, manifestFile->user, manifestFile->group, info->userId, info->groupId, false);
                restoreCleanMode(pgPath, manifestFile->mode, info);
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
            const ManifestLink *manifestLink = manifestLinkFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestLink != NULL)
            {
                if (!strEq(manifestLink->destination, info->linkDestination))
                {
                    LOG_DETAIL_FMT("remove link '%s' because destination changed", strZ(pgPath));
                    storageRemoveP(storageLocalWrite(), pgPath, .errorOnMissing = true);
                }
                else
                    restoreCleanOwnership(pgPath, manifestLink->user, manifestLink->group, info->userId, info->groupId, false);
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
            const ManifestPath *manifestPath = manifestPathFindDefault(cleanData->manifest, manifestName, NULL);

            if (manifestPath != NULL)
            {
                // Check ownership/permissions
                restoreCleanOwnership(pgPath, manifestPath->user, manifestPath->group, info->userId, info->groupId, false);
                restoreCleanMode(pgPath, manifestPath->mode, info);

                // Recurse into the path
                RestoreCleanCallbackData cleanDataSub = *cleanData;
                cleanDataSub.targetName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(info->name));
                cleanDataSub.targetPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(info->name));
                cleanDataSub.basePath = false;

                storageInfoListP(
                    storageLocalWrite(), cleanDataSub.targetPath, restoreCleanInfoListCallback, &cleanDataSub,
                    .errorOnMissing = true, .sortOrder = sortOrderAsc);
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

    FUNCTION_TEST_RETURN_VOID();
}

static void
restoreCleanBuild(Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Is this a delta restore?
        bool delta = cfgOptionBool(cfgOptDelta) || cfgOptionBool(cfgOptForce);

        // Allocate data for each target
        RestoreCleanCallbackData *cleanDataList = memNew(sizeof(RestoreCleanCallbackData) * manifestTargetTotal(manifest));

        // Step 1: Check permissions and validity (is the directory empty without delta?) if the target directory exists
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *pathChecked = strLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *cleanData = &cleanDataList[targetIdx];

            *cleanData = (RestoreCleanCallbackData)
            {
                .manifest = manifest,
                .target = manifestTarget(manifest, targetIdx),
                .delta = delta,
                .fileIgnore = strLstNew(),
            };

            cleanData->targetName = cleanData->target->name;
            cleanData->targetPath = manifestTargetPath(manifest, cleanData->target);
            cleanData->basePath = strEq(cleanData->targetName, MANIFEST_TARGET_PGDATA_STR);

            // Ignore backup.manifest while cleaning since it may exist from an prior incomplete restore
            strLstAdd(cleanData->fileIgnore, BACKUP_MANIFEST_FILE_STR);

            // Also ignore recovery files when recovery type = preserve
            if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_PRESERVE)
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
                const String *tablespaceId = pgTablespaceId(
                    manifestData(manifest)->pgVersion, manifestData(manifest)->pgCatalogVersion);

                // Only PostgreSQL >= 9.0 has tablespace indentifiers
                if (tablespaceId != NULL)
                {
                    cleanData->targetName = strNewFmt("%s/%s", strZ(cleanData->targetName), strZ(tablespaceId));
                    cleanData->targetPath = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(tablespaceId));
                }
            }

            strLstSort(cleanData->fileIgnore, sortOrderAsc);

            // Check that the path exists.  If not, there's no need to do any cleaning and we'll attempt to create it later.
            // Don't log check for the same path twice.  There can be multiple links to files in the same path, but logging it more
            // than once makes the logs noisy and looks like a bug.
            if (!strLstExists(pathChecked, cleanData->targetPath))
                LOG_DETAIL_FMT("check '%s' exists", strZ(cleanData->targetPath));

            StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .ignoreMissing = true, .followLink = true);
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
                        storageInfoListP(
                            storageLocal(), cleanData->targetPath, restoreCleanInfoListCallback, cleanData,
                            .errorOnMissing = true);
                    }
                    else
                    {
                        const String *file = strNewFmt("%s/%s", strZ(cleanData->targetPath), strZ(cleanData->target->file));

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
        if (manifestFileFindDefault(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_TABLESPACEMAP), NULL) != NULL &&
            manifestData(manifest)->pgVersion >= PG_VERSION_TABLESPACE_MAP)
        {
            LOG_DETAIL_FMT("skip '" PG_FILE_TABLESPACEMAP "' -- tablespace links will be created based on mappings");
            manifestFileRemove(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_TABLESPACEMAP));
        }

        // Skip postgresql.auto.conf if preserve is set and the PostgreSQL version supports recovery GUCs
        if (manifestData(manifest)->pgVersion >= PG_VERSION_RECOVERY_GUC && cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_PRESERVE &&
            manifestFileFindDefault(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF), NULL) != NULL)
        {
            LOG_DETAIL_FMT("skip '" PG_FILE_POSTGRESQLAUTOCONF "' -- recovery type is preserve");
            manifestFileRemove(manifest, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_POSTGRESQLAUTOCONF));
        }

        // Step 2: Clean target directories
        // -------------------------------------------------------------------------------------------------------------------------
        // Delete the pg_control file (if it exists) so the cluster cannot be started if restore does not complete.  Sync the path
        // so the file does not return, zombie-like, in the case of a host crash.
        if (storageExistsP(storagePg(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)))
        {
            LOG_DETAIL_FMT(
                "remove '" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "' so cluster will not start if restore does not complete");
            storageRemoveP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL));
            storagePathSyncP(storagePgWrite(), PG_PATH_GLOBAL_STR);
        }

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            RestoreCleanCallbackData *cleanData = &cleanDataList[targetIdx];

            // Only clean if the target exists
            if (cleanData->exists)
            {
                // Don't clean file links.  It doesn't matter whether the file exists or not since we know it is in the manifest.
                if (cleanData->target->file == NULL)
                {
                    // Only log when doing a delta restore because otherwise the targets should be empty.  We'll still run the clean
                    // to fix permissions/ownership on the target paths.
                    if (delta)
                        LOG_INFO_FMT("remove invalid files/links/paths from '%s'", strZ(cleanData->targetPath));

                    // Check target ownership/permissions
                    const ManifestPath *manifestPath = manifestPathFind(cleanData->manifest, cleanData->targetName);
                    StorageInfo info = storageInfoP(storageLocal(), cleanData->targetPath, .followLink = true);

                    restoreCleanOwnership(
                        cleanData->targetPath, manifestPath->user, manifestPath->group, info.userId, info.groupId, false);
                    restoreCleanMode(cleanData->targetPath, manifestPath->mode, &info);

                    // Clean the target
                    storageInfoListP(
                        storageLocalWrite(), cleanData->targetPath, restoreCleanInfoListCallback, cleanData, .errorOnMissing = true,
                        .sortOrder = sortOrderAsc);
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
                restoreCleanOwnership(cleanData->targetPath, path->user, path->group, userId(), groupId(), true);
            }
        }

        // Step 3: Create missing paths and path links
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
        {
            const ManifestPath *path = manifestPath(manifest, pathIdx);

            // Skip the pg_tblspc path because it only maps to the manifest.  We should remove this in a future release but not much
            // can be done about it for now.
            if (strEq(path->name, MANIFEST_TARGET_PGTBLSPC_STR))
                continue;

            // If this path has been mapped as a link then create a link.  The path has already been created as part of target
            // creation (or it might have already existed).
            const ManifestLink *link = manifestLinkFindDefault(
                manifest,
                strBeginsWith(path->name, MANIFEST_TARGET_PGTBLSPC_STR) ?
                    strNewFmt(MANIFEST_TARGET_PGDATA "/%s", strZ(path->name)) : path->name,
                NULL);

            if (link != NULL)
            {
                const String *pgPath = storagePathP(storagePg(), manifestPathPg(link->name));
                StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the link if it is missing.  If it exists it should already have the correct ownership and destination.
                if (!linkInfo.exists)
                {
                    LOG_DETAIL_FMT("create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));

                    THROW_ON_SYS_ERROR_FMT(
                        symlink(strZ(link->destination), strZ(pgPath)) == -1, FileOpenError,
                        "unable to create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));
                    restoreCleanOwnership(pgPath, link->user, link->group, userId(), groupId(), true);
                }
            }
            // Create the path normally
            else
            {
                const String *pgPath = storagePathP(storagePg(), manifestPathPg(path->name));
                StorageInfo pathInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

                // Create the path if it is missing  If it exists it should already have the correct ownership and mode.
                if (!pathInfo.exists)
                {
                    LOG_DETAIL_FMT("create path '%s'", strZ(pgPath));

                    storagePathCreateP(storagePgWrite(), pgPath, .mode = path->mode, .noParentCreate = true, .errorOnExists = true);
                    restoreCleanOwnership(storagePathP(storagePg(), pgPath), path->user, path->group, userId(), groupId(), true);
                }
            }
        }

        // Step 4: Create file links.  These don't get created during path creation because they do not have a matching path entry.
        // -------------------------------------------------------------------------------------------------------------------------
        for (unsigned int linkIdx = 0; linkIdx < manifestLinkTotal(manifest); linkIdx++)
        {
            const ManifestLink *link = manifestLink(manifest, linkIdx);

            const String *pgPath = storagePathP(storagePg(), manifestPathPg(link->name));
            StorageInfo linkInfo = storageInfoP(storagePg(), pgPath, .ignoreMissing = true);

            // Create the link if it is missing.  If it exists it should already have the correct ownership and destination.
            if (!linkInfo.exists)
            {
                LOG_DETAIL_FMT("create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));

                THROW_ON_SYS_ERROR_FMT(
                    symlink(strZ(link->destination), strZ(pgPath)) == -1, FileOpenError,
                    "unable to create symlink '%s' to '%s'", strZ(pgPath), strZ(link->destination));
                restoreCleanOwnership(pgPath, link->user, link->group, userId(), groupId(), true);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Generate the expression to zero files that are not needed for selective restore
***********************************************************************************************************************************/
static String *
restoreSelectiveExpression(Manifest *manifest)
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
            RegExp *baseRegExp = regExpNew(STRDEF("^" MANIFEST_TARGET_PGDATA "/" PG_PATH_BASE "/[0-9]+/" PG_FILE_PGVERSION));

            // Generate tablespace expression
            RegExp *tablespaceRegExp = NULL;
            const String *tablespaceId = pgTablespaceId(
                manifestData(manifest)->pgVersion, manifestData(manifest)->pgCatalogVersion);

            if (tablespaceId == NULL)
                tablespaceRegExp = regExpNew(STRDEF("^" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/[0-9]+/" PG_FILE_PGVERSION));
            else
            {
                tablespaceRegExp = regExpNew(
                    strNewFmt("^" MANIFEST_TARGET_PGTBLSPC "/[0-9]+/%s/[0-9]+/" PG_FILE_PGVERSION, strZ(tablespaceId)));
            }

            // Generate a list of databases in base or in a tablespace and get all standard system databases, even in cases where
            // users have recreated them
            StringList *systemDbIdList = strLstNew();
            StringList *dbList = strLstNew();

            for (unsigned int systemDbIdx = 0; systemDbIdx < manifestDbTotal(manifest); systemDbIdx++)
            {
                const ManifestDb *systemDb = manifestDb(manifest, systemDbIdx);

                if (strEqZ(systemDb->name, "template0") || strEqZ(systemDb->name, "template1") ||
                    strEqZ(systemDb->name, "postgres") || systemDb->id < PG_USER_OBJECT_MIN_ID)
                {
                    // Build the system id list and add to the dbList for logging and checking
                    const String *systemDbId = varStrForce(VARUINT(systemDb->id));
                    strLstAdd(systemDbIdList, systemDbId);
                    strLstAdd(dbList, systemDbId);
                }
            }

            for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
            {
                const ManifestFile *file = manifestFile(manifest, fileIdx);

                if (regExpMatch(baseRegExp, file->name) || regExpMatch(tablespaceRegExp, file->name))
                {
                    String *dbId = strBase(strPath(file->name));

                    // In the highly unlikely event that a system database was somehow added after the backup began, it will only be
                    // found in the file list and not the manifest db section, so add it to the system database list
                    if (cvtZToUInt64(strZ(dbId)) < PG_USER_OBJECT_MIN_ID)
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
            StringList *excludeDbIdList = strLstNew();
            const StringList *excludeList = strLstNewVarLst(cfgOptionLst(cfgOptDbExclude));

            for (unsigned int excludeIdx = 0; excludeIdx < strLstSize(excludeList); excludeIdx++)
            {
                const String *excludeDb = strLstGet(excludeList, excludeIdx);

                // If the db to exclude is not in the list as an id then search by name
                if (!strLstExists(dbList, excludeDb))
                {
                    const ManifestDb *db = manifestDbFindDefault(manifest, excludeDb, NULL);

                    if (db == NULL || !strLstExists(dbList, varStrForce(VARUINT(db->id))))
                        THROW_FMT(DbMissingError, "database to exclude '%s' does not exist", strZ(excludeDb));

                    // Set the exclude db to the id if the name mapping was successful
                    excludeDb = varStrForce(VARUINT(db->id));
                }

                // Add to exclude list
                strLstAdd(excludeDbIdList, excludeDb);
            }

            // Remove included databases from the list
            const StringList *includeList = strLstNewVarLst(cfgOptionLst(cfgOptDbInclude));

            for (unsigned int includeIdx = 0; includeIdx < strLstSize(includeList); includeIdx++)
            {
                const String *includeDb = strLstGet(includeList, includeIdx);

                // If the db to include is not in the list as an id then search by name
                if (!strLstExists(dbList, includeDb))
                {
                    const ManifestDb *db = manifestDbFindDefault(manifest, includeDb, NULL);

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
                    const String *db = strLstGet(dbList, dbIdx);

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
                        const ManifestTarget *target = manifestTarget(manifest, targetIdx);

                        if (target->tablespaceId != 0)
                        {
                            if (tablespaceId == NULL)
                                strCatFmt(expression, "|(^%s/%s/)", strZ(target->name), strZ(db));
                            else
                                strCatFmt(expression, "|(^%s/%s/%s/)", strZ(target->name), strZ(tablespaceId), strZ(db));
                        }
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

/***********************************************************************************************************************************
Generate the recovery file
***********************************************************************************************************************************/
// Helper to generate recovery options
static KeyValue *
restoreRecoveryOption(unsigned int pgVersion)
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
            const KeyValue *recoveryOption = cfgOptionKv(cfgOptRecoveryOption);
            recoveryOptionKey = strLstSort(strLstNewVarLst(kvKeyList(recoveryOption)), sortOrderAsc);

            for (unsigned int keyIdx = 0; keyIdx < strLstSize(recoveryOptionKey); keyIdx++)
            {
                // Get the key and value
                String *key = strLstGet(recoveryOptionKey, keyIdx);
                const String *value = varStr(kvGet(recoveryOption, VARSTR(key)));

                // Replace - in key with _.  Since we use - users naturally will as well.
                strReplaceChr(key, '-', '_');

                kvPut(result, VARSTR(key), VARSTR(value));
            }

            strLstSort(recoveryOptionKey, sortOrderAsc);
        }

        // If archive-mode is not preserve
        if (cfgOptionStrId(cfgOptArchiveMode) != CFGOPTVAL_ARCHIVE_MODE_PRESERVE)
        {
            if (pgVersion < PG_VERSION_12)
            {
                THROW_FMT(
                    OptionInvalidError,
                    "option '" CFGOPT_ARCHIVE_MODE "' is not supported on " PG_NAME " < " PG_VERSION_12_STR "\n"
                        "HINT: 'archive_mode' should be manually set to 'off' in postgresql.conf.");
            }

            // The only other valid option is off
            ASSERT(cfgOptionStrId(cfgOptArchiveMode) == CFGOPTVAL_ARCHIVE_MODE_OFF);

            // If archive-mode=off then set archive_mode=off
            kvPut(result, VARSTRDEF(ARCHIVE_MODE), VARSTRDEF(CFGOPTVAL_ARCHIVE_MODE_OFF_Z));
        }

        // Write restore_command
        if (!strLstExists(recoveryOptionKey, RESTORE_COMMAND_STR))
        {
            // Null out options that it does not make sense to pass from the restore command to archive-get.  All of these have
            // reasonable defaults so there is no danger of an error -- they just might not be optimal.  In any case, it seems
            // better than, for example, passing --process-max=32 to archive-get because it was specified for restore.
            KeyValue *optionReplace = kvNew();

            kvPut(optionReplace, VARSTRDEF(CFGOPT_EXEC_ID), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_JOB_RETRY), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_JOB_RETRY_INTERVAL), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_CONSOLE), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_FILE), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_LEVEL_STDERR), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_SUBPROCESS), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_LOG_TIMESTAMP), NULL);
            kvPut(optionReplace, VARSTRDEF(CFGOPT_PROCESS_MAX), NULL);

            kvPut(
                result, VARSTRZ(RESTORE_COMMAND),
                VARSTR(
                    strNewFmt(
                        "%s %s %%f \"%%p\"", strZ(cfgExe()),
                        strZ(strLstJoin(cfgExecParam(cfgCmdArchiveGet, cfgCmdRoleMain, optionReplace, true, true), " ")))));
        }

        // If recovery type is immediate
        if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_IMMEDIATE)
        {
            kvPut(result, VARSTRZ(RECOVERY_TARGET), VARSTRZ(CFGOPTVAL_TYPE_IMMEDIATE_Z));
        }
        // Else recovery type is standby
        else if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_STANDBY)
        {
            // Write standby_mode for PostgreSQL versions that support it
            if (pgVersion < PG_VERSION_RECOVERY_GUC)
                kvPut(result, VARSTR(STANDBY_MODE_STR), VARSTRDEF("on"));
        }
        // Else recovery type is not default so write target options
        else if (cfgOptionStrId(cfgOptType) != CFGOPTVAL_TYPE_DEFAULT)
        {
            // Write the recovery target
            kvPut(
                result, VARSTR(strNewFmt(RECOVERY_TARGET "_%s", strZ(cfgOptionStr(cfgOptType)))),
                VARSTR(cfgOptionStr(cfgOptTarget)));

            // Write recovery_target_inclusive
            if (cfgOptionTest(cfgOptTargetExclusive) && cfgOptionBool(cfgOptTargetExclusive))
                kvPut(result, VARSTRZ(RECOVERY_TARGET_INCLUSIVE), VARSTR(FALSE_STR));
        }

        // Write pause_at_recovery_target/recovery_target_action
        if (cfgOptionTest(cfgOptTargetAction))
        {
            StringId targetAction = cfgOptionStrId(cfgOptTargetAction);

            if (targetAction != CFGOPTVAL_TARGET_ACTION_PAUSE)
            {
                // Write recovery_target on supported PostgreSQL versions
                if (pgVersion >= PG_VERSION_RECOVERY_TARGET_ACTION)
                {
                    kvPut(result, VARSTRZ(RECOVERY_TARGET_ACTION), VARSTR(strIdToStr(targetAction)));
                }
                // Write pause_at_recovery_target on supported PostgreSQL versions
                else if (pgVersion >= PG_VERSION_RECOVERY_TARGET_PAUSE)
                {
                    // Shutdown action is not supported with pause_at_recovery_target setting
                    if (targetAction == CFGOPTVAL_TARGET_ACTION_SHUTDOWN)
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            CFGOPT_TARGET_ACTION "=" CFGOPTVAL_TARGET_ACTION_SHUTDOWN_Z " is only available in PostgreSQL >= %s",
                            strZ(pgVersionToStr(PG_VERSION_RECOVERY_TARGET_ACTION)));
                    }

                    kvPut(result, VARSTRZ(PAUSE_AT_RECOVERY_TARGET), VARSTR(FALSE_STR));
                }
                // Else error on unsupported version
                else
                {
                    THROW_FMT(
                        OptionInvalidError, CFGOPT_TARGET_ACTION " option is only available in PostgreSQL >= %s",
                        strZ(pgVersionToStr(PG_VERSION_RECOVERY_TARGET_PAUSE)));
                }
            }
        }

        // Write recovery_target_timeline
        if (cfgOptionTest(cfgOptTargetTimeline))
            kvPut(result, VARSTRZ(RECOVERY_TARGET_TIMELINE), VARSTR(cfgOptionStr(cfgOptTargetTimeline)));

        // Move to prior context
        kvMove(result, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(KEY_VALUE, result);
}

// Helper to convert recovery options to text format
static String *
restoreRecoveryConf(unsigned int pgVersion, const String *restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        result = strCatFmt(strNew(), "# Recovery settings generated by " PROJECT_NAME " restore on %s\n", strZ(restoreLabel));

        // Output all recovery options
        KeyValue *optionKv = restoreRecoveryOption(pgVersion);
        const VariantList *optionKeyList = kvKeyList(optionKv);

        for (unsigned int optionKeyIdx = 0; optionKeyIdx < varLstSize(optionKeyList); optionKeyIdx++)
        {
            const Variant *optionKey = varLstGet(optionKeyList, optionKeyIdx);
            strCatFmt(result, "%s = '%s'\n", strZ(varStr(optionKey)), strZ(varStr(kvGet(optionKv, optionKey))));
        }

        // Move to prior context
        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(result);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

// Helper to write recovery options into recovery.conf
static void
restoreRecoveryWriteConf(const Manifest *manifest, unsigned int pgVersion, const String *restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    // Only write recovery.conf if recovery type != none
    if (cfgOptionStrId(cfgOptType) != CFGOPTVAL_TYPE_NONE)
    {
        LOG_INFO_FMT("write %s", strZ(storagePathP(storagePg(), PG_FILE_RECOVERYCONF_STR)));

        // Use the data directory to set permissions and ownership for recovery file
        const ManifestPath *dataPath = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);
        mode_t recoveryFileMode = dataPath->mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        // Write recovery.conf
        storagePutP(
            storageNewWriteP(
                storagePgWrite(), PG_FILE_RECOVERYCONF_STR, .noCreatePath = true, .modeFile = recoveryFileMode, .noAtomic = true,
                .noSyncPath = true, .user = dataPath->user, .group = dataPath->group),
            BUFSTR(restoreRecoveryConf(pgVersion, restoreLabel)));
    }

    FUNCTION_LOG_RETURN_VOID();
}

// Helper to write recovery options into postgresql.auto.conf
static void
restoreRecoveryWriteAutoConf(unsigned int pgVersion, const String *restoreLabel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(STRING, restoreLabel);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *content = strNew();

        // Load postgresql.auto.conf so we can preserve the existing contents
        Buffer *autoConf = storageGetP(storageNewReadP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR, .ignoreMissing = true));

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
            RegExp *recoveryExp =
                regExpNew(
                    STRDEF(
                        "^[\t ]*(" RECOVERY_TARGET "|" RECOVERY_TARGET_ACTION "|" RECOVERY_TARGET_INCLUSIVE "|"
                            RECOVERY_TARGET_LSN "|" RECOVERY_TARGET_NAME "|" RECOVERY_TARGET_TIME "|" RECOVERY_TARGET_TIMELINE "|"
                            RECOVERY_TARGET_XID ")[\t ]*="));

            // Check each line for recovery settings
            const StringList *contentList = strLstNewSplit(strNewBuf(autoConf), LF_STR);

            for (unsigned int contentIdx = 0; contentIdx < strLstSize(contentList); contentIdx++)
            {
                if (contentIdx != 0)
                    strCat(content, LF_STR);

                const String *line = strLstGet(contentList, contentIdx);

                if (regExpMatch(recoveryExp, line))
                    strCatFmt(content, "# Removed by " PROJECT_NAME " restore on %s # ", strZ(restoreLabel));

                strCat(content, line);
            }

            // If settings will be appended then format the file so a blank line will be between old and new settings
            if (cfgOptionStrId(cfgOptType) != CFGOPTVAL_TYPE_NONE)
            {
                strTrim(content);
                strCatZ(content, "\n\n");
            }
        }

        // If recovery was requested then write the recovery options
        if (cfgOptionStrId(cfgOptType) != CFGOPTVAL_TYPE_NONE)
        {
            // If the user specified standby_mode as a recovery option then error.  It's tempting to just set type=standby in this
            // case but since config parsing has already happened the target options could be in an invalid state.
            if (cfgOptionTest(cfgOptRecoveryOption))
            {
                const KeyValue *recoveryOption = cfgOptionKv(cfgOptRecoveryOption);
                StringList *recoveryOptionKey = strLstNewVarLst(kvKeyList(recoveryOption));

                for (unsigned int keyIdx = 0; keyIdx < strLstSize(recoveryOptionKey); keyIdx++)
                {
                    // Get the key and value
                    String *key = strLstGet(recoveryOptionKey, keyIdx);

                    // Replace - in key with _.  Since we use - users naturally will as well.
                    strReplaceChr(key, '-', '_');

                    if (strEq(key, STANDBY_MODE_STR))
                    {
                        THROW_FMT(
                            OptionInvalidError,
                            "'" STANDBY_MODE "' setting is not valid for " PG_NAME " >= %s\n"
                            "HINT: use --" CFGOPT_TYPE "=" CFGOPTVAL_TYPE_STANDBY_Z " instead of --" CFGOPT_RECOVERY_OPTION "="
                                STANDBY_MODE "=on.",
                            strZ(pgVersionToStr(PG_VERSION_RECOVERY_GUC)));
                    }
                }
            }

            strCatFmt(content, "%s", strZ(restoreRecoveryConf(pgVersion, restoreLabel)));
        }

        LOG_INFO_FMT(
            "write %s%s", autoConf == NULL ? "" : "updated ", strZ(storagePathP(storagePg(), PG_FILE_POSTGRESQLAUTOCONF_STR)));

        // Use the data directory to set permissions and ownership for recovery file
        const StorageInfo dataPath = storageInfoP(storagePg(), NULL);
        mode_t recoveryFileMode = dataPath.mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

        // Write postgresql.auto.conf
        storagePutP(
            storageNewWriteP(
                storagePgWrite(), PG_FILE_POSTGRESQLAUTOCONF_STR, .noCreatePath = true, .modeFile = recoveryFileMode,
                .noAtomic = true, .noSyncPath = true, .user = dataPath.user, .group = dataPath.group),
            BUFSTR(content));

        // The standby.signal file is required for standby mode
        if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_STANDBY)
        {
            storagePutP(
                storageNewWriteP(
                    storagePgWrite(), PG_FILE_STANDBYSIGNAL_STR, .noCreatePath = true, .modeFile = recoveryFileMode,
                    .noAtomic = true, .noSyncPath = true, .user = dataPath.user, .group = dataPath.group),
                NULL);
        }
        // Else the recovery.signal file is required for targeted recovery
        else
        {
            storagePutP(
                storageNewWriteP(
                    storagePgWrite(), PG_FILE_RECOVERYSIGNAL_STR, .noCreatePath = true, .modeFile = recoveryFileMode,
                    .noAtomic = true, .noSyncPath = true, .user = dataPath.user, .group = dataPath.group),
                NULL);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

static void
restoreRecoveryWrite(const Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    // Get PostgreSQL version to write recovery for
    unsigned int pgVersion = manifestData(manifest)->pgVersion;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // If recovery type is preserve then leave recovery file as it is
        if (cfgOptionStrId(cfgOptType) == CFGOPTVAL_TYPE_PRESERVE)
        {
            // Determine which file recovery setttings will be written to
            const String *recoveryFile = pgVersion >= PG_VERSION_RECOVERY_GUC ?
                PG_FILE_POSTGRESQLAUTOCONF_STR : PG_FILE_RECOVERYCONF_STR;

            if (!storageExistsP(storagePg(), recoveryFile))
            {
                LOG_WARN_FMT(
                    "recovery type is " CFGOPTVAL_TYPE_PRESERVE_Z " but recovery file does not exist at '%s'",
                    strZ(storagePathP(storagePg(), recoveryFile)));
            }
        }
        // Else write recovery file
        else
        {
            // Generate a label used to identify this restore in the recovery file
            struct tm timePart;
            char restoreTimestamp[20];
            time_t timestamp = time(NULL);

            strftime(restoreTimestamp, sizeof(restoreTimestamp), "%Y-%m-%d %H:%M:%S", localtime_r(&timestamp, &timePart));
            const String *restoreLabel = STR(restoreTimestamp);

            // Write recovery file based on PostgreSQL version
            if (pgVersion >= PG_VERSION_RECOVERY_GUC)
                restoreRecoveryWriteAutoConf(pgVersion, restoreLabel);
            else
                restoreRecoveryWriteConf(manifest, pgVersion, restoreLabel);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Generate a list of queues that determine the order of file processing
***********************************************************************************************************************************/
// Comparator to order ManifestFile objects by size then name
static int
restoreProcessQueueComparator(const void *item1, const void *item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    // If the size differs then that's enough to determine order
    if ((*(ManifestFile **)item1)->size < (*(ManifestFile **)item2)->size)
        FUNCTION_TEST_RETURN(-1);
    else if ((*(ManifestFile **)item1)->size > (*(ManifestFile **)item2)->size)
        FUNCTION_TEST_RETURN(1);

    // If size is the same then use name to generate a deterministic ordering (names must be unique)
    FUNCTION_TEST_RETURN(strCmp((*(ManifestFile **)item1)->name, (*(ManifestFile **)item2)->name));
}

static uint64_t
restoreProcessQueue(Manifest *manifest, List **queueList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(LIST, queueList);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    uint64_t result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create list of process queue
        *queueList = lstNewP(sizeof(List *));

        // Generate the list of processing queues (there is always at least one)
        StringList *targetList = strLstNew();
        strLstAdd(targetList, STRDEF(MANIFEST_TARGET_PGDATA "/"));

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);

            if (target->tablespaceId != 0)
                strLstAdd(targetList, strNewFmt("%s/", strZ(target->name)));
        }

        // Generate the processing queues
        MEM_CONTEXT_BEGIN(lstMemContext(*queueList))
        {
            for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            {
                List *queue = lstNewP(sizeof(ManifestFile *), .comparator = restoreProcessQueueComparator);
                lstAdd(*queueList, &queue);
            }
        }
        MEM_CONTEXT_END();

        // Now put all files into the processing queues
        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile *file = manifestFile(manifest, fileIdx);

            // Find the target that contains this file
            unsigned int targetIdx = 0;

            do
            {
                // A target should always be found
                CHECK(targetIdx < strLstSize(targetList));

                if (strBeginsWith(file->name, strLstGet(targetList, targetIdx)))
                    break;

                targetIdx++;
            }
            while (1);

            // Add file to queue
            lstAdd(*(List **)lstGet(*queueList, targetIdx), &file);

            // Add size to total
            result += file->size;
        }

        // Sort the queues
        for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            lstSort(*(List **)lstGet(*queueList, targetIdx), sortOrderDesc);

        // Move process queues to prior context
        lstMove(*queueList, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT64, result);
}

/***********************************************************************************************************************************
Log the results of a job and throw errors
***********************************************************************************************************************************/
// Helper function to determine if a file should be zeroed
static bool
restoreFileZeroed(const String *manifestName, RegExp *zeroExp)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, manifestName);
        FUNCTION_TEST_PARAM(REGEXP, zeroExp);
    FUNCTION_TEST_END();

    ASSERT(manifestName != NULL);

    FUNCTION_TEST_RETURN(
        zeroExp == NULL ? false : regExpMatch(zeroExp, manifestName) && !strEndsWith(manifestName, STRDEF("/" PG_FILE_PGVERSION)));
}

// Helper function to construct the absolute pg path for any file
static String *
restoreFilePgPath(const Manifest *manifest, const String *manifestName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(MANIFEST, manifest);
        FUNCTION_TEST_PARAM(STRING, manifestName);
    FUNCTION_TEST_END();

    ASSERT(manifest != NULL);
    ASSERT(manifestName != NULL);

    String *result = strNewFmt("%s/%s", strZ(manifestTargetBase(manifest)->path), strZ(manifestPathPg(manifestName)));

    if (strEq(manifestName, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL)))
        result = strNewFmt("%s." STORAGE_FILE_TEMP_EXT, strZ(result));

    FUNCTION_TEST_RETURN(result);
}

static uint64_t
restoreJobResult(const Manifest *manifest, ProtocolParallelJob *job, RegExp *zeroExp, uint64_t sizeTotal, uint64_t sizeRestored)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, job);
        FUNCTION_LOG_PARAM(REGEXP, zeroExp);
        FUNCTION_LOG_PARAM(UINT64, sizeTotal);
        FUNCTION_LOG_PARAM(UINT64, sizeRestored);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    // The job was successful
    if (protocolParallelJobErrorCode(job) == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const ManifestFile *file = manifestFileFind(manifest, varStr(protocolParallelJobKey(job)));
            bool zeroed = restoreFileZeroed(file->name, zeroExp);
            bool copy = pckReadBoolP(protocolParallelJobResult(job));

            String *log = strCatZ(strNew(), "restore");

            // Note if file was zeroed (i.e. selective restore)
            if (zeroed)
                strCatZ(log, " zeroed");

            // Add filename
            strCatFmt(log, " file %s", strZ(restoreFilePgPath(manifest, file->name)));

            // If not copied and not zeroed add details to explain why it was not copied
            if (!copy && !zeroed)
            {
                strCatZ(log, " - ");

                // On force we match on size and modification time
                if (cfgOptionBool(cfgOptForce))
                {
                    strCatFmt(
                        log, "exists and matches size %" PRIu64 " and modification time %" PRIu64, file->size,
                        (uint64_t)file->timestamp);
                }
                // Else a checksum delta or file is zero-length
                else
                {
                    strCatZ(log, "exists and ");

                    // No need to copy zero-length files
                    if (file->size == 0)
                    {
                        strCatZ(log, "is zero size");
                    }
                    // The file matched the manifest checksum so did not need to be copied
                    else
                        strCatZ(log, "matches backup");
                }
            }

            // Add size and percent complete
            sizeRestored += file->size;
            strCatFmt(log, " (%s, %" PRIu64 "%%)", strZ(strSizeFormat(file->size)), sizeRestored * 100 / sizeTotal);

            // If not zero-length add the checksum
            if (file->size != 0 && !zeroed)
                strCatFmt(log, " checksum %s", file->checksumSha1);

            LOG_DETAIL_PID(protocolParallelJobProcessId(job), strZ(log));
        }
        MEM_CONTEXT_TEMP_END();

        // Free the job
        protocolParallelJobFree(job);
    }
    // Else the job errored
    else
        THROW_CODE(protocolParallelJobErrorCode(job), strZ(protocolParallelJobErrorMessage(job)));

    FUNCTION_LOG_RETURN(UINT64, sizeRestored);
}

/***********************************************************************************************************************************
Return new restore jobs as requested
***********************************************************************************************************************************/
typedef struct RestoreJobData
{
    unsigned int repoIdx;                                           // Internal repo idx
    Manifest *manifest;                                             // Backup manifest
    List *queueList;                                                // List of processing queues
    RegExp *zeroExp;                                                // Identify files that should be sparse zeroed
    const String *cipherSubPass;                                    // Passphrase used to decrypt files in the backup
} RestoreJobData;

// Helper to calculate the next queue to scan based on the client index
static int
restoreJobQueueNext(unsigned int clientIdx, int queueIdx, unsigned int queueTotal)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT, clientIdx);
        FUNCTION_TEST_PARAM(INT, queueIdx);
        FUNCTION_TEST_PARAM(UINT, queueTotal);
    FUNCTION_TEST_END();

    // Move (forward or back) to the next queue
    queueIdx += clientIdx % 2 ? -1 : 1;

    // Deal with wrapping on either end
    if (queueIdx < 0)
        FUNCTION_TEST_RETURN((int)queueTotal - 1);
    else if (queueIdx == (int)queueTotal)
        FUNCTION_TEST_RETURN(0);

    FUNCTION_TEST_RETURN(queueIdx);
}

// Callback to fetch restore jobs for the parallel executor
static ProtocolParallelJob *restoreJobCallback(void *data, unsigned int clientIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(UINT, clientIdx);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);

    ProtocolParallelJob *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get a new job if there are any left
        RestoreJobData *jobData = data;

        // Determine where to begin scanning the queue (we'll stop when we get back here)
        int queueIdx = (int)(clientIdx % lstSize(jobData->queueList));
        int queueEnd = queueIdx;

        do
        {
            List *queue = *(List **)lstGet(jobData->queueList, (unsigned int)queueIdx);

            if (!lstEmpty(queue))
            {
                const ManifestFile *file = *(ManifestFile **)lstGet(queue, 0);

                // Create restore job
                ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_RESTORE_FILE);
                PackWrite *const param = protocolCommandParam(command);

                pckWriteStrP(param, file->name);
                pckWriteU32P(param, jobData->repoIdx);
                pckWriteStrP(param, file->reference != NULL ? file->reference : manifestData(jobData->manifest)->backupLabel);
                pckWriteU32P(param, manifestData(jobData->manifest)->backupOptionCompressType);
                pckWriteStrP(param, restoreFilePgPath(jobData->manifest, file->name));
                pckWriteStrP(param, STR(file->checksumSha1));
                pckWriteBoolP(param, restoreFileZeroed(file->name, jobData->zeroExp));
                pckWriteU64P(param, file->size);
                pckWriteTimeP(param, file->timestamp);
                pckWriteModeP(param, file->mode);
                pckWriteStrP(param, file->user);
                pckWriteStrP(param, file->group);
                pckWriteTimeP(param, manifestData(jobData->manifest)->backupTimestampCopyStart);
                pckWriteBoolP(param, cfgOptionBool(cfgOptDelta));
                pckWriteBoolP(param, cfgOptionBool(cfgOptDelta) && cfgOptionBool(cfgOptForce));
                pckWriteStrP(param, jobData->cipherSubPass);

                // Remove job from the queue
                lstRemoveIdx(queue, 0);

                // Assign job to result
                MEM_CONTEXT_PRIOR_BEGIN()
                {
                    result = protocolParallelJobNew(VARSTR(file->name), command);
                }
                MEM_CONTEXT_PRIOR_END();

                // Break out of the loop early since we found a job
                break;
            }

            queueIdx = restoreJobQueueNext(clientIdx, queueIdx, lstSize(jobData->queueList));
        }
        while (queueIdx != queueEnd);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

/**********************************************************************************************************************************/
void
cmdRestore(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get information for the current user
        userInit();

        // PostgreSQL must be local
        pgIsLocalVerify();

        // Validate restore path
        restorePathValidate();

        // Remove stanza archive spool path so existing files do not interfere with the new cluster. For instance, old archive-push
        // acknowledgements could cause a new cluster to skip archiving. This should not happen if a new timeline is selected but
        // better to be safe. Missing stanza spool paths are ignored.
        storagePathRemoveP(storageSpoolWrite(), STORAGE_SPOOL_ARCHIVE_STR, .recurse = true);

        // Get the backup set
        RestoreBackupData backupData = restoreBackupSet();

        // Load manifest
        RestoreJobData jobData = {.repoIdx = backupData.repoIdx};

        jobData.manifest = manifestLoadFile(
            storageRepoIdx(backupData.repoIdx),
            strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupData.backupSet)), backupData.repoCipherType,
            backupData.backupCipherPass);

        // Remotes (if any) are no longer needed since the rest of the repository reads will be done by the local processes
        protocolFree();

        // Validate manifest.  Don't use strict mode because we'd rather ignore problems that won't affect a restore.
        manifestValidate(jobData.manifest, false);

        // Get the cipher subpass used to decrypt files in the backup
        jobData.cipherSubPass = manifestCipherSubPass(jobData.manifest);

        // Validate the manifest
        restoreManifestValidate(jobData.manifest, backupData.backupSet);

        // Log the backup set to restore. If the backup was online then append the time recovery will start from.
        String *const message = strCatFmt(
            strNew(), "repo%u: restore backup set %s", cfgOptionGroupIdxToKey(cfgOptGrpRepo, backupData.repoIdx),
            strZ(backupData.backupSet));

        if (manifestData(jobData.manifest)->backupOptionOnline)
        {
            struct tm timePart;
            char timeBuffer[20];
            time_t backupTimestampStart = manifestData(jobData.manifest)->backupTimestampStart;

            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime_r(&backupTimestampStart, &timePart));
            strCatFmt(message, ", recovery will start at %s", timeBuffer);
        }

        LOG_INFO(strZ(message));

        // Map manifest
        restoreManifestMap(jobData.manifest);

        // Check that links are sane
        manifestLinkCheck(jobData.manifest);

        // Update ownership
        restoreManifestOwner(jobData.manifest);

        // Generate the selective restore expression
        String *expression = restoreSelectiveExpression(jobData.manifest);
        jobData.zeroExp = expression == NULL ? NULL : regExpNew(expression);

        // Clean the data directory and build path/link structure
        restoreCleanBuild(jobData.manifest);

        // Generate processing queues
        uint64_t sizeTotal = restoreProcessQueue(jobData.manifest, &jobData.queueList);

        // Save manifest to the data directory so we can restart a delta restore even if the PG_VERSION file is missing
        manifestSave(jobData.manifest, storageWriteIo(storageNewWriteP(storagePgWrite(), BACKUP_MANIFEST_FILE_STR)));

        // Create the parallel executor
        ProtocolParallel *parallelExec = protocolParallelNew(
            cfgOptionUInt64(cfgOptProtocolTimeout) / 2, restoreJobCallback, &jobData);

        for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
            protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypeRepo, 0, processIdx));

        // Process jobs
        uint64_t sizeRestored = 0;

        MEM_CONTEXT_TEMP_RESET_BEGIN()
        {
            do
            {
                unsigned int completed = protocolParallelProcess(parallelExec);

                for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
                {
                    sizeRestored = restoreJobResult(
                        jobData.manifest, protocolParallelResult(parallelExec), jobData.zeroExp, sizeTotal, sizeRestored);
                }

                // Reset the memory context occasionally so we don't use too much memory or slow down processing
                MEM_CONTEXT_TEMP_RESET(1000);
            }
            while (!protocolParallelDone(parallelExec));
        }
        MEM_CONTEXT_TEMP_END();

        // Write recovery settings
        restoreRecoveryWrite(jobData.manifest);

        // Remove backup.manifest
        storageRemoveP(storagePgWrite(), BACKUP_MANIFEST_FILE_STR);

        // Sync file link paths. These need to be synced separately because they are not linked from the data directory.
        StringList *pathSynced = strLstNew();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(jobData.manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(jobData.manifest, targetIdx);

            if (target->type == manifestTargetTypeLink && target->file != NULL)
            {
                const String *pgPath = manifestTargetPath(jobData.manifest, target);

                // Don't sync the same path twice.  There can be multiple links to files in the same path, but syncing it more than
                // once makes the logs noisy and looks like a bug even though it doesn't hurt anything or realistically affect
                // performance.
                if (strLstExists(pathSynced, pgPath))
                    continue;
                else
                    strLstAdd(pathSynced, pgPath);

                // Sync the path
                LOG_DETAIL_FMT("sync path '%s'", strZ(pgPath));
                storagePathSyncP(storageLocalWrite(), pgPath);
            }
        }

        // Sync paths in the data directory
        for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(jobData.manifest); pathIdx++)
        {
            const String *manifestName = manifestPath(jobData.manifest, pathIdx)->name;

            // Skip the pg_tblspc path because it only maps to the manifest.  We should remove this in a future release but not much
            // can be done about it for now.
            if (strEqZ(manifestName, MANIFEST_TARGET_PGTBLSPC))
                continue;

            // We'll sync global after pg_control is written
            if (strEq(manifestName, STRDEF(MANIFEST_TARGET_PGDATA "/" PG_PATH_GLOBAL)))
                continue;

            const String *pgPath = storagePathP(storagePg(), manifestPathPg(manifestName));

            LOG_DETAIL_FMT("sync path '%s'", strZ(pgPath));
            storagePathSyncP(storagePgWrite(), pgPath);
        }

        // Rename pg_control
        if (storageExistsP(storagePg(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "." STORAGE_FILE_TEMP_EXT)))
        {
            LOG_INFO(
                "restore " PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL " (performed last to ensure aborted restores cannot be started)");

            storageMoveP(
                storagePgWrite(),
                storageNewReadP(storagePg(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "." STORAGE_FILE_TEMP_EXT)),
                storageNewWriteP(storagePgWrite(), STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL), .noSyncPath = true));
        }
        else
            LOG_WARN("backup does not contain '" PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL "' -- cluster will not start");

        // Sync global path
        LOG_DETAIL_FMT("sync path '%s'", strZ(storagePathP(storagePg(), PG_PATH_GLOBAL_STR)));
        storagePathSyncP(storagePgWrite(), PG_PATH_GLOBAL_STR);

        // Restore info
        LOG_INFO_FMT(
            "restore size = %s, file total = %u", strZ(strSizeFormat(sizeRestored)), manifestFileTotal(jobData.manifest));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
