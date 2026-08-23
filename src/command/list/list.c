/***********************************************************************************************************************************
List Command
***********************************************************************************************************************************/
#include <build.h>

#include <string.h>
#include <unistd.h>

#include "command/list/list.h"
#include "command/lock.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "config/config.h"
#include "info/infoBackup.h"
#include "postgres/interface.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Value shown for data the backup does not record, e.g. the LSN range of a backup made before it was stored in backup.info
STRING_STATIC(LIST_VALUE_UNKNOWN_STR,                               "-");

// WAL required for consistency is either copied into the backup or read from the archive
STRING_STATIC(LIST_WAL_MODE_ARCHIVE_STR,                            "ARCHIVE");
STRING_STATIC(LIST_WAL_MODE_STREAM_STR,                             "STREAM");

// Errors were or were not detected during the backup
STRING_STATIC(LIST_STATUS_ERROR_STR,                                "ERROR");
STRING_STATIC(LIST_STATUS_OK_STR,                                   "OK");

// A backup is running but has not reported how far it has got
STRING_STATIC(LIST_STATUS_RUNNING_STR,                              "RUNNING");

// Space before the first column of a row
#define LIST_INDENT_SIZE                                            1

// Space between columns of a row
#define LIST_SEPARATOR_SIZE                                         2

/***********************************************************************************************************************************
Data types and structures
***********************************************************************************************************************************/
// Column of the table, in the order the columns are output
typedef struct ListColumn
{
    const char *header;                                             // Header the column is labeled with
    bool alignRight;                                                // Align the values right rather than left?
} ListColumn;

static const ListColumn listColumnList[] =
{
    {.header = "Repo"},
    {.header = "Version"},
    {.header = "ID"},
    {.header = "Recovery Time"},
    {.header = "Mode"},
    {.header = "WAL Mode"},
    {.header = "TLI"},
    {.header = "Time", .alignRight = true},
    {.header = "Data", .alignRight = true},
    {.header = "Zratio", .alignRight = true},
    {.header = "Start LSN"},
    {.header = "Stop LSN"},
    {.header = "Status"},
};

#define LIST_COLUMN_TOTAL                                           LENGTH_OF(listColumnList)

// Index of each column, which must be kept synced with listColumnList
typedef enum
{
    listColumnRepo,
    listColumnVersion,
    listColumnId,
    listColumnRecoveryTime,
    listColumnMode,
    listColumnWalMode,
    listColumnTli,
    listColumnTime,
    listColumnData,
    listColumnZratio,
    listColumnLsnStart,
    listColumnLsnStop,
    listColumnStatus,
} ListColumnIdx;

// A backup as it will be output, i.e. one row of the table
typedef struct ListBackup
{
    const String *label;                                            // Label the backups are ordered by, NULL when still running
    bool inProgress;                                                // Is the backup still running, i.e. not yet in backup.info?
    unsigned int repoKey;                                           // Repo the backup was found in, which orders duplicate labels
    const String *valueList[LIST_COLUMN_TOTAL];                     // Value output in each column
} ListBackup;

// A stanza as it will be output, i.e. one table
typedef struct ListStanza
{
    const String *name;                                             // Stanza name, which must be first to allow for list sorting
    List *backupList;                                               // Backups found for the stanza on all repos
} ListStanza;

#define FUNCTION_LOG_LIST_STANZA_TYPE                                                                                              \
    ListStanza *
#define FUNCTION_LOG_LIST_STANZA_FORMAT(value, buffer, bufferSize)                                                                 \
    objNameToLog(value, "ListStanza", buffer, bufferSize)

/***********************************************************************************************************************************
Order the backups of a stanza oldest to newest, which the label does since it begins with the time the backup started. The same
backup may be found in more than one repo, so the repo orders backups that share a label.
***********************************************************************************************************************************/
static int
listBackupComparator(const void *const item1, const void *const item2)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, item1);
        FUNCTION_TEST_PARAM_P(VOID, item2);
    FUNCTION_TEST_END();

    ASSERT(item1 != NULL);
    ASSERT(item2 != NULL);

    const ListBackup *const backup1 = item1;
    const ListBackup *const backup2 = item2;

    // A backup that is still running is ordered after every backup that has completed, since it is the newest of them
    int result = LST_COMPARATOR_CMP(backup1->inProgress, backup2->inProgress);

    if (result != 0)
        FUNCTION_TEST_RETURN(INT, result);

    // A backup that is still running has no label to order by, so the repo orders those on its own
    if (!backup1->inProgress)
    {
        result = strCmp(backup1->label, backup2->label);

        if (result != 0)
            FUNCTION_TEST_RETURN(INT, result);
    }

    FUNCTION_TEST_RETURN(INT, LST_COMPARATOR_CMP(backup1->repoKey, backup2->repoKey));
}

