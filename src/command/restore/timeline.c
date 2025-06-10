/***********************************************************************************************************************************
Timeline Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/restore/timeline.h"
#include "common/crypto/cipherBlock.h"
#include "common/log.h"
#include "config/config.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Parse history file
***********************************************************************************************************************************/
typedef struct HistoryItem
{
    unsigned int timeline;                                          // Parent timeline
    uint64_t lsn;                                                   // Boundary lsn
} HistoryItem;

static List *
historyParse(const String *const history)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, history);
    FUNCTION_LOG_END();

    List *const result = lstNewP(sizeof(HistoryItem));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const historyList = strLstNewSplitZ(history, "\n");

        for (unsigned int historyIdx = 0; historyIdx < strLstSize(historyList); historyIdx++)
        {
            const String *const line = strTrim(strLstGet(historyList, historyIdx));

            // Skip empty lines
            if (strSize(line) == 0)
                continue;

            // Skip comments. PostgreSQL does not write comments into history files but it does allow for them.
            if (strBeginsWithZ(line, "#"))
                continue;

            // Split line
            const StringList *const split = strLstNewSplitZ(line, "\t");

            if (strLstSize(split) < 2)
                THROW_FMT(FormatError, "invalid history line format '%s'", strZ(strLstGet(historyList, historyIdx)));

            const HistoryItem historyItem =
            {
                .timeline = cvtZToUInt(strZ(strLstGet(split, 0))),
                .lsn = pgLsnFromStr(strLstGet(split, 1)),
            };

            ASSERT(historyItem.lsn > 0);
            ASSERT(historyItem.timeline > 0);

            lstAdd(result, &historyItem);
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}

/***********************************************************************************************************************************
Load history file
***********************************************************************************************************************************/
static List *
historyLoad(
    const Storage *const storageRepo, const String *const archiveId, const unsigned int timeline, const CipherType cipherType,
    const String *const cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, timeline);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_LOG_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    List *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const historyFile = strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%08X.history", strZ(archiveId), timeline);
        StorageRead *const storageRead = storageNewReadP(storageRepo, historyFile);
        cipherBlockFilterGroupAdd(ioReadFilterGroup(storageReadIo(storageRead)), cipherType, cipherModeDecrypt, cipherPass);
        const Buffer *const history = storageGetP(storageRead);

        TRY_BEGIN()
        {
            result = lstMove(historyParse(strNewBuf(history)), memContextPrior());
        }
        CATCH_ANY()
        {
            THROW_FMT(
                FormatError, "invalid timeline '%X' at '%s': %s", timeline, strZ(storagePathP(storageRepo, historyFile)),
                errorMessage());
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}

/***********************************************************************************************************************************
Find latest timeline
***********************************************************************************************************************************/
static unsigned int
timelineLatest(const Storage *const storageRepo, const String *const archiveId, const unsigned int timelineCurrent)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, timelineCurrent);
    FUNCTION_LOG_END();

    ASSERT(storageRepo != NULL);

    unsigned int result = timelineCurrent;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *const historyList = strLstSort(
            storageListP(
                storageRepo, strNewFmt(STORAGE_REPO_ARCHIVE "/%s", strZ(archiveId)), .expression = STRDEF("[0-F]{8}\\.history")),
            sortOrderDesc);

        if (!strLstEmpty(historyList))
        {
            const unsigned int timelineLatest = cvtZToUIntBase(strZ(strSubN(strLstGet(historyList, 0), 0, 8)), 16);

            if (timelineLatest > timelineCurrent)
                result = timelineLatest;
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(UINT, result);
}

/**********************************************************************************************************************************/
STRING_STATIC(TIMELINE_CURRENT_STR,                                 "current");
STRING_STATIC(TIMELINE_LATEST_STR,                                  "latest");

