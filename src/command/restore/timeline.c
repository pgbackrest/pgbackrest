/***********************************************************************************************************************************
Timeline Management
***********************************************************************************************************************************/
#include "build.auto.h"

#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "command/restore/timeline.h"
#include "common/log.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
!!!
***********************************************************************************************************************************/
typedef struct HistoryItem
{
    unsigned int timeline;                                          // parent timeline
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

/**********************************************************************************************************************************/
static List *
historyLoad(const Storage *const storageRepo, const String *const archiveId, const unsigned int timeline)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, timeline);
    FUNCTION_LOG_END();

    List *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const historyFile = strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%08X.history", strZ(archiveId), timeline);
        const Buffer *const history = storageGetP(storageNewReadP(storageRepo, historyFile));

        TRY_BEGIN()
        {
            result = lstMove(historyParse(strNewBuf(history)), memContextPrior());
        }
        CATCH_ANY()
        {
            THROW_FMT(FormatError, "unable to parse '%s': %s", strZ(storagePathP(storageRepo, historyFile)), errorMessage());
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(LIST, result);
}

/**********************************************************************************************************************************/
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
    const unsigned int timelineCurrent, const uint64_t lsnCurrent, const String *timelineTargetStr)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storageRepo);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT, timelineCurrent);
        FUNCTION_LOG_PARAM(UINT64, lsnCurrent);
        FUNCTION_LOG_PARAM(STRING, timelineTargetStr);
    FUNCTION_LOG_END();

    ASSERT(storageRepo != NULL);
    ASSERT(archiveId != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(timelineCurrent > 0);
    ASSERT(lsnCurrent > 0);

    // If timeline target is not specified then set based on the default by PostgreSQL version
    if (timelineTargetStr == NULL)
    {
        if (pgVersion <= PG_VERSION_11)
            timelineTargetStr = TIMELINE_CURRENT_STR;
        else
            timelineTargetStr = TIMELINE_LATEST_STR;
    }

    // If target timeline is not current then verify that the timeline is valid based on current timeline and lsn. Following the
    // current timeline is guaranteed as long as the WAL is available -- no history file required.
    if (!strEq(timelineTargetStr, TIMELINE_CURRENT_STR))
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Determine timeline target in order to load the history file. If target timeline is latest scan the history files to
            // find the the most recent. If a target timeline is a number then parse it.
            unsigned int timelineTarget;

            if (strEq(timelineTargetStr, TIMELINE_LATEST_STR))
                timelineTarget = timelineLatest(storageRepo, archiveId, timelineCurrent);
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
                    THROW_FMT(FormatError, "invalid target timeline '%s'", strZ(timelineTargetStr));
                }
                TRY_END();
            }

            // Only proceed if target timeline is not the current timeline
            if (timelineTarget != timelineCurrent)
            {
                // Search through the history for the target timeline to make sure it includes the current timeline
                const List *const historyList = historyLoad(storageRepo, archiveId, timelineTarget);
                uint64_t timelineFound = 0;

                for (unsigned int historyIdx = 0; historyIdx < lstSize(historyList); historyIdx++)
                {
                    const HistoryItem *const historyItem = lstGet(historyList, historyIdx);

                    if (lsnCurrent < historyItem->lsn)
                    {
                        timelineFound = historyItem->timeline;

                        // !!!
                        if (timelineFound != timelineCurrent)
                        {
                            THROW_FMT(FormatError, "!!!WRONG FOUND");
                        }

                        break;
                    }
                }

                // !!!
                if (timelineFound == 0)
                    THROW_FMT(FormatError, "!!!NO FOUND");
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_LOG_RETURN_VOID();
}
