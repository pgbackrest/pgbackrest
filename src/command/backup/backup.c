/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
// #include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/control/common.h"
#include "command/backup/backup.h"
#include "command/backup/common.h"
#include "command/backup/file.h"
#include "command/backup/protocol.h"
#include "command/stanza/common.h"
#include "common/crypto/cipherBlock.h"
#include "common/compress/gzip/common.h"
#include "common/debug.h"
#include "common/log.h"
// #include "common/regExp.h"
// #include "common/user.h"
#include "common/type/convert.h"
#include "common/type/json.h" // !!! TRY TO REMOVE
#include "config/config.h"
// #include "config/exec.h"
#include "db/helper.h"
#include "info/infoBackup.h"
#include "info/manifest.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "protocol/helper.h"
#include "protocol/parallel.h"
#include "storage/helper.h"
// #include "storage/write.intern.h"
#include "version.h"

/***********************************************************************************************************************************
Backup path constants
***********************************************************************************************************************************/
#define BACKUP_PATH_HISTORY                                         "backup.history"

/**********************************************************************************************************************************
Generate a unique backup label that does not contain a timestamp from a previous backup
***********************************************************************************************************************************/
// Helper to format the backup label
static String *
backupLabelFormat(BackupType type, const String *backupLabelLast, time_t timestamp)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(STRING, backupLabelLast);
        FUNCTION_LOG_PARAM(TIME, timestamp);
    FUNCTION_LOG_END();

    ASSERT((type == backupTypeFull && backupLabelLast == NULL) || (type != backupTypeFull && backupLabelLast != NULL));
    ASSERT(timestamp > 0);

    // Format the timestamp
    char buffer[16];
    THROW_ON_SYS_ERROR(
        strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", localtime(&timestamp)) == 0, AssertError, "unable to format time");

    // If full label
    String *result = NULL;

    if (type == backupTypeFull)
    {
        result = strNewFmt("%sF", buffer);
    }
    // Else diff or incr label
    else
    {
        // Get the full backup portion of the last backup label
        result = strSubN(backupLabelLast, 0, 16);

        // Append the diff/incr timestamp
        strCatFmt(result, "_%s%s", buffer, type == backupTypeDiff ? "D" : "I");
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

static String *
backupLabel(BackupType type, const String *backupLabelLast, time_t timestamp)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(ENUM, type);
        FUNCTION_LOG_PARAM(STRING, backupLabelLast);
        FUNCTION_LOG_PARAM(TIME, timestamp);
    FUNCTION_LOG_END();

    ASSERT((type == backupTypeFull && backupLabelLast == NULL) || (type != backupTypeFull && backupLabelLast != NULL));
    ASSERT(timestamp > 0);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        while (true)
        {
            // Get the current year to use for searching the history.  Put this in the loop since we could theoretically roll over to
            // a new year while searching for a valid label.
            char year[5];
            THROW_ON_SYS_ERROR(
                strftime(year, sizeof(year), "%Y", localtime(&timestamp)) == 0, AssertError, "unable to format year");

            // Create regular expression for search.  We can't just search on the label because we want to be sure that no other
            // backup uses the same timestamp, no matter what type it is.
            String *timestampStr = strSubN(backupLabelFormat(backupTypeFull, NULL, timestamp), 0, 15);
            String *timestampExp = strNewFmt("(^%sF$)|(_%s(D|I)$)", strPtr(timestampStr), strPtr(timestampStr));

            // Check for the timestamp in the backup path
            if (strLstSize(storageListP(storageRepo(), STORAGE_REPO_BACKUP_STR, .expression = timestampExp)) == 0)
            {
                // Now check in the backup.history
                String *historyPath = strNewFmt(STORAGE_REPO_BACKUP "/" BACKUP_PATH_HISTORY "/%s", year);
                String *historyExp = strNewFmt(
                    "(^%sF\\.manifest\\." GZIP_EXT "$)|(_%s(D|I)\\.manifest\\." GZIP_EXT "$)", strPtr(timestampStr),
                    strPtr(timestampStr));

                // If the timestamp also does not appear in the history then it is safe to use
                if (strLstSize(storageListP(storageRepo(), historyPath, .expression = historyExp)) == 0)
                    break;
            }

            // !!! NOT SURE ABOUT THIS LOGIC -- WHAT IF THIS IS NOT THE LAST BACKUP?
            timestamp++;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, backupLabelFormat(type, backupLabelLast, timestamp));
}

/***********************************************************************************************************************************
Get the postgres database and storage objects
***********************************************************************************************************************************/
#define FUNCTION_LOG_BACKUP_PG_TYPE                                                                                                \
    BackupPg
#define FUNCTION_LOG_BACKUP_PG_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(&value, "BackupPg", buffer, bufferSize)

typedef struct BackupPg
{
    const Storage *storagePrimary;
    const String *const primaryHost;

    const Db *dbStandby;
    const String *const standbyHost;

    unsigned int pageSize;                                          // PostgreSQL page size
} BackupPg;

static BackupPg
backupPgGet(const InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // Get control information from the primary and validate it against backup info
    // !!! PRETTY BADLY FAKED FOR NOW, SHOULD BE pgGet() KINDA THING
    const Storage *storagePrimary = storagePgId(1);
    const String *const primaryHost = cfgOptionStr(cfgOptPgHost);
    const String *const standbyHost = primaryHost;

    InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
    PgControl pgControl = pgControlFromFile(storagePrimary);

    // Initialize result
    BackupPg result =
    {
        .storagePrimary = storagePrimary,

        .primaryHost = primaryHost,
        .standbyHost = standbyHost,

        .pageSize = pgControl.pageSize,
    };

    if (pgControl.version != infoPg.version || pgControl.systemId != infoPg.systemId)
    {
        THROW_FMT(
            BackupMismatchError,
            PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
            strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(infoPg.version)),
            infoPg.systemId);
    }

    // Backup from standby can only be used on PostgreSQL >= 9.1
    if (cfgOption(cfgOptOnline) && cfgOption(cfgOptBackupStandby) && infoPg.version < PG_VERSION_BACKUP_STANDBY)
    {
        THROW_FMT(
            ConfigError, "option '" CFGOPT_BACKUP_STANDBY "' not valid for " PG_NAME " < %s",
            strPtr(pgVersionToStr(PG_VERSION_BACKUP_STANDBY)));
    }

    // If backup from standby option is set but a standby was not configured in the config file or on the command line, then turn
    // off backup-standby and warn that backups will be performed from the primary.
    if (result.dbStandby == NULL && cfgOptionBool(cfgOptBackupStandby))
    {
        cfgOptionSet(cfgOptBackupStandby, cfgSourceParam, BOOL_FALSE_VAR);
        LOG_WARN(
            "option " CFGOPT_BACKUP_STANDBY " is enabled but standby is not properly configured - backups will be performed from"
            " the primary");
    }

    FUNCTION_LOG_RETURN(BACKUP_PG, result);
}

