/***********************************************************************************************************************************
Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/backup/common.h"
#include "common/debug.h"
#include "common/log.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define DATE_TIME_REGEX                                             "[0-9]{8}\\-[0-9]{6}"
#define BACKUP_LINK_LATEST                                          "latest"

/**********************************************************************************************************************************/
String *
backupFileRepoPath(const String *const backupLabel, const BackupFileRepoPathParam param)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(STRING, param.manifestName);
        FUNCTION_TEST_PARAM(UINT64, param.bundleId);
        FUNCTION_TEST_PARAM(ENUM, param.compressType);
    FUNCTION_TEST_END();

    ASSERT(backupLabel != NULL);
    ASSERT(param.bundleId != 0 || param.manifestName != NULL);

    String *const result = strCatFmt(strNew(), STORAGE_REPO_BACKUP "/%s/", strZ(backupLabel));

    if (param.bundleId != 0)
        strCatFmt(result, MANIFEST_PATH_BUNDLE "/%" PRIu64, param.bundleId);
    else
        strCatFmt(result, "%s%s", strZ(param.manifestName), strZ(compressExtStr(param.compressType)));

    FUNCTION_TEST_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
backupLabelFormat(BackupType type, const String *backupLabelPrior, time_t timestamp)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING_ID, type);
        FUNCTION_LOG_PARAM(STRING, backupLabelPrior);
        FUNCTION_LOG_PARAM(TIME, timestamp);
    FUNCTION_LOG_END();

    ASSERT((type == backupTypeFull && backupLabelPrior == NULL) || (type != backupTypeFull && backupLabelPrior != NULL));
    ASSERT(timestamp > 0);

    // Format the timestamp
    struct tm timePart;
    char buffer[16];

    THROW_ON_SYS_ERROR(
        strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", localtime_r(&timestamp, &timePart)) == 0, AssertError,
        "unable to format time");

    // If full label
    String *result = NULL;

    if (type == backupTypeFull)
    {
        result = strNewFmt("%sF", buffer);
    }
    // Else diff or incr label
    else
    {
        // Get the full backup portion of the prior backup label and append the diff/incr timestamp
        result = strNewFmt("%.16s_%s%s", strZ(backupLabelPrior), buffer, type == backupTypeDiff ? "D" : "I");
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
backupRegExp(const BackupRegExpParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, param.full);
        FUNCTION_LOG_PARAM(BOOL, param.differential);
        FUNCTION_LOG_PARAM(BOOL, param.incremental);
        FUNCTION_LOG_PARAM(BOOL, param.noAnchorEnd);
    FUNCTION_LOG_END();

    ASSERT(param.full || param.differential || param.incremental);

    // Start the expression with the anchor, date/time regexp and full backup indicator
    String *const result = strCatZ(strNew(), "^" DATE_TIME_REGEX "F");

    // Add the diff and/or incr expressions if requested
    if (param.differential || param.incremental)
    {
        // If full requested then diff/incr is optional
        if (param.full)
        {
            strCatZ(result, "(\\_");
        }
        // Else diff/incr is required
        else
        {
            strCatZ(result, "\\_");
        }

        // Append date/time regexp for diff/incr
        strCatZ(result, DATE_TIME_REGEX);

        // Filter on both diff/incr
        if (param.differential && param.incremental)
        {
            strCatZ(result, "(D|I)");
        }
        // Else just diff
        else if (param.differential)
        {
            strCatChr(result, 'D');
        }
        // Else just incr
        else
        {
            strCatChr(result, 'I');
        }

        // If full requested then diff/incr is optional
        if (param.full)
        {
            strCatZ(result, "){0,1}");
        }
    }

    // Append the end anchor
    if (!param.noAnchorEnd)
        strCatZ(result, "$");

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
void
backupLinkLatest(const String *backupLabel, unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, backupLabel);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create a symlink to the most recent backup if supported.  This link is purely informational for the user and is never
        // used by us since symlinks are not supported on all storage types.
        // -------------------------------------------------------------------------------------------------------------------------
        const String *const latestLink = storagePathP(storageRepoIdx(repoIdx), STRDEF(STORAGE_REPO_BACKUP "/" BACKUP_LINK_LATEST));

        // Remove an existing latest link/file in case symlink capabilities have changed
        storageRemoveP(storageRepoIdxWrite(repoIdx), latestLink);

        if (storageFeature(storageRepoIdxWrite(repoIdx), storageFeatureSymLink))
            storageLinkCreateP(storageRepoIdxWrite(repoIdx), backupLabel, latestLink);

        // Sync backup path if required
        if (storageFeature(storageRepoIdxWrite(repoIdx), storageFeaturePathSync))
            storagePathSyncP(storageRepoIdxWrite(repoIdx), STORAGE_REPO_BACKUP_STR);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN_VOID();
}
