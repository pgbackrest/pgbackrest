/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#include "command/expire/expire.h"
#include "common/debug.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "storage/helper.h"

// CSHANG Need to decide if this needs to be a structure and if so if it needs to be instantiated or just variables in cmdExpire
// struct archiveExpire
// {
//     uint64_t total;
//     String *start;
//     String *stop;
// };

// CSHANG Putting backupRegExpGet here until figure out where it should go
#define DATE_TIME_REGEX                                             "[0-9]{8}\\-[0-9]{6}"
/***********************************************************************************************************************************
Return a regex string for filtering backups based on the type
***********************************************************************************************************************************/
String *
backupRegExpGet(BackupRegExpGetParam param)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BOOL, param.full);
        FUNCTION_LOG_PARAM(BOOL, param.differential);
        FUNCTION_LOG_PARAM(BOOL, param.incremental);
        FUNCTION_LOG_PARAM(BOOL, param.anchor);
    FUNCTION_LOG_END();

    ASSERT(param.full || param.differential || param.incremental);

    String *result = NULL;

    // Start the expression with the anchor if requested, date/time regexp and full backup indicator
    if (param.anchor)
        result = strNew("^" DATE_TIME_REGEX "F");
    else
        result = strNew(DATE_TIME_REGEX "F");

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Add the diff and/or incr expressions if requested
        if (param.differential || param.incremental)
        {
            // If full requested then diff/incr is optional
            if (param.full)
            {
                strCat(result, "(\\_");
            }
            // Else diff/incr is required
            else
            {
                strCat(result, "\\_");
            }

            // Append date/time regexp for diff/incr
            strCat(result, DATE_TIME_REGEX);

            // Filter on both diff/incr
            if (param.differential && param.incremental)
            {
                strCat(result, "(D|I)");
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
                strCat(result, "){0,1}");
            }
        }

        // Append the end anchor if requested
        if (param.anchor)
            strCat(result, "$");    // CSHANG Did not like the "\$", errors with: unknown escape sequence: '\$'
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}


/***********************************************************************************************************************************
Expire backups and archives
***********************************************************************************************************************************/
void
cmdExpire(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get the retention options
        // ??? Retention options will need to be indexed
        unsigned int fullRetention =
            cfgOptionTest(cfgOptRepoRetentionFull) ? (unsigned int) cfgOptionInt(cfgOptRepoRetentionFull) : 0;
        // int differentialRetention = cfgOptionTest(cfgOptRepoRetentionDiff) ? cfgOptionInt(cfgOptRepoRetentionDiff) : 0;
        // String *archiveRetentionType =
        //     cfgOptionTest(cfgOptRepoRetentionArchiveType) ? cfgOptionStr(cfgOptRepoRetentionArchiveType) : NULL;
        // int archiveRetention = cfgOptionTest(cfgOptRepoRetentionArchive) ? cfgOptionInt(cfgOptRepoRetentionArchive) : 0;

        // Load the backup.info
        String *stanzaBackupPath = strNewFmt(STORAGE_PATH_BACKUP "/%s", strPtr(cfgOptionStr(cfgOptStanza)));
        InfoBackup *infoBackup = infoBackupNew(
            storageRepo(), strNewFmt("%s/%s", strPtr(stanzaBackupPath), INFO_BACKUP_FILE), false,
            cipherType(cfgOptionStr(cfgOptRepoCipherType)), cfgOptionStr(cfgOptRepoCipherPass));

        StringList *pathList = NULL;

        // Find all the expired full backups
        if (fullRetention > 0)
        {
            pathList = infoBackupDataLabelListP(infoBackup, .filter = backupRegExpGetP(.full = true));
        }

        LOG_INFO("pathlist %s", strPtr(strLstGet(pathList, 0)));

        if (strLstSize(pathList) > fullRetention)
        {
            LOG_INFO("pathlist > fullretention");
        }

    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