/***********************************************************************************************************************************
Create an incremental backup if type is not full and a prior backup exists
***********************************************************************************************************************************/
// Helper to find a compatible prior backup
static Manifest *
backupBuildIncrPrior(const InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    Manifest *result = NULL;

    // No incremental if backup type is full
    if (backupType(cfgOptionStr(cfgOptType)) != backupTypeFull)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            BackupType type = backupType(cfgOptionStr(cfgOptType));
            InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
            const String *backupLabelPrior = NULL;

            if (type != backupTypeFull)
            {
                unsigned int backupTotal = infoBackupDataTotal(infoBackup);

                for (unsigned int backupIdx = backupTotal - 1; backupIdx < backupTotal; backupIdx--)
                {
                     InfoBackupData backupPrior = infoBackupData(infoBackup, backupIdx);

                     // The prior backup for a diff must be full
                     if (type == backupTypeDiff && backupType(backupPrior.backupType) != backupTypeFull)
                        continue;

                    // The backups must come from the same cluster
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
                        storageRepo(), strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabelPrior)),
                        cipherType(cfgOptionStr(cfgOptRepoCipherType)), infoPgCipherPass(infoBackupPg(infoBackup)));
                    const ManifestData *manifestPriorData = manifestData(result);

                    // Warn if compress option changed
                    if (cfgOptionBool(cfgOptCompress) != manifestPriorData->backupOptionCompress)
                    {
                        LOG_WARN_FMT(
                            "%s backup cannot alter compress option to '%s', reset to value in %s",
                            strPtr(cfgOptionStr(cfgOptType)), cvtBoolToConstZ(cfgOptionBool(cfgOptCompress)),
                            strPtr(backupLabelPrior));
                        cfgOptionSet(cfgOptCompress, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionCompress));
                    }

                    // Warn if hardlink option changed
                    if (cfgOptionBool(cfgOptRepoHardlink) != manifestPriorData->backupOptionHardLink)
                    {
                        LOG_WARN_FMT(
                            "%s backup cannot alter hardlink option to '%s', reset to value in %s",
                            strPtr(cfgOptionStr(cfgOptType)), cvtBoolToConstZ(cfgOptionBool(cfgOptRepoHardlink)),
                            strPtr(backupLabelPrior));
                        cfgOptionSet(cfgOptRepoHardlink, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionHardLink));
                    }

                    // If not defined this backup was done in a version prior to page checksums being introduced.  Just set
                    // checksum-page to false and move on without a warning.  Page checksums will start on the next full backup.
                    if (manifestData(result)->backupOptionChecksumPage == NULL)
                    {
                        cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
                    }
                    // Don't allow the checksum-page option to change in a diff or incr backup.  This could be confusing as only
                    // certain files would be checksummed and the list could be incomplete during reporting.
                    else
                    {
                        bool checksumPagePrior = varBool(manifestData(result)->backupOptionChecksumPage);

                        // Warn if an incompatible setting was explicitly requested
                        // ??? After the migration this condition can be:
                        // ???    !cfgOptionTest(cfgOptChecksumPage) || checksumPagePrior != cfgOptionBool(cfgOptChecksumPage)
                        // ??? Since we don't need to log if the user did not express an explicit preference
                        if (!cfgOptionTest(cfgOptChecksumPage) || checksumPagePrior != cfgOptionBool(cfgOptChecksumPage))
                        {
                            // ??? This can be removed after the migration since the warning will not be logged
                            if (!cfgOptionTest(cfgOptChecksumPage))
                                cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);

                            LOG_WARN_FMT(
                                "%s backup cannot alter '" CFGOPT_CHECKSUM_PAGE "' option to '%s', reset to '%s' from %s",
                                strPtr(cfgOptionStr(cfgOptType)), cvtBoolToConstZ(cfgOptionBool(cfgOptChecksumPage)),
                                cvtBoolToConstZ(checksumPagePrior), strPtr(manifestData(result)->backupLabel));
                        }

                        cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, VARBOOL(checksumPagePrior));
                    }

                    manifestMove(result, MEM_CONTEXT_OLD());
                }
                else
                {
                    LOG_WARN_FMT("no prior backup exists, %s backup has been changed to full", strPtr(cfgOptionStr(cfgOptType)));
                    cfgOptionSet(cfgOptType, cfgSourceParam, VARSTR(backupTypeStr(type)));
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupBuildIncr(const InfoBackup *infoBackup, Manifest *manifest, Manifest *manifestPrior)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(MANIFEST, manifestPrior);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    bool result = false;

    // No incremental if no prior manifest
    if (manifestPrior != NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            LOG_INFO_FMT(
                "last backup label = %s, version = %s", strPtr(manifestData(manifestPrior)->backupLabel),
                strPtr(manifestData(manifestPrior)->backrestVersion));

            // Move the manifest to this context so it will be freed when we are done
            manifestMove(manifestPrior, MEM_CONTEXT_TEMP());

            // Build incremental manifest
            manifestBuildIncr(manifest, manifestPrior, backupType(cfgOptionStr(cfgOptType)), NULL/* !!! ARCHIVE_START */);

            // Set the cipher subpass from prior manifest since we want a single subpass for the entire backup set
            manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestPrior));

            // Incremental was built
            result = true;
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Check for a backup that can be resumed and merge into the manifest if found
***********************************************************************************************************************************/
typedef struct BackupResumeData
{
    Manifest *manifest;                                             // New manifest
    const Manifest *manifestResume;                                 // Resumed manifest
    const bool compressed;                                          // Is the backup compressed?
    const bool delta;                                               // Is this a delta backup?
    const String *backupPath;                                       // Path to the current level of the backup being cleaned
    const String *manifestParentName;                               // Parent manifest name used to construct manifest name
} BackupResumeData;

