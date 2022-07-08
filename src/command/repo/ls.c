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
Render storage list
***********************************************************************************************************************************/
static void
storageListRenderInfo(const StorageInfo *const info, IoWrite *const write, const bool json)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
        FUNCTION_TEST_PARAM(IO_WRITE, write);
        FUNCTION_TEST_PARAM(BOOL, json);
    FUNCTION_TEST_END();

    ASSERT(info != NULL);
    ASSERT(write != NULL);

    // Render in json
    if (json)
    {
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
        ioWrite(write, BUFSTR(info->name));
        ioWrite(write, LF_BUF);
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageListRender(IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

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
    bool json = cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON ? true : false;
    const String *expression = cfgOptionStrNull(cfgOptFilter);
    RegExp *regExp = expression == NULL ? NULL : regExpNew(expression);

    ioWriteOpen(write);

    if (json)
        ioWrite(write, BRACEL_BUF);

    // Check if this is a file
    StorageInfo info = storageInfoP(storageRepo(), path, .ignoreMissing = true);

    if (info.exists && info.type == storageTypeFile)
    {
        if (regExp == NULL || regExpMatch(regExp, storagePathP(storageRepo(), path)))
        {
            info.name = DOT_STR;
            storageListRenderInfo(&info, write, json);
        }
    }
    // Else try to list the path
    else
    {
        // The path will always be reported as existing so we don't get different results from storage that does not support paths
        bool first = true;

        if (json && (regExp == NULL || regExpMatch(regExp, DOT_STR)))
        {
            storageListRenderInfo(&(StorageInfo){.type = storageTypePath, .name = DOT_STR}, write, json);
            first = false;
        }

        // List content of the path
        StorageIterator *const storageItr = storageNewItrP(
            storageRepo(), path, .sortOrder = sortOrder, .expression = expression, .recurse = cfgOptionBool(cfgOptRecurse));

        while (storageItrMore(storageItr))
        {
            const StorageInfo info = storageItrNext(storageItr);

            // Add separator character
            if (!first && json)
                ioWrite(write, COMMA_BUF);
            else
                first = false;

            storageListRenderInfo(&info, write, json);
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
void
cmdStorageList(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        TRY_BEGIN()
        {
            storageListRender(ioFdWriteNew(STRDEF("stdout"), STDOUT_FILENO, cfgOptionUInt64(cfgOptIoTimeout)));
        }
        // Ignore write errors because it's possible (even likely) that this output is being piped to something like head which
        // will exit when it gets what it needs and leave us writing to a broken pipe.  It would be better to just ignore the broken
        // pipe error but currently we don't store system error codes.
        CATCH(FileWriteError)
        {
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