/***********************************************************************************************************************************
Format a duration in the two largest units it has, which is how a backup is usually talked about
***********************************************************************************************************************************/
static String *
listDurationFormat(const uint64_t duration)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(UINT64, duration);
    FUNCTION_TEST_END();

    String *result;

    if (duration < 60)
        result = strNewFmt("%" PRIu64 "s", duration);
    else if (duration < 3600)
        result = strNewFmt("%" PRIu64 "m:%" PRIu64 "s", duration / 60, duration % 60);
    else
        result = strNewFmt("%" PRIu64 "h:%" PRIu64 "m", duration / 3600, duration % 3600 / 60);

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Version of PostgreSQL the backup was made from, which the history the backup refers to says
***********************************************************************************************************************************/
static const String *
listPgVersion(const InfoPg *const infoPg, const unsigned int pgId)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_PG, infoPg);
        FUNCTION_TEST_PARAM(UINT, pgId);
    FUNCTION_TEST_END();

    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
    {
        const InfoPgData pgData = infoPgData(infoPg, pgIdx);

        if (pgData.id == pgId)
            FUNCTION_TEST_RETURN_CONST(STRING, pgVersionToStr(pgData.version));
    }

    FUNCTION_TEST_RETURN_CONST(STRING, LIST_VALUE_UNKNOWN_STR);
}

/***********************************************************************************************************************************
Timeline the backup was made on followed by the timeline of the backup it depends on, which is zero when it depends on none. Both
are read from the WAL the backup requires, so neither is known when the backup does not record it.
***********************************************************************************************************************************/
static const String *
listTimeline(const InfoBackup *const infoBackup, const InfoBackupData *const backupData)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_TEST_PARAM_P(INFO_BACKUP_DATA, backupData);
    FUNCTION_TEST_END();

    ASSERT(infoBackup != NULL);
    ASSERT(backupData != NULL);

    if (backupData->backupArchiveStop == NULL)
        FUNCTION_TEST_RETURN_CONST(STRING, LIST_VALUE_UNKNOWN_STR);

    uint32_t timelinePrior = 0;

    // The prior backup is expected to be in backup.info but do not depend on it, since backup.info may have been reconstructed
    // from a repo that no longer has the prior backup
    if (backupData->backupPrior != NULL && infoBackupLabelExists(infoBackup, backupData->backupPrior))
    {
        const InfoBackupData *const backupDataPrior = infoBackupDataByLabel(infoBackup, backupData->backupPrior);

        if (backupDataPrior->backupArchiveStop != NULL)
            timelinePrior = pgTimelineFromWalSegment(backupDataPrior->backupArchiveStop);
    }

    FUNCTION_TEST_RETURN_CONST(
        STRING, strNewFmt("%u/%u", pgTimelineFromWalSegment(backupData->backupArchiveStop), timelinePrior));
}