// Callback to clean invalid paths/files/links out of the repo
void backupResumeCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, *storageInfo);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    BackupResumeData *resumeData = data;

    // Skip all . paths because they have already been handled on the previous level of recursion
    if (strEq(info->name, DOT_STR))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Skip backup.manifest.copy -- it will never be in the manifest
    if (resumeData->manifestParentName == NULL && strEqZ(info->name, BACKUP_MANIFEST_FILE INFO_COPY_EXT))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Build the name used to lookup files in the manifest
    const String *manifestName = resumeData->manifestParentName != NULL ?
        strNewFmt("%s/%s", strPtr(resumeData->manifestParentName), strPtr(info->name)) : info->name;

    // Build the backup path used to remove files/links/paths that are invalid
    const String *backupPath = strNewFmt("%s/%s", strPtr(resumeData->backupPath), strPtr(info->name));

    // Process file types
    switch (info->type)
    {
        // Check paths
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypePath:
        {
            // If the path was not found remove it
            if (manifestPathFindDefault(resumeData->manifest, manifestName, NULL) == NULL)
            {
                LOG_DETAIL_FMT("remove path '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
                storagePathRemoveP(storageRepoWrite(), backupPath, .recurse = true);
            }
            // Else recurse into the path
            {
                BackupResumeData resumeDataSub = *resumeData;
                resumeDataSub.manifestParentName = manifestName;
                resumeDataSub.backupPath = backupPath;

                storageInfoListP(
                    storageRepo(), resumeDataSub.backupPath, backupResumeCallback, &resumeDataSub, .sortOrder = sortOrderAsc);
            }

            break;
        }

        // Check files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeFile:
        {
            // If the backup is compressed then strip off the extension before doing the lookup
            if (resumeData->compressed)
                manifestName = strSubN(manifestName, 0, strSize(manifestName) - sizeof(GZIP_EXT));

            // Find the file in both manifests
            const ManifestFile *file = manifestFileFindDefault(resumeData->manifest, manifestName, NULL);
            const ManifestFile *fileResume = manifestFileFindDefault(resumeData->manifestResume, manifestName, NULL);

            // To be preserved the file must:
            // *) exist in the both manifests
            // *) not be a reference to a previous backup in either manifest
            // *) have the same size
            // *) have the same timestamp if not a delta backup
            if (file != NULL && file->reference == NULL &&
                fileResume != NULL && fileResume->reference == NULL && fileResume->checksumSha1[0] != 0 &&
                file->size == fileResume->size && (resumeData->delta || file->timestamp == fileResume->timestamp) &&
                file->size != 0 /* ??? don't zero size files because Perl wouldn't -- this can be removed after the migration*/)
            {
                manifestFileUpdate(
                    resumeData->manifest, manifestName, file->size, fileResume->sizeRepo, fileResume->checksumSha1, NULL,
                    fileResume->checksumPage, fileResume->checksumPageError, fileResume->checksumPageErrorList);
            }
            // Else remove the file
            else
            {
                LOG_DETAIL_FMT("remove file '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
                storageRemoveP(storageRepoWrite(), backupPath);
            }

            break;
        }

        // Remove links.  We could check that the link has not changed and preserve it but it doesn't seem worth the extra testing.
        // The link will be recreated during the backup if needed.
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeLink:
        {
            storageRemoveP(storageRepoWrite(), backupPath);
            break;
        }

        // Remove special files
        // -------------------------------------------------------------------------------------------------------------------------
        case storageTypeSpecial:
        {
            LOG_WARN_FMT("remove special file '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
            storageRemoveP(storageRepoWrite(), backupPath);
            break;
        }
    }

    FUNCTION_TEST_RETURN_VOID();
}

// Helper to find a resumable backup
static const Manifest *
backupResumeFind(const InfoBackup *infoBackup, const Manifest *manifest, String **backupLabelResume)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM_P(STRING, backupLabelResume);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    Manifest *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Only the last backup can be resumed
        const StringList *backupList = strLstSort(
            storageListP(
                storageRepo(), STRDEF(STORAGE_REPO_BACKUP),
                .expression = backupRegExpP(.full = true, .differential = true, .incremental = true)),
            sortOrderDesc);

        if (strLstSize(backupList) > 0)
        {
            const String *backupLabel = strLstGet(backupList, 0);
            const String *manifestFile = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strPtr(backupLabel));

            // Resumable backups have a copy of the manifest but no main
            if (storageExistsP(storageRepo(), strNewFmt("%s" INFO_COPY_EXT, strPtr(manifestFile))) &&
                !storageExistsP(storageRepo(), manifestFile))
            {
                bool usable = false;
                const String *reason = STRDEF("resume is disabled");
                Manifest *manifestResume = NULL;

                // Attempt to read the manifest file in the resumable backup to see if it can be used.  If any error at all occurs
                // then the backup will be considered unusable and a resume will not be attempted.
                if (cfgOptionBool(cfgOptResume))
                {
                    reason = strNewFmt("unable to read %s", strPtr(manifestFile));

                    TRY_BEGIN()
                    {
                        manifestResume = manifestLoadFile(
                            storageRepo(), manifestFile, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
                            infoPgCipherPass(infoBackupPg(infoBackup)));
                        const ManifestData *manifestResumeData = manifestData(manifestResume);

                        // Check version
                        if (!strEqZ(manifestResumeData->backrestVersion, PROJECT_VERSION))
                        {
                            reason = strNewFmt(
                                "new " PROJECT_NAME " version '%s' does not match resumable " PROJECT_NAME " version '%s'",
                                PROJECT_VERSION, strPtr(manifestResumeData->backrestVersion));
                        }
                        // Check backup type
                        else if (manifestResumeData->backupType != backupType(cfgOptionStr(cfgOptType)))
                        {
                            reason = strNewFmt(
                                "new backup type '%s' does not match resumable backup type '%s'", strPtr(cfgOptionStr(cfgOptType)),
                                strPtr(backupTypeStr(manifestResumeData->backupType)));
                        }
                        else if (!strEq(
                                    manifestResumeData->backupLabelPrior,
                                    manifestData(manifest)->backupLabelPrior ? manifestData(manifest)->backupLabelPrior : NULL))
                        {
                            reason = strNewFmt(
                                "new prior backup label '%s' does not match resumable prior backup label '%s'",
                                manifestResumeData->backupLabelPrior ? strPtr(manifestResumeData->backupLabelPrior) : "<undef>",
                                manifestData(manifest)->backupLabelPrior ?
                                    strPtr(manifestData(manifest)->backupLabelPrior) : "<undef>");
                        }
                        // Check compression
                        else if (manifestResumeData->backupOptionCompress != cfgOptionBool(cfgOptCompress))
                        {
                            reason = strNewFmt(
                                "new compress option '%s' does not match resumable compress option '%s'",
                                cvtBoolToConstZ(cfgOptionBool(cfgOptCompress)),
                                cvtBoolToConstZ(manifestResumeData->backupOptionCompress));
                        }
                        // Check hardlink
                        else if (manifestResumeData->backupOptionHardLink != cfgOptionBool(cfgOptRepoHardlink))
                        {
                            reason = strNewFmt(
                                "new hardlink option '%s' does not match resumable hardlink option '%s'",
                                cvtBoolToConstZ(cfgOptionBool(cfgOptRepoHardlink)),
                                cvtBoolToConstZ(manifestResumeData->backupOptionHardLink));
                        }
                        else
                            usable = true;
                    }
                    CATCH_ANY()
                    {
                    }
                    TRY_END();
                }

                // If the backup is usable then return the manifest
                if (usable)
                {
                    // !!! HACKY BIT TO MAKE PERL HAPPY
                    memContextSwitch(MEM_CONTEXT_OLD());
                    *backupLabelResume = strDup(backupLabel);
                    memContextSwitch(MEM_CONTEXT_TEMP());

                    result = manifestMove(manifestResume, MEM_CONTEXT_OLD());
                }
                // Else warn and remove the unusable backup
                else
                {
                    LOG_WARN_FMT("backup '%s' cannot be resumed: %s", strPtr(backupLabel), strPtr(reason));

                    storagePathRemoveP(
                        storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(backupLabel)), .recurse = true);
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupResume(const InfoBackup *infoBackup, Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    bool result = false;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *backupLabelResume = NULL;
        const Manifest *manifestResume = backupResumeFind(infoBackup, manifest, &backupLabelResume);

        // If a resumable backup was found set the label and cipher subpass
        if (manifestResume)
        {
            // Resuming
            result = true;

            // Set the backup label to the resumed backup !!! WE SHOULD BE ABLE TO USE LABEL STORED IN RESUME MANIFEST BUT THE PERL
            // TESTS ARE NOT SETTING THIS VALUE CORRECTLY.  SHOULD FIX BEFORE RELEASE.
            manifestBackupLabelSet(manifest, backupLabelResume);

            LOG_WARN_FMT(
                "resumable backup %s of same type exists -- remove invalid files and resume",
                strPtr(manifestData(manifest)->backupLabel));

            // If resuming a full backup then copy cipher subpass since it was used to encrypt the resumable files
            if (manifestData(manifest)->backupType == backupTypeFull)
                manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestResume));

            // Clean resumed backup
            BackupResumeData resumeData =
            {
                .manifest = manifest,
                .manifestResume = manifestResume,
                .compressed = cfgOptionBool(cfgOptCompress),
                .backupPath = strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(manifestData(manifest)->backupLabel)),
            };

            storageInfoListP(storageRepo(), resumeData.backupPath, backupResumeCallback, &resumeData, .sortOrder = sortOrderAsc);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, result);
}