FN_EXTERN void
timelineVerify(
    const Storage *const storageRepo, const String *const archiveId, const unsigned int pgVersion,
    const unsigned int timelineBackup, const uint64_t lsnBackup, const StringId recoveryType, const String *timelineTargetStr,
    const CipherType cipherType, const String *const cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT, timelineBackup);
        FUNCTION_LOG_PARAM(UINT64, lsnBackup);
        FUNCTION_LOG_PARAM(STRING_ID, recoveryType);
        FUNCTION_LOG_PARAM(STRING, timelineTargetStr);
        FUNCTION_LOG_PARAM(STRING_ID, cipherType);
        FUNCTION_LOG_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storageRepo != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(timelineBackup > 0);
    ASSERT(lsnBackup > 0);

    // If timeline target is not specified then set based on the default by PostgreSQL version. Force timeline to current when the
    // recovery type is immediate since no timeline switch will be needed but PostgreSQL will error if there is a problem with the
    // latest timeline.
    if (timelineTargetStr == NULL)
    {
        if (pgVersion <= PG_VERSION_11 || recoveryType == CFGOPTVAL_TYPE_IMMEDIATE)
            timelineTargetStr = TIMELINE_CURRENT_STR;
        else
            timelineTargetStr = TIMELINE_LATEST_STR;
    }

    // If target timeline is not current then verify that the timeline is valid based on backup timeline and lsn. Following the
    // current/backup timeline is guaranteed as long as the WAL is available -- no history file required.
    if (!strEq(timelineTargetStr, TIMELINE_CURRENT_STR))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Determine timeline target in order to load the history file. If target timeline is latest scan the history files to
            // find the most recent. If a target timeline is a number then parse it.
            unsigned int timelineTarget;

            if (strEq(timelineTargetStr, TIMELINE_LATEST_STR))
                timelineTarget = timelineLatest(storageRepo, archiveId, timelineBackup);
            else
            {
                TRY_BEGIN()
                {
                    if (strBeginsWithZ(timelineTargetStr, "0x"))
                        timelineTarget = cvtZToUIntBase(strZ(timelineTargetStr) + 2, 16);
                    else
                        timelineTarget = cvtZToUInt(strZ(timelineTargetStr));
                }
                CATCH_ANY()
                {
                    THROW_FMT(DbMismatchError, "invalid target timeline '%s'", strZ(timelineTargetStr));
                }
                TRY_END();
            }

            // Only proceed if target timeline is not the backup timeline
            if (timelineTarget != timelineBackup)
            {
                // Search through the history for the target timeline to make sure it includes the backup timeline. This follows
                // the logic in PostgreSQL's readTimeLineHistory() and tliOfPointInHistory() but is a bit simplified for our usage.
                const List *const historyList = historyLoad(storageRepo, archiveId, timelineTarget, cipherType, cipherPass);
                unsigned int timelineFound = 0;

                for (unsigned int historyIdx = 0; historyIdx < lstSize(historyList); historyIdx++)
                {
                    const HistoryItem *const historyItem = lstGet(historyList, historyIdx);

                    // If backup timeline exists in the target timeline's history before the fork LSN then restore can proceed
                    if (historyItem->timeline == timelineBackup && lsnBackup < historyItem->lsn)
                    {
                        timelineFound = timelineBackup;
                        break;
                    }
                }

                // Error when the timeline was not found or not found in the expected range
                if (timelineFound == 0)
                {
                    uint64_t lsnFound = 0;

                    // Check if timeline exists but did not fork off when expected
                    for (unsigned int historyIdx = 0; historyIdx < lstSize(historyList); historyIdx++)
                    {
                        const HistoryItem *const historyItem = lstGet(historyList, historyIdx);

                        if (historyItem->timeline == timelineBackup)
                        {
                            timelineFound = timelineBackup;
                            lsnFound = historyItem->lsn;
                        }
                    }

                    // The timeline was found but it forked off earlier than the backup LSN. This covers the common case where a
                    // standby is promoted by accident, which creates a new timeline, and then the user tries to restore a backup
                    // from the original primary that was made after the new timeline was created. Since PostgreSQL >= 12 will
                    // always try to follow the latest timeline this restore will fail since there is no path to the target
                    // timeline.
                    if (timelineFound != 0)
                    {
                        ASSERT(lsnFound != 0);

                        THROW_FMT(
                            DbMismatchError,
                            "target timeline %X forked from backup timeline %X at %s which is before backup lsn of %s\n"
                            "HINT: was the target timeline created by accidentally promoting a standby?\n"
                            "HINT: was the target timeline created by testing a restore without --archive-mode=off?\n"
                            "HINT: was the backup made after the target timeline was created?",
                            timelineTarget, timelineBackup, strZ(pgLsnToStr(lsnFound)), strZ(pgLsnToStr(lsnBackup)));
                    }
                    // Else the backup timeline was not found at all. This less common case occurs when, for example, a restore from
                    // timeline 4 results in timeline 7 being created on promotion because 5 and 6 have already been created by
                    // prior promotions and then the user tries to restore a backup from timeline 5 with a target of timeline 7.
                    // There is no relationship between timeline 5 and 7 in this case at any LSN range.
                    else
                    {
                        THROW_FMT(
                            DbMismatchError,
                            "backup timeline %X, lsn %s is not in the history of target timeline %X\n"
                            "HINT: was the target timeline created by promoting from a timeline < latest?",
                            timelineBackup, strZ(pgLsnToStr(lsnBackup)), timelineTarget);
                    }
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}
