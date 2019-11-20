/***********************************************************************************************************************************
Backup Command
***********************************************************************************************************************************/
#include "build.auto.h"

// #include <string.h>
// #include <sys/stat.h>
#include <time.h>
// #include <unistd.h>
//
#include "command/control/common.h"
#include "command/backup/backup.h"
#include "command/backup/common.h"
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
// #include "protocol/parallel.h"
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
    const Db *dbStandby;
} BackupPg;

static BackupPg
backupPgGet(const InfoBackup *infoBackup)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);

    // !!! PRETTY BADLY FAKED FOR NOW, SHOULD BE pgGet() KINDA THING
    BackupPg result = {.storagePrimary = storagePgId(1)};

    // Get control information from the primary and validate it against backup info
    InfoPgData infoPg = infoPgDataCurrent(infoBackupPg(infoBackup));
    PgControl pgControl = pgControlFromFile(result.storagePrimary);

    if (pgControl.version != infoPg.version || pgControl.systemId != infoPg.systemId)
    {
        THROW_FMT(
            BackupMismatchError,
            PG_NAME " version %s, system-id %" PRIu64 " do not match stanza version %s, system-id %" PRIu64,
            strPtr(pgVersionToStr(pgControl.version)), pgControl.systemId, strPtr(pgVersionToStr(infoPg.version)),
            infoPg.systemId);
    }

    // If backup from standby option is set but a standby was not configured in the config file or on the command line, then turn
    // off backup-standby and warn that backups will be performed from the prinary.
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

                LOG_INFO(
                    "last backup label = %s, version = %s", strPtr(backupLabelPrior), strPtr(manifestPriorData->backrestVersion));

                // Warn if compress option changed
                if (cfgOptionBool(cfgOptCompress) != manifestPriorData->backupOptionCompress)
                {
                    LOG_WARN(
                        "%s backup cannot alter compress option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptCompress)), strPtr(backupLabelPrior));
                    cfgOptionSet(cfgOptCompress, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionCompress));
                }

                // Warn if hardlink option changed
                if (cfgOptionBool(cfgOptRepoHardlink) != manifestPriorData->backupOptionHardLink)
                {
                    LOG_WARN(
                        "%s backup cannot alter hardlink option to '%s', reset to value in %s", strPtr(cfgOptionStr(cfgOptType)),
                        cvtBoolToConstZ(cfgOptionBool(cfgOptRepoHardlink)), strPtr(backupLabelPrior));
                    cfgOptionSet(cfgOptRepoHardlink, cfgSourceParam, VARBOOL(manifestPriorData->backupOptionHardLink));
                }
            }
            else
            {
                LOG_WARN("no prior backup exists, %s backup has been changed to full", strPtr(cfgOptionStr(cfgOptType)));
                cfgOptionSet(cfgOptType, cfgSourceParam, VARSTR(backupTypeStr(type)));
            }
        }

        manifestMove(result, MEM_CONTEXT_OLD());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(MANIFEST, result);
}