/***********************************************************************************************************************************
Start the backup
***********************************************************************************************************************************/
static void
backupStart(BackupPg *pg)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_PG, pg);
    FUNCTION_LOG_END();

    // If this is an offline backup
    if (!cfgOptionBool(cfgOptOnline))
    {
        // If checksum-page is not explicitly enabled then disable it.  We can now detect checksums by reading pg_control directly
        // but the integration tests can't properly enable checksums.
        if (!cfgOptionTest(cfgOptChecksumPage))
            cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);

        // Check if Postgres is running and if so only continue when forced
        if (storageExistsP(pg->storagePrimary, PG_FILE_POSTMASTERPID_STR))
        {
            if (cfgOptionBool(cfgOptForce))
            {
                LOG_WARN(
                    "--no-" CFGOPT_ONLINE " passed and " PG_FILE_POSTMASTERPID " exists but --" CFGOPT_FORCE " was passed so backup"
                    " will continue though it looks like the postmaster is running and the backup will probably not be consistent");
            }
            else
            {
                THROW(
                    PostmasterRunningError,
                    "--no-" CFGOPT_ONLINE " passed but " PG_FILE_POSTMASTERPID " exists - looks like the postmaster is running."
                    " Shutdown the postmaster and try again, or use --force.");
            }
        }
    }
    // Else start the backup normally
    else
    {
        THROW(AssertError, "ONLINE BACKUPS DISABLED DURING DEVELOPMENT");

        // # Start the backup
        // ($strArchiveStart, $strLsnStart, $iWalSegmentSize) =
        //     $oDbMaster->backupStart(
        //         PROJECT_NAME . ' backup started at ' . timestampFormat(undef, $rhParam->{timestampStart}),
        //         cfgOption(CFGOPT_START_FAST));
        //
        // # Record the archive start location
        // $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_ARCHIVE_START, undef, $strArchiveStart);
        // $oBackupManifest->set(MANIFEST_SECTION_BACKUP, MANIFEST_KEY_LSN_START, undef, $strLsnStart);
        // &log(INFO, "backup start archive = ${strArchiveStart}, lsn = ${strLsnStart}");
        //
        // # Get the timeline from the archive
        // $strTimelineCurrent = substr($strArchiveStart, 0, 8);
        //
        // # Get tablespace map
        // $hTablespaceMap = $oDbMaster->tablespaceMapGet();
        //
        // # Get database map
        // $hDatabaseMap = $oDbMaster->databaseMapGet();
        //
        // # Wait for replay on the standby to catch up
        // if (cfgOption(CFGOPT_BACKUP_STANDBY))
        // {
        //     my ($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId) = $oDbStandby->info();
        //     $oBackupInfo->check($strStandbyDbVersion, $iStandbyControlVersion, $iStandbyCatalogVersion, $ullStandbyDbSysId);
        //
        //     $oDbStandby->configValidate();
        //
        //     &log(INFO, "wait for replay on the standby to reach ${strLsnStart}");
        //
        //     my ($strReplayedLSN, $strCheckpointLSN) = $oDbStandby->replayWait($strLsnStart);
        //
        //     &log(
        //         INFO,
        //         "replay on the standby reached ${strReplayedLSN}" .
        //             (defined($strCheckpointLSN) ? ", checkpoint ${strCheckpointLSN}" : ''));
        //
        //     # The standby db object won't be used anymore so undef it to catch any subsequent references
        //     undef($oDbStandby);
        //     protocolDestroy(CFGOPTVAL_REMOTE_TYPE_DB, $self->{iCopyRemoteIdx}, true);
        // }
        // !!! NEED TO SET START LSN HERE
    }

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Log the results of a job and throw errors
***********************************************************************************************************************************/
static uint64_t
backupJobResult(
    Manifest *manifest, const String *const fileLog, ProtocolParallelJob *job, const uint64_t sizeTotal, uint64_t sizeCopied,
    unsigned int pageSize)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
        FUNCTION_LOG_PARAM(STRING, fileLog);
        FUNCTION_LOG_PARAM(PROTOCOL_PARALLEL_JOB, job);
        FUNCTION_LOG_PARAM(UINT64, sizeTotal);
        FUNCTION_LOG_PARAM(UINT64, sizeCopied);
        FUNCTION_LOG_PARAM(UINT, pageSize);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);
    ASSERT(fileLog != NULL);
    ASSERT(job != NULL);

    // The job was successful
    if (protocolParallelJobErrorCode(job) == 0)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const ManifestFile *const file = manifestFileFind(manifest, varStr(protocolParallelJobKey(job)));
            const unsigned int processId = protocolParallelJobProcessId(job);

            const VariantList *const jobResult = varVarLst(protocolParallelJobResult(job));
            const BackupCopyResult copyResult = (BackupCopyResult)varUIntForce(varLstGet(jobResult, 0));
            const uint64_t copySize = varUInt64(varLstGet(jobResult, 1));
            const uint64_t repoSize = varUInt64(varLstGet(jobResult, 2));
            const String *const copyChecksum = varStr(varLstGet(jobResult, 3));
            const KeyValue *const checksumPageResult = varKv(varLstGet(jobResult, 4));

            // Increment backup copy progress
            sizeCopied += copySize;

            // Format log strings
            const String *const logProgress =
                strNewFmt("%s, %" PRIu64 "%%", strPtr(strSizeFormat(copySize)), sizeCopied * 100 / sizeTotal);
            const String *const logChecksum = copySize != 0 ? strNewFmt(" checksum %s", strPtr(copyChecksum)) : EMPTY_STR;

            // If the file is in a prior backup and nothing changed, then nothing needs to be done
            if (copyResult == backupCopyResultNoOp)
            {
                LOG_DETAIL_PID_FMT(
                    processId, "match file from prior backup %s (%s)%s", strPtr(fileLog), strPtr(logProgress), strPtr(logChecksum));
            }
            // Else if the file was removed during backup then remove from manifest
            else if (copyResult == backupCopyResultSkip)
            {
                // &log(DETAIL, 'skip file removed by database ' . (defined($strHost) ? "${strHost}:" : '') . $strDbFile);
                // $oManifest->remove(MANIFEST_SECTION_TARGET_FILE, $strRepoFile);
            }
            // Else file was copied so update manifest
            else
            {
                // If the the repo matched the expect checksum then log
                if (copyResult == backupCopyResultChecksum)
                {
                    LOG_DETAIL_PID_FMT(
                        processId, "checksum resumed file %s (%s)%s", strPtr(fileLog), strPtr(logProgress), strPtr(logChecksum));
                }
                // Else the file was copied
                else
                {
                    // If the file had to be recopied then warn that there may be an issue with corruption in the repository
                    // ??? This should really be below the message below for more context -- can be moved after the migration
                    // ??? The name should be a pg path not manifest name -- can be fixed after the migration
                    if (copyResult == backupCopyResultReCopy)
                    {
                        LOG_WARN_FMT(
                            "resumed backup file %s does not have expected checksum %s. The file will be recopied and backup will"
                            " continue but this may be an issue unless the resumed backup path in the repository is known to be"
                            " corrupted.\n"
                            "NOTE: this does not indicate a problem with the PostgreSQL page checksums.",
                            strPtr(file->name), file->checksumSha1);
                    }

                    LOG_INFO_PID_FMT(
                        processId, "backup file %s (%s)%s", strPtr(fileLog), strPtr(logProgress), strPtr(logChecksum));

                    // If the file had page checksums calculated during the copy
                    bool checksumPageError = file->checksumPageError;
                    const VariantList *checksumPageErrorList = file->checksumPageErrorList;

                    if (checksumPageResult != NULL)
                    {
                        ASSERT(file->checksumPage);

                        if (varBool(kvGet(checksumPageResult, VARSTRDEF("valid"))))
                        {
                            checksumPageError = false;
                            checksumPageErrorList = NULL;
                        }
                        else
                        {
                            checksumPageError = true;

                            if (!varBool(kvGet(checksumPageResult, VARSTRDEF("align"))))
                            {
                                checksumPageErrorList = NULL;

                                // ??? Update formatting after migration
                                LOG_WARN_FMT(
                                    "page misalignment in file %s: file size %" PRIu64 " is not divisible by page size %u",
                                    strPtr(fileLog), copySize, pageSize);
                            }
                            else
                            {
                                // Format the psage checksum errors
                                checksumPageErrorList = varVarLst(kvGet(checksumPageResult, VARSTRDEF("error")));
                                ASSERT(varLstSize(checksumPageErrorList) > 0);

                                String *error = strNew("");
                                unsigned int errorTotalMin = 0;

                                for (unsigned int errorIdx = 0; errorIdx < varLstSize(checksumPageErrorList); errorIdx++)
                                {
                                    const Variant *const errorItem = varLstGet(checksumPageErrorList, errorIdx);

                                    // Add a comma if this is not the first item
                                    if (errorIdx != 0)
                                        strCat(error, ", ");

                                    // If an error range
                                    if (varType(errorItem) == varTypeVariantList)
                                    {
                                        const VariantList *const errorItemList = varVarLst(errorItem);
                                        ASSERT(varLstSize(errorItemList) == 2);

                                        strCatFmt(
                                            error, "%" PRIu64 "-%" PRIu64, varUInt64(varLstGet(errorItemList, 0)),
                                            varUInt64(varLstGet(errorItemList, 1)));
                                        errorTotalMin += 2;
                                    }
                                    else
                                    {
                                        ASSERT(varType(errorItem) == varTypeUInt64);

                                        strCatFmt(error, "%" PRIu64, varUInt64(errorItem));
                                        errorTotalMin++;
                                    }
                                }

                                // Make message plural when appropriate
                                const String *const plural = errorTotalMin > 1 ? STRDEF("s") : EMPTY_STR;

                                // ??? Update formatting after migration
                                LOG_WARN_FMT(
                                    "invalid page checksum%s found in file %s at page%s %s", strPtr(plural), strPtr(fileLog),
                                    strPtr(plural), strPtr(error));
                            }
                        }
                    }

                    // Remove any reference to the file's existence in a prior backup
                    // !!! THIS IS SO NOT KOSHER -- MAKE THIS A PARAM OF MANIFESTFILEUPDATE()
                    ((ManifestFile *)manifestFileFind(manifest, file->name))->reference = NULL;

                    manifestFileUpdate(
                        manifest, file->name, copySize, repoSize, copySize > 0 ? strPtr(copyChecksum) : "", NULL,
                        file->checksumPage, checksumPageError, checksumPageErrorList);
                }
            }
        }
        MEM_CONTEXT_TEMP_END();

        // Free the job
        protocolParallelJobFree(job);
    }
    // Else the job errored
    else
        THROW_CODE(protocolParallelJobErrorCode(job), strPtr(protocolParallelJobErrorMessage(job)));

    FUNCTION_LOG_RETURN(UINT64, sizeCopied);
}

