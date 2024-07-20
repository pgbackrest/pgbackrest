/***********************************************************************************************************************************
Repository List Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/repo/common.h"
#include "common/debug.h"
#include "common/io/fdWrite.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/regExp.h"
#include "common/type/json.h"
#include "common/type/string.h"
#include "config/config.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
!!! COPIED FROM RESTORE. NEED TO MOVE TO TIME.C
***********************************************************************************************************************************/
static String *
storageListDateTime(const time_t epoch)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(TIME, epoch);
    FUNCTION_TEST_END();

    // Construct date/time
    String *const result = strNew();
    struct tm timePart;
    char timeBuffer[20];

    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime_r(&epoch, &timePart));
    strCatZ(result, timeBuffer);

    // Add timezone offset. Since this is not directly available in Posix-compliant APIs, use the difference between gmtime_r() and
    // localtime_r to determine the offset. Handle minute offsets when present.
    struct tm timePartGm;

    gmtime_r(&epoch, &timePartGm);

    timePart.tm_isdst = -1;
    timePartGm.tm_isdst = -1;
    time_t offset = mktime(&timePart) - mktime(&timePartGm);

    if (offset >= 0)
        strCatChr(result, '+');
    else
    {
        offset *= -1;
        strCatChr(result, '-');
    }

    const unsigned int minute = (unsigned int)(offset / 60);
    const unsigned int hour = minute / 60;

    strCatFmt(result, "%02u", hour);

    if (minute % 60 != 0)
        strCatFmt(result, ":%02u", minute - (hour * 60));

    FUNCTION_TEST_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Render storage list
***********************************************************************************************************************************/
static void
storageListRenderInfo(
    const StorageInfo *const info, IoWrite *const write, const bool json, const bool version, const bool versionFirst)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
        FUNCTION_TEST_PARAM(IO_WRITE, write);
        FUNCTION_TEST_PARAM(BOOL, json);
        FUNCTION_TEST_PARAM(BOOL, version);
        FUNCTION_TEST_PARAM(BOOL, versionFirst);
    FUNCTION_TEST_END();

    FUNCTION_AUDIT_HELPER();

    ASSERT(info != NULL);
    ASSERT(write != NULL);

    // Render in json
    if (json)
    {
        (void)versionFirst; // !!! JSON OUTPUT BROKEN FOR VERSIONS
        ioWriteStr(write, jsonFromVar(VARSTR(info->name)));
        ioWrite(write, BUFSTRDEF(":{\"type\":\""));

        switch (info->type)
        {
            case storageTypeFile:
                ioWrite(write, BUFSTRDEF("file\""));
                break;

            case storageTypeLink:
                ioWrite(write, BUFSTRDEF("link\""));
                break;

            case storageTypePath:
                ioWrite(write, BUFSTRDEF("path\""));
                break;

            case storageTypeSpecial:
                ioWrite(write, BUFSTRDEF("special\""));
                break;
        }

        if (info->type == storageTypeFile)
        {
            ioWriteStr(write, strNewFmt(",\"size\":%" PRIu64, info->size));
            ioWriteStr(write, strNewFmt(",\"time\":%" PRId64, (int64_t)info->timeModified));
        }

        if (info->type == storageTypeLink)
            ioWriteStr(write, strNewFmt(",\"destination\":%s", strZ(jsonFromVar(VARSTR(info->linkDestination)))));

        ioWrite(write, BRACER_BUF);
    }
    // Render in text
    else
    {
        if (version)
        {
            if (info->type == storageTypeFile)
                ioWrite(write, BUFSTR(storageListDateTime(info->timeModified)));
            else
                ioWrite(write, BUFSTRDEF("                      "));

            if (info->type == storageTypeFile && !info->deleteMarker)
                ioWrite(write, BUFSTR(strNewFmt(" %10" PRIu64, info->size)));
            else
                ioWrite(write, BUFSTRDEF("           "));

            if (info->deleteMarker)
                ioWrite(write, BUFSTRDEF(" D "));
            else
                ioWrite(write, BUFSTRDEF("   "));
        }

        ioWrite(write, BUFSTR(info->name));
        ioWrite(write, LF_BUF);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageListRender(IoWrite *const write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    FUNCTION_AUDIT_HELPER();

    // Get sort order
    SortOrder sortOrder = sortOrderAsc;

    switch (cfgOptionStrId(cfgOptSort))
    {
        case CFGOPTVAL_SORT_DESC:
            sortOrder = sortOrderDesc;
            break;

        case CFGOPTVAL_SORT_NONE:
            sortOrder = sortOrderNone;
            break;

        default:
            ASSERT(cfgOptionStrId(cfgOptSort) == CFGOPTVAL_SORT_ASC);
    }

    // Get path
    const String *path = NULL;

    // Get and validate if path is valid for repo
    if (strLstSize(cfgCommandParam()) == 1)
        path = repoPathIsValid(strLstGet(cfgCommandParam(), 0));
    else if (strLstSize(cfgCommandParam()) > 1)
        THROW(ParamInvalidError, "only one path may be specified");

    // Get options
    const bool json = cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON ? true : false;
    const String *const expression = cfgOptionStrNull(cfgOptFilter);
    RegExp *const regExp = expression == NULL ? NULL : regExpNew(expression);
    const bool versions = cfgOptionBool(cfgOptVers);

    ioWriteOpen(write);

    if (json)
        ioWrite(write, BRACEL_BUF);

    // Check if this is a file
    StorageInfo info = storageInfoP(storageRepo(), path, .ignoreMissing = true);

    if (!versions && info.exists && info.type == storageTypeFile)
    {
        if (regExp == NULL || regExpMatch(regExp, storagePathP(storageRepo(), path)))
        {
            info.name = DOT_STR;
            storageListRenderInfo(&info, write, json, false, false);
        }
    }
    // Else try to list the path
    else
    {
        // The path will always be reported as existing so we don't get different results from storage that does not support paths
        bool first = true;

        if (json && (regExp == NULL || regExpMatch(regExp, DOT_STR)))
        {
            storageListRenderInfo(&(StorageInfo){.type = storageTypePath, .name = DOT_STR}, write, json, false, false);
            first = false;
        }

        // Output header !!! NEED THIS?
        // if (versions && !json)
        // {
        //     ioWrite(write, BUFSTRDEF("      Timestamp           Size    D                    Name\n"));
        //     ioWrite(write, BUFSTRDEF("---------------------- ---------- - --------------------------------------------\n"));
        // }

        // List content of the path
        String *const infoNameLast = strNew();

        StorageIterator *const storageItr = storageNewItrP(
            storageRepo(), path, .sortOrder = sortOrder, .expression = expression, .recurse = cfgOptionBool(cfgOptRecurse),
            .versions = versions);

        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Add separator character
            if (!first && json)
                ioWrite(write, COMMA_BUF);
            else
                first = false;

            storageListRenderInfo(&info, write, json, versions, !strEq(info.name, infoNameLast));

            strTrunc(infoNameLast);
            strCat(infoNameLast, info.name);
        }
    }

    if (json)
    {
        ioWrite(write, BRACER_BUF);
        ioWrite(write, LF_BUF);
    }

    ioWriteClose(write);

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStorageList(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            storageListRender(ioFdWriteNew(STRDEF("stdout"), STDOUT_FILENO, cfgOptionUInt64(cfgOptIoTimeout)));
        }
        // Ignore write errors because it's possible (even likely) that this output is being piped to something like head which will
        // exit when it gets what it needs and leave us writing to a broken pipe. It would be better to just ignore the broken pipe
        // error but currently we don't store system error codes.
        CATCH(FileWriteError)
        {
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