static bool
backupBuildIncr(const InfoBackup *infoBackup, Manifest *manifest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_LOG_PARAM(MANIFEST, manifest);
    FUNCTION_LOG_END();

    ASSERT(infoBackup != NULL);
    ASSERT(manifest != NULL);

    bool result = false;

    // No incremental if backup type is full
    if (backupType(cfgOptionStr(cfgOptType)) != backupTypeFull)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            Manifest *manifestPrior = backupBuildIncrPrior(infoBackup);

            if (backupType(cfgOptionStr(cfgOptType)) != backupTypeFull)
            {
                manifestBuildIncr(manifest, manifestPrior, backupType(cfgOptionStr(cfgOptType)));

                // !!! SHOULDN'T THIS LOGIC BE IN backupBuildIncrPrior()?
                // If not defined this backup was done in a version prior to page checksums being introduced.  Just set
                // checksum-page to false and move on without a warning.  Page checksums will start on the next full backup.
                if (manifestData(manifestPrior)->backupOptionChecksumPage == NULL)
                {
                    manifestChecksumPageSet(manifest, false);
                    cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, BOOL_FALSE_VAR);
                }
                // Don't allow the checksum-page option to change in a diff or incr backup.  This could be confusing as only certain
                // files would be checksummed and the list could be incomplete during reporting.
                else
                {
                    bool checksumPagePrior = varBool(manifestData(manifestPrior)->backupOptionChecksumPage);

                    if (checksumPagePrior != cfgOptionBool(cfgOptChecksumPage))
                    {
                        LOG_WARN(
                            "%s backup cannot alter '" CFGOPT_CHECKSUM_PAGE "' option to '%s', reset to '%s' from %s",
                            strPtr(cfgOptionStr(cfgOptType)), cvtBoolToConstZ(cfgOptionBool(cfgOptChecksumPage)),
                            cvtBoolToConstZ(checksumPagePrior), strPtr(manifestData(manifestPrior)->backupLabel));

                        manifestChecksumPageSet(manifest, checksumPagePrior);
                        cfgOptionSet(cfgOptChecksumPage, cfgSourceParam, VARBOOL(checksumPagePrior));
                    }
                }

                // Set the cipher subpass from prior manifest since we want a single subpass for the entire backup set
                manifestCipherSubPassSet(manifest, manifestCipherSubPass(manifestPrior));

                // Incremental was built
                result = true;
            }
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
                LOG_DETAIL("remove path '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
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
                file->size == fileResume->size && (resumeData->delta || file->timestamp == fileResume->timestamp))
            {
                manifestFileUpdate(
                    resumeData->manifest, manifestName, fileResume->sizeRepo, fileResume->checksumSha1, NULL,
                    fileResume->checksumPage, fileResume->checksumPageError, fileResume->checksumPageErrorList);
            }
            // Else remove the file
            else
            {
                LOG_DETAIL("remove file '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
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
            LOG_WARN("remove special file '%s' from resumed backup", strPtr(storagePathP(storageRepo(), backupPath)));
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
                    LOG_WARN("backup '%s' cannot be resumed: %s", strPtr(backupLabel), strPtr(reason));

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

            LOG_WARN(
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

        // Start the backup
        backupStart(&pg);

        // Build the manifest
        Manifest *manifest = manifestNewBuild(
            pg.storagePrimary, infoPg.version, false, strLstNewVarLst(cfgOptionLst(cfgOptExclude)));

        // !!! NEED TO GET THIS FROM THE REMOTE AND WAIT REMAINDER WHEN ONLINE
        time_t timestampCopyStart = time(NULL);

        manifestBuildValidate(manifest, cfgOptionBool(cfgOptDelta), timestampCopyStart);

        // Build an incremental backup if type is not full
        if (!backupBuildIncr(infoBackup, manifest))
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

        // Set the values required to complete the manifest
        manifestBuildComplete(
            // !!! SEEMS LIKE THE ARCHIVE CHECK CALCULATION SHOULD BE ONLINE ONLY (WAS COPIED FROM PERL, THOUGH)
            manifest, timestampStart, infoPg.id, infoPg.systemId, !cfgOptionBool(cfgOptOnline) || cfgOptionBool(cfgOptArchiveCheck),
            !cfgOptionBool(cfgOptOnline) || (cfgOptionBool(cfgOptArchiveCheck) && cfgOptionBool(cfgOptArchiveCopy)),
            cfgOptionUInt(cfgOptBufferSize), cfgOptionBool(cfgOptChecksumPage), cfgOptionBool(cfgOptCompress),
            cfgOptionUInt(cfgOptCompressLevel), cfgOptionUInt(cfgOptCompressLevelNetwork), cfgOptionBool(cfgOptRepoHardlink),
            cfgOptionBool(cfgOptOnline), cfgOptionUInt(cfgOptProcessMax), cfgOptionBool(cfgOptBackupStandby));

        // Backup from standby can only be used on PostgreSQL >= 9.1
        if (cfgOption(cfgOptOnline) && cfgOption(cfgOptBackupStandby) && infoPg.version < PG_VERSION_BACKUP_STANDBY)
        {
            THROW_FMT(
                ConfigError, "option '" CFGOPT_BACKUP_STANDBY "' not valid for " PG_NAME " < %s",
                strPtr(pgVersionToStr(PG_VERSION_BACKUP_STANDBY)));
        }

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