/***********************************************************************************************************************************
Process the backup manifest
***********************************************************************************************************************************/
// Comparator to order ManifestFile objects by size then name
static int
backupProcessQueueComparator(const void *item1, const void *item2)
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

// Helper to generate the backup queues
static uint64_t
backupProcessQueue(Manifest *manifest, List **queueList)
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
        *queueList = lstNew(sizeof(List *));

        // Generate the list of processing queues (there is always at least one)
        StringList *targetList = strLstNew();
        strLstAdd(targetList, STRDEF(MANIFEST_TARGET_PGDATA "/"));

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
        {
            const ManifestTarget *target = manifestTarget(manifest, targetIdx);

            if (target->tablespaceId != 0)
                strLstAdd(targetList, strNewFmt("%s/", strPtr(target->name)));
        }

        // Generate the processing queues
        MEM_CONTEXT_BEGIN(lstMemContext(*queueList))
        {
            for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            {
                List *queue = lstNewP(sizeof(ManifestFile *), .comparator = backupProcessQueueComparator);
                lstAdd(*queueList, &queue);
            }
        }
        MEM_CONTEXT_END();

        // Now put all files into the processing queues
        bool delta = cfgOptionBool(cfgOptDelta);

        for (unsigned int fileIdx = 0; fileIdx < manifestFileTotal(manifest); fileIdx++)
        {
            const ManifestFile *file = manifestFile(manifest, fileIdx);

            // If the file is a reference it should only be backed up if delta and not zero size
            if (file->reference != NULL && (!delta || file->size == 0))
                continue;

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

        // !!! pg_control should always be in the backup (unless this is an offline backup)
        // if (!$oBackupManifest->test(MANIFEST_SECTION_TARGET_FILE, MANIFEST_FILE_PGCONTROL) && cfgOption(CFGOPT_ONLINE))
        // {
        //     confess &log(ERROR, DB_FILE_PGCONTROL . " must be present in all online backups\n" .
        //                  'HINT: is something wrong with the clock or filesystem timestamps?', ERROR_FILE_MISSING);
        // }

        // !!! If there are no files to backup then we'll exit with an error unless in test mode.  The other way this could happen is if
        // !!! the database is down and backup is called with --no-online twice in a row.
        // if ($lFileTotal == 0 && !cfgOption(CFGOPT_TEST))
        // {
        //     confess &log(ERROR, "no files have changed since the last backup - this seems unlikely", ERROR_FILE_MISSING);
        // }

        // Sort the queues
        for (unsigned int targetIdx = 0; targetIdx < strLstSize(targetList); targetIdx++)
            lstSort(*(List **)lstGet(*queueList, targetIdx), sortOrderDesc);

        // Move process queues to calling context
        lstMove(*queueList, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT64, result);
}