/***********************************************************************************************************************************
Add a backup to the stanza it belongs to
***********************************************************************************************************************************/
static void
listBackupAdd(
    List *const backupList, const InfoBackup *const infoBackup, const InfoBackupData *const backupData,
    const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, backupList);
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_TEST_PARAM_P(INFO_BACKUP_DATA, backupData);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(backupList != NULL);
    ASSERT(infoBackup != NULL);
    ASSERT(backupData != NULL);

    ListBackup backup =
    {
        .label = backupData->backupLabel,
        .repoKey = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
    };

    backup.valueList[listColumnRepo] = strNewZ(cfgOptionGroupName(cfgOptGrpRepo, repoIdx));
    backup.valueList[listColumnVersion] = listPgVersion(infoBackupPg(infoBackup), backupData->backupPgId);
    backup.valueList[listColumnId] = backupData->backupLabel;

    // The backup completed when it stopped, which is the earliest time it can recover to
    backup.valueList[listColumnRecoveryTime] = strNewTimeP("%Y-%m-%d %H:%M:%S%z", backupData->backupTimestampStop);
    backup.valueList[listColumnMode] = strNewStrId(backupData->backupType);
    backup.valueList[listColumnWalMode] =
        backupData->optionArchiveCopy ? LIST_WAL_MODE_STREAM_STR : LIST_WAL_MODE_ARCHIVE_STR;
    backup.valueList[listColumnTli] = listTimeline(infoBackup, backupData);
    backup.valueList[listColumnTime] = listDurationFormat(
        (uint64_t)(backupData->backupTimestampStop - backupData->backupTimestampStart));
    backup.valueList[listColumnData] = strSizeFormat(backupData->backupInfoSizeDelta);

    // The ratio of what the backup copied to the space it occupies in the repo, which cannot be calculated when the backup
    // occupies no space
    backup.valueList[listColumnZratio] =
        backupData->backupInfoRepoSizeDelta != 0 ?
            strNewDivP(backupData->backupInfoSizeDelta, backupData->backupInfoRepoSizeDelta, .precision = 2) :
            LIST_VALUE_UNKNOWN_STR;

    // The LSN range is not recorded by a backup made before it was added to backup.info
    backup.valueList[listColumnLsnStart] =
        backupData->backupLsnStart != NULL ? backupData->backupLsnStart : LIST_VALUE_UNKNOWN_STR;
    backup.valueList[listColumnLsnStop] =
        backupData->backupLsnStop != NULL ? backupData->backupLsnStop : LIST_VALUE_UNKNOWN_STR;

    // Whether errors were detected is not recorded by a backup made before it was added to backup.info
    if (backupData->backupError == NULL)
        backup.valueList[listColumnStatus] = LIST_VALUE_UNKNOWN_STR;
    else
        backup.valueList[listColumnStatus] = varBool(backupData->backupError) ? LIST_STATUS_ERROR_STR : LIST_STATUS_OK_STR;

    lstAdd(backupList, &backup);

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Add a row for a backup that is running, which backup.info does not know about yet. All that a running backup reports is how large
it will be and how far it has got, so that is all the row can say about it.
***********************************************************************************************************************************/
static void
listProgressAdd(List *const backupList, const String *const stanzaName, const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, backupList);
        FUNCTION_TEST_PARAM(STRING, stanzaName);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(backupList != NULL);
    ASSERT(stanzaName != NULL);

    // The backup lock is held for the length of a backup, and by expire as well, so a valid lock means one of them is running
    const LockReadResult lockResult = cmdLockRead(lockTypeBackup, stanzaName, repoIdx);

    if (lockResult.status == lockReadStatusValid)
    {
        ListBackup backup =
        {
            .inProgress = true,
            .repoKey = cfgOptionGroupIdxToKey(cfgOptGrpRepo, repoIdx),
        };

        // Nothing a completed backup records is known yet, so every column starts out unknown
        for (unsigned int columnIdx = 0; columnIdx < LIST_COLUMN_TOTAL; columnIdx++)
            backup.valueList[columnIdx] = LIST_VALUE_UNKNOWN_STR;

        backup.valueList[listColumnRepo] = strNewZ(cfgOptionGroupName(cfgOptGrpRepo, repoIdx));

        // Size of the backup, which is what it will be when it finishes rather than what has been copied so far
        if (lockResult.data.size != NULL)
            backup.valueList[listColumnData] = strSizeFormat(varUInt64(lockResult.data.size));

        // How far the backup has got, which expire does not report and neither does a backup that has not got far enough to know
        if (lockResult.data.percentComplete != NULL)
            backup.valueList[listColumnStatus] = strNewPct(varUInt(lockResult.data.percentComplete), 10000);
        else
            backup.valueList[listColumnStatus] = LIST_STATUS_RUNNING_STR;

        lstAdd(backupList, &backup);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Get the stanza from the stanza list, adding it when neither another repo nor a prior stanza name has added it already
***********************************************************************************************************************************/
static ListStanza *
listStanzaGet(List *const stanzaList, const String *const stanzaName)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, stanzaList);
        FUNCTION_TEST_PARAM(STRING, stanzaName);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(stanzaList != NULL);
    ASSERT(stanzaName != NULL);

    ListStanza *result = lstFind(stanzaList, &stanzaName);

    if (result == NULL)
    {
        const ListStanza stanzaAdd =
        {
            .name = strDup(stanzaName),
            .backupList = lstNewP(sizeof(ListBackup), .comparator = listBackupComparator),
        };

        result = lstAdd(stanzaList, &stanzaAdd);
    }

    FUNCTION_TEST_RETURN(LIST_STANZA, result);
}

