/***********************************************************************************************************************************
Archive Segment Find
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/archive/common.h"
#include "command/archive/find.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/wait.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct WalSegmentFind
{
    const Storage *storage;                                         // Storage to find WAL in
    const String *archiveId;                                        // Archive id to find segments in
    bool single;                                                    // Optimize for a single segment?
    TimeMSec timeout;                                               // Timeout for each segment
    String *prefix;                                                 // Current list prefix
    StringList *list;                                               // List of found segments
};

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_WAL_SEGMENT_FIND_TYPE                                                                                         \
    WalSegmentFind *
#define FUNCTION_LOG_WAL_SEGMENT_FIND_FORMAT(value, buffer, bufferSize)                                                            \
    objNameToLog(value, "WalSegmentFind", buffer, bufferSize)

/**********************************************************************************************************************************/
FN_EXTERN WalSegmentFind *
walSegmentFindNew(const Storage *const storage, const String *const archiveId, const bool single, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(BOOL, single);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(archiveId != NULL);

    OBJ_NEW_BEGIN(WalSegmentFind, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (WalSegmentFind)
        {
            .storage = storage,
            .archiveId = strDup(archiveId),
            .single = single,
            .timeout = timeout,
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(WAL_SEGMENT_FIND, this);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
walSegmentFind(WalSegmentFind *const this, const String *const walSegment)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(WAL_SEGMENT_FIND, this);
        FUNCTION_LOG_PARAM(STRING, walSegment);
    FUNCTION_LOG_END();

    ASSERT(walSegment != NULL);
    ASSERT(walIsSegment(walSegment));

    String *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *const wait = waitNew(this->timeout);
        const String *const prefix = strSubN(walSegment, 0, 16);
        const String *const path = strNewFmt(STORAGE_REPO_ARCHIVE "/%s/%s", strZ(this->archiveId), strZ(prefix));
        const String *const expression = strNewFmt(
            "^%s%s-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$", strZ(strSubN(walSegment, 0, 24)),
            walIsPartial(walSegment) ? WAL_SEGMENT_PARTIAL_EXT : "");
        RegExp *regExp = NULL;

        do
        {
            // Get a list of all WAL segments that match the directory (and prefix when finding a single WAL)
            if (this->list == NULL || !strEq(prefix, this->prefix))
            {
                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    // Free and store prefix
                    if (!strEq(prefix, this->prefix))
                    {
                        strFree(this->prefix);
                        this->prefix = strDup(prefix);
                    }

                    // Free list
                    strLstFree(this->list);

                    // Get list
                    this->list = strLstSort(
                        storageListP(this->storage, path, .expression = this->single ? expression : NULL), sortOrderAsc);
                }
                MEM_CONTEXT_OBJ_END();
            }

            // If there are results
            if (!strLstEmpty(this->list))
            {
                // By default the match size is the list size since filtering happened above. When not finding a single WAL then
                // non-matching entries before the matching WAL will need to be removed and then the matching WAL counted.
                unsigned int match = strLstSize(this->list);

                if (!this->single)
                {
                    // Build regexp if not yet built
                    if (regExp == NULL)
                        regExp = regExpNew(expression);

                    // Remove list items that do not match. This prevents us from having check them again on the next find.
                    while (!strLstEmpty(this->list) && !regExpMatch(regExp, strLstGet(this->list, 0)))
                        strLstRemoveIdx(this->list, 0);

                    // Find matches at the beginning of the remaining list
                    match = 0;

                    while (match < strLstSize(this->list) && regExpMatch(regExp, strLstGet(this->list, match)))
                        match++;
                }

                // Error if there is more than one match
                if (match > 1)
                {
                    // Build list of duplicate WAL
                    StringList *const matchList = strLstNew();

                    for (unsigned int matchIdx = 0; matchIdx < match; matchIdx++)
                        strLstAdd(matchList, strLstGet(this->list, matchIdx));

                    // Clear list for next find
                    strLstFree(this->list);
                    this->list = NULL;

                    THROW_FMT(
                        ArchiveDuplicateError,
                        "duplicates found in archive for WAL segment %s: %s\n"
                        "HINT: are multiple primaries archiving to this stanza?",
                        strZ(walSegment), strZ(strLstJoin(matchList, ", ")));
                }

                // On match copy file name of WAL segment found into the prior context
                if (match == 1)
                {
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        result = strDup(strLstGet(this->list, 0));
                    }
                    MEM_CONTEXT_PRIOR_END();
                }

                // Remove matching entries so list will be reloaded when empty
                if (!this->single)
                {
                    ASSERT(regExp != NULL);

                    while (!strLstEmpty(this->list) && regExpMatch(regExp, strLstGet(this->list, 0)))
                        strLstRemoveIdx(this->list, 0);
                }
            }

            // Clear list for next find
            if (this->single || strLstEmpty(this->list))
            {
                strLstFree(this->list);
                this->list = NULL;
            }
        }
        while (result == NULL && waitMore(wait));
    }
    MEM_CONTEXT_TEMP_END();

    // Error if segment not found before timeout
    if (result == NULL && this->timeout != 0)
    {
        THROW_FMT(
            ArchiveTimeoutError,
            "WAL segment %s was not archived before the %" PRIu64 "ms timeout\n"
            "HINT: check the archive_command to ensure that all options are correct (especially --stanza).\n"
            "HINT: check the PostgreSQL server log for errors.\n"
            "HINT: run the 'start' command if the stanza was previously stopped.",
            strZ(walSegment), this->timeout);
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
FN_EXTERN String *
walSegmentFindOne(
    const Storage *const storage, const String *const archiveId, const String *const walSegment, const TimeMSec timeout)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, archiveId);
        FUNCTION_LOG_PARAM(STRING, walSegment);
        FUNCTION_LOG_PARAM(TIME_MSEC, timeout);
    FUNCTION_LOG_END();

    String *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        WalSegmentFind *const find = walSegmentFindNew(storage, archiveId, true, timeout);

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            result = walSegmentFind(find, walSegment);
        }
        MEM_CONTEXT_PRIOR_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}