// Helper to caculate the next queue to scan based on the client index
static int
backupJobQueueNext(unsigned int clientIdx, int queueIdx, unsigned int queueTotal)
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

// Callback to fetch backup jobs for the parallel executor
typedef struct BackupJobData
{
    const String *const backupLabel;                                // Backup label (defines the backup path)
    const String *const cipherSubPass;                              // Passphrase used to encrypt files in the backup
    const bool compress;                                            // Is the backup compressed?
    const unsigned int compressLevel;                               // Compress level if backup is compressed
    const bool delta;                                               // Is this a checksum delta backup?

    List *queueList;                                                // List of processing queues
} BackupJobData;

static ProtocolParallelJob *backupJobCallback(void *data, unsigned int clientIdx)
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
        BackupJobData *jobData = data;

        // Determine where to begin scanning the queue (we'll stop when we get back here)
        int queueIdx = (int)(clientIdx % lstSize(jobData->queueList));
        int queueEnd = queueIdx;

        do
        {
            List *queue = *(List **)lstGet(jobData->queueList, (unsigned int)queueIdx);

            if (lstSize(queue) > 0)
            {
                const ManifestFile *file = *(ManifestFile **)lstGet(queue, 0);

                // Create backup job
                ProtocolCommand *command = protocolCommandNew(PROTOCOL_COMMAND_BACKUP_FILE_STR);

                protocolCommandParamAdd(command, VARSTR(manifestPathPg(file->name)));

                protocolCommandParamAdd(command, VARBOOL(true)); // !!! NEED EXCEPTION FOR PG_CONTROL

                protocolCommandParamAdd(command, VARUINT64(file->size));
                protocolCommandParamAdd(command, file->checksumSha1[0] != 0 ? VARSTRZ(file->checksumSha1) : NULL);
                protocolCommandParamAdd(command, VARBOOL(file->checksumPage));
                protocolCommandParamAdd(command, VARUINT(0xFFFFFFFF)); // !!! COMBINE INTO ONE PARAM
                protocolCommandParamAdd(command, VARUINT(0xFFFFFFFF)); // !!! COMBINE INTO ONE PARAM
                protocolCommandParamAdd(command, VARSTR(file->name));
                protocolCommandParamAdd(command, VARBOOL(file->reference != NULL));
                protocolCommandParamAdd(command, VARBOOL(jobData->compress));
                protocolCommandParamAdd(command, VARBOOL(jobData->compressLevel));
                protocolCommandParamAdd(command, VARSTR(jobData->backupLabel));
                protocolCommandParamAdd(command, VARBOOL(jobData->delta));
                protocolCommandParamAdd(command, VARSTR(jobData->cipherSubPass));

                // Remove job from the queue
                lstRemoveIdx(queue, 0);

                // Assign job to result
                result = protocolParallelJobMove(protocolParallelJobNew(VARSTR(file->name), command), MEM_CONTEXT_OLD());

                // Break out of the loop early since we found a job
                break;
            }

            queueIdx = backupJobQueueNext(clientIdx, queueIdx, lstSize(jobData->queueList));
        }
        while (queueIdx != queueEnd);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(result);
}