/***********************************************************************************************************************************
Add the backups a repo has for a stanza
***********************************************************************************************************************************/
static void
listStanzaAdd(List *const backupList, const InfoBackup *const infoBackup, const unsigned int repoIdx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(LIST, backupList);
        FUNCTION_TEST_PARAM(INFO_BACKUP, infoBackup);
        FUNCTION_TEST_PARAM(UINT, repoIdx);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(backupList != NULL);
    ASSERT(infoBackup != NULL);

    for (unsigned int backupIdx = 0; backupIdx < infoBackupDataTotal(infoBackup); backupIdx++)
    {
        const InfoBackupData backupData = infoBackupData(infoBackup, backupIdx);

        // Skip the backup when a type was requested and this is not it
        if (cfgOptionTest(cfgOptType) && cfgOptionStrId(cfgOptType) != backupData.backupType)
            continue;

        listBackupAdd(backupList, infoBackup, &backupData, repoIdx);
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Pad a value out to the width of the column it is in
***********************************************************************************************************************************/
static void
listPadCat(String *const result, const size_t pad)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, result);
        FUNCTION_TEST_PARAM(SIZE, pad);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(result != NULL);

    for (size_t padIdx = 0; padIdx < pad; padIdx++)
        strCatChr(result, ' ');

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Render the table of backups for a stanza. Each column is as wide as the widest value it holds, so the table is only as wide as it
needs to be for the backups that are in it.
***********************************************************************************************************************************/
static void
listStanzaRender(String *const result, const String *const name, const List *const backupList)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, result);
        FUNCTION_TEST_PARAM(STRING, name);
        FUNCTION_TEST_PARAM(LIST, backupList);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(result != NULL);
    ASSERT(name != NULL);
    ASSERT(backupList != NULL);

    // Width of each column and of the table as a whole
    size_t widthList[LIST_COLUMN_TOTAL];
    size_t width = LIST_INDENT_SIZE + LIST_SEPARATOR_SIZE * (LIST_COLUMN_TOTAL - 1);

    for (unsigned int columnIdx = 0; columnIdx < LIST_COLUMN_TOTAL; columnIdx++)
    {
        widthList[columnIdx] = strlen(listColumnList[columnIdx].header);

        for (unsigned int backupIdx = 0; backupIdx < lstSize(backupList); backupIdx++)
        {
            const ListBackup *const backup = lstGet(backupList, backupIdx);

            if (strSize(backup->valueList[columnIdx]) > widthList[columnIdx])
                widthList[columnIdx] = strSize(backup->valueList[columnIdx]);
        }

        width += widthList[columnIdx];
    }

    // Stanza name, which is what the table is a table of
    strCatFmt(result, "STANZA '%s'\n", strZ(name));

    // Rule that runs the width of the table, which separates the header from the name above it and the backups below it
    String *const rule = strNew();

    for (size_t ruleIdx = 0; ruleIdx < width; ruleIdx++)
        strCatChr(rule, '-');

    // Header
    strCatFmt(result, "%s\n", strZ(rule));

    for (unsigned int columnIdx = 0; columnIdx < LIST_COLUMN_TOTAL; columnIdx++)
    {
        strCatZN(result, "  ", columnIdx == 0 ? LIST_INDENT_SIZE : LIST_SEPARATOR_SIZE);
        strCatZ(result, listColumnList[columnIdx].header);

        // The last column is not padded, since the padding would only be trailing space
        if (columnIdx < LIST_COLUMN_TOTAL - 1)
            listPadCat(result, widthList[columnIdx] - strlen(listColumnList[columnIdx].header));
    }

    strCatFmt(result, "\n%s\n", strZ(rule));

    // Backups
    for (unsigned int backupIdx = 0; backupIdx < lstSize(backupList); backupIdx++)
    {
        const ListBackup *const backup = lstGet(backupList, backupIdx);

        for (unsigned int columnIdx = 0; columnIdx < LIST_COLUMN_TOTAL; columnIdx++)
        {
            const size_t pad = widthList[columnIdx] - strSize(backup->valueList[columnIdx]);

            strCatZN(result, "  ", columnIdx == 0 ? LIST_INDENT_SIZE : LIST_SEPARATOR_SIZE);

            if (listColumnList[columnIdx].alignRight)
            {
                listPadCat(result, pad);
                strCat(result, backup->valueList[columnIdx]);
            }
            else
            {
                strCat(result, backup->valueList[columnIdx]);

                // The last column is not padded, since the padding would only be trailing space
                if (columnIdx < LIST_COLUMN_TOTAL - 1)
                    listPadCat(result, pad);
            }
        }

        strCatChr(result, '\n');
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Render the backups of every stanza the command was asked for
***********************************************************************************************************************************/
static String *
listRender(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get stanza if specified
        const String *const stanza = cfgOptionStrNull(cfgOptStanza);

        // Stanzas found on all repos, which are sorted for output once every repo has added the stanzas it has
        List *const stanzaList = lstNewP(sizeof(ListStanza), .comparator = lstComparatorStr);

        // Initialize the repo index
        unsigned int repoIdxMin = 0;
        unsigned int repoIdxMax = cfgOptionGroupIdxTotal(cfgOptGrpRepo) - 1;

        // If the repo was specified then set index to the array location and max to loop only once
        if (cfgOptionTest(cfgOptRepo))
        {
            repoIdxMin = cfgOptionGroupIdxDefault(cfgOptGrpRepo);
            repoIdxMax = repoIdxMin;
        }

        for (unsigned int repoIdx = repoIdxMin; repoIdx <= repoIdxMax; repoIdx++)
        {
            // Get the repo storage in case it is remote and encryption settings need to be pulled down
            const Storage *const storageRepo = storageRepoIdx(repoIdx);

            // Get the stanzas on this repo, or only the stanza that was requested
            StringList *stanzaNameList;

            if (stanza != NULL)
            {
                stanzaNameList = strLstNew();
                strLstAdd(stanzaNameList, stanza);
            }
            else
                stanzaNameList = strLstSort(storageListP(storageRepo, STORAGE_PATH_BACKUP_STR), sortOrderAsc);

            for (unsigned int stanzaIdx = 0; stanzaIdx < strLstSize(stanzaNameList); stanzaIdx++)
            {
                const String *const stanzaName = strLstGet(stanzaNameList, stanzaIdx);

                // The stanza is added before its backups are read, since a backup may be running for a stanza that has none yet
                List *const backupList = listStanzaGet(stanzaList, stanzaName)->backupList;

                // A stanza that has no backup.info on this repo has no completed backups to list here, which is the case for a
                // stanza that has not been created on this repo and for one that was requested but does not exist at all
                TRY_BEGIN()
                {
                    listStanzaAdd(
                        backupList,
                        infoBackupLoadFile(
                            storageRepo, strNewFmt(STORAGE_PATH_BACKUP "/%s/%s", strZ(stanzaName), INFO_BACKUP_FILE),
                            cfgCipherSpecMainIdx(repoIdx)),
                        repoIdx);
                }
                CATCH(FileMissingError)
                {
                    LOG_DETAIL_FMT(
                        "%s has no " INFO_BACKUP_FILE " for stanza '%s'", cfgOptionGroupName(cfgOptGrpRepo, repoIdx),
                        strZ(stanzaName));
                }
                TRY_END();

                // A backup that is running has no type, so it is not listed when the output was filtered on one
                if (!cfgOptionTest(cfgOptType))
                    listProgressAdd(backupList, stanzaName, repoIdx);
            }
        }

        // Render a table per stanza, skipping a stanza that has no backups to render
        String *const resultStr = strNew();

        lstSort(stanzaList, sortOrderAsc);

        for (unsigned int stanzaIdx = 0; stanzaIdx < lstSize(stanzaList); stanzaIdx++)
        {
            const ListStanza *const stanzaData = lstGet(stanzaList, stanzaIdx);

            if (lstEmpty(stanzaData->backupList))
                continue;

            // Add a blank line between stanzas
            if (!strEmpty(resultStr))
                strCatChr(resultStr, '\n');

            listStanzaRender(resultStr, stanzaData->name, lstSort(stanzaData->backupList, sortOrderAsc));
        }

        if (strEmpty(resultStr))
            strCatZ(resultStr, "No backups exist in the repository.\n");

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = strDup(resultStr);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdList(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ioFdWriteOneStr(STDOUT_FILENO, listRender());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