static void
backupProcess(BackupPg pg, Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(BACKUP_PG, pg);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(manifest != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get backup info
        const BackupType backupType = manifestData(manifest)->backupType;
        const String *const backupLabel = manifestData(manifest)->backupLabel;
        const String *const backupPathExp = strNewFmt(STORAGE_REPO_BACKUP "/%s", strPtr(backupLabel));

        // If this is a full backup or hard-linked and paths are supported then create all paths explicitly so that empty paths will
        // exist in to repo.  Also create tablspace symlinks when symlinks are available,  This makes it possible for the user to
        // make a copy of the backup path and get a valid cluster.
        if ((backupType == backupTypeFull ||
                (cfgOptionBool(cfgOptRepoHardlink) && storageFeature(storageRepo(), storageFeatureHardLink))) &&
            storageFeature(storageRepo(), storageFeaturePath))
        {
            // Create paths
            for (unsigned int pathIdx = 0; pathIdx < manifestPathTotal(manifest); pathIdx++)
            {
                storagePathCreateP(
                    storageRepoWrite(), strNewFmt("%s/%s", strPtr(backupPathExp), strPtr(manifestPath(manifest, pathIdx)->name)));
            }

            // Create tablespace symlinks when available
            if (storageFeature(storageRepo(), storageFeatureSymLink))
            {
                for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(manifest); targetIdx++)
                {
                    const ManifestTarget *const target = manifestTarget(manifest, targetIdx);

                    if (target->tablespaceId != 0)
                    {
                        const String *const link = storagePathP(
                            storageRepo(),
                            strNewFmt("%s/" MANIFEST_TARGET_PGDATA "/%s", strPtr(backupPathExp), strPtr(target->name)));
                        const String *const linkDestination = strNewFmt(
                            "../../" MANIFEST_TARGET_PGTBLSPC "/%u", target->tablespaceId);

                        THROW_ON_SYS_ERROR_FMT(
                            symlink(strPtr(linkDestination), strPtr(link)) == -1, FileOpenError,
                            "unable to create symlink '%s' to '%s'", strPtr(link), strPtr(linkDestination));
                    }
                }
            }
        }

        // Generate processing queues
        BackupJobData jobData =
        {
            .backupLabel = backupLabel,
            .compress = cfgOptionBool(cfgOptCompress),
            .compressLevel = cfgOptionUInt(cfgOptCompressLevel),
            .cipherSubPass = manifestCipherSubPass(manifest),
            .delta = cfgOptionBool(cfgOptDelta),
        };

        uint64_t sizeTotal = backupProcessQueue(manifest, &jobData.queueList);

        // Create the parallel executor
        ProtocolParallel *parallelExec = protocolParallelNew(
            (TimeMSec)(cfgOptionDbl(cfgOptProtocolTimeout) * MSEC_PER_SEC) / 2, backupJobCallback, &jobData);

        for (unsigned int processIdx = 1; processIdx <= cfgOptionUInt(cfgOptProcessMax); processIdx++)
            protocolParallelClientAdd(parallelExec, protocolLocalGet(protocolStorageTypePg, 1, processIdx));

        // Process jobs
        uint64_t sizeCopied = 0;

        do
        {
            unsigned int completed = protocolParallelProcess(parallelExec);

            for (unsigned int jobIdx = 0; jobIdx < completed; jobIdx++)
            {
                ProtocolParallelJob *job = protocolParallelResult(parallelExec);

                // !!! Should add hostname in here
                const String *const host = protocolParallelJobProcessId(job) == 0 ? pg.primaryHost : pg.standbyHost;
                String *const fileLog = strNew("");

                if (host != NULL)
                    strCatFmt(fileLog, "%s:", strPtr(host));

                strCat(
                    fileLog,
                    strPtr(
                        storagePathP(
                            pg.storagePrimary,
                            manifestPathPg(manifestFileFind(manifest, varStr(protocolParallelJobKey(job)))->name))));

                sizeCopied = backupJobResult(manifest, fileLog, job, sizeTotal, sizeCopied, pg.pageSize);
            }

            // A keep-alive is required here for the remote holding open the backup connection
            protocolKeepAlive();
        }
        while (!protocolParallelDone(parallelExec));

        // # Determine how often the manifest will be saved
        // my $lManifestSaveCurrent = 0;
        // my $lManifestSaveSize = int($lSizeTotal / 100);
        //
        // if (cfgOptionSource(CFGOPT_MANIFEST_SAVE_THRESHOLD) ne CFGDEF_SOURCE_DEFAULT ||
        //     $lManifestSaveSize < cfgOption(CFGOPT_MANIFEST_SAVE_THRESHOLD))
        // {
        //     $lManifestSaveSize = cfgOption(CFGOPT_MANIFEST_SAVE_THRESHOLD);
        // }
        //
        //     # Determine whether to save the manifest
        //     $lManifestSaveCurrent += $lSize;
        //
        //     if ($lManifestSaveCurrent >= $lManifestSaveSize)
        //     {
        //         $oManifest->saveCopy();
        //
        //         logDebugMisc
        //         (
        //             $strOperation, 'save manifest',
        //             {name => 'lManifestSaveSize', value => $lManifestSaveSize},
        //             {name => 'lManifestSaveCurrent', value => $lManifestSaveCurrent}
        //         );
        //
        //         $lManifestSaveCurrent = 0;
        //     }

        LOG_INFO_FMT("%s backup size = %s", strPtr(backupTypeStr(backupType)), strPtr(strSizeFormat(sizeTotal)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Make a backup
***********************************************************************************************************************************/
void
cmdBackup(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    // Get the start timestamp which will later be written into the manifest to track total backup time
    time_t timestampStart = time(NULL);

    // Verify the repo is local
    repoIsLocalVerify();

    // Test for stop file
    lockStopTest();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Load backup.info
        InfoBackup *infoBackup = infoBackupLoadFileReconstruct(
            storageRepo(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));
        InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));

        // Get pg storage and database objects
        BackupPg pg = backupPgGet(infoBackup);

        // Check if there is a prior manifest if diff/incr
        Manifest *manifestPrior = backupBuildIncrPrior(infoBackup);

        // Start the backup
        backupStart(&pg);

        // Build the manifest
        Manifest *manifest = manifestNewBuild(
            pg.storagePrimary, infoPg.version, cfgOptionBool(cfgOptOnline), cfgOptionBool(cfgOptChecksumPage),
            strLstNewVarLst(cfgOptionLst(cfgOptExclude)));

        // !!! NEED TO GET THIS FROM THE REMOTE AND WAIT REMAINDER WHEN ONLINE
        time_t timestampCopyStart = time(NULL);

        manifestBuildValidate(manifest, cfgOptionBool(cfgOptDelta), timestampCopyStart);

        // Build an incremental backup if type is not full (manifestPrior will be freed in this call)
        if (!backupBuildIncr(infoBackup, manifest, manifestPrior))
            manifestCipherSubPassSet(manifest, cipherPassGen(cipherType(cfgOptionStr(cfgOptRepoCipherType))));

        // Set delta if it is not already set and the manifest requires it
        if (!cfgOptionBool(cfgOptDelta) && varBool(manifestData(manifest)->backupOptionDelta))
            cfgOptionSet(cfgOptDelta, cfgSourceParam, BOOL_TRUE_VAR);

        // Resume a backup when possible
        if (!backupResume(infoBackup, manifest))
        {
            manifestBackupLabelSet(
                manifest,
                backupLabel(backupType(cfgOptionStr(cfgOptType)), manifestData(manifest)->backupLabelPrior, timestampStart));
        }

        // Process the backup manifest
        backupProcess(pg, manifest);

        // Set the values required to complete the manifest
        manifestBuildComplete(
            // !!! SEEMS LIKE THE ARCHIVE CHECK CALCULATION SHOULD BE ONLINE ONLY (WAS COPIED FROM PERL, THOUGH)
            manifest, timestampStart, infoPg.id, infoPg.systemId, !cfgOptionBool(cfgOptOnline) || cfgOptionBool(cfgOptArchiveCheck),
            !cfgOptionBool(cfgOptOnline) || (cfgOptionBool(cfgOptArchiveCheck) && cfgOptionBool(cfgOptArchiveCopy)),
            cfgOptionUInt(cfgOptBufferSize), cfgOptionBool(cfgOptCompress), cfgOptionUInt(cfgOptCompressLevel),
            cfgOptionUInt(cfgOptCompressLevelNetwork), cfgOptionBool(cfgOptRepoHardlink), cfgOptionBool(cfgOptOnline),
            cfgOptionUInt(cfgOptProcessMax), cfgOptionBool(cfgOptBackupStandby));

        // !!! BELOW NEEDED FOR PERL MIGRATION
        // !!! ---------------------------------------------------------------------------------------------------------------------

        // Parameters that must be passed to Perl during migration
        KeyValue *paramKv = kvNew();
        kvPut(paramKv, VARSTRDEF("timestampStart"), VARUINT64((uint64_t)timestampStart));
        kvPut(paramKv, VARSTRDEF("pgId"), VARUINT(infoPg.id));
        kvPut(paramKv, VARSTRDEF("pgVersion"), VARSTR(pgVersionToStr(infoPg.version)));
        kvPut(paramKv, VARSTRDEF("backupLabel"), VARSTR(manifestData(manifest)->backupLabel));

        StringList *paramList = strLstNew();
        strLstAdd(paramList, jsonFromVar(varNewKv(paramKv)));
        cfgCommandParamSet(paramList);

        // Save the manifest so the Perl code can read it
        IoWrite *write = storageWriteIo(
            storageNewWriteP(
                storageRepoWrite(),
                strNewFmt(
                    STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE INFO_COPY_EXT, strPtr(manifestData(manifest)->backupLabel))));
        cipherBlockFilterGroupAdd(
            ioWriteFilterGroup(write), cipherType(cfgOptionStr(cfgOptRepoCipherType)), cipherModeEncrypt,
            infoPgCipherPass(infoBackupPg(infoBackup)));
        manifestSave(manifest, write);

        // Save an original copy so we can see what the C code wrote out
        // write = storageWriteIo(storageNewWriteP(storageRepoWrite(), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_MANIFEST_FILE ".orig")));
        // manifestSave(manifest, write);

        // Do this so Perl does not need to reconstruct backup.info
        infoBackupSaveFile(
            infoBackup, storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR, cipherType(cfgOptionStr(cfgOptRepoCipherType)),
            cfgOptionStr(cfgOptRepoCipherPass));

        // Shutdown protocol so Perl can take locks
        protocolFree();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
