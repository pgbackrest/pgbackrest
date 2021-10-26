/***********************************************************************************************************************************
Repository List Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

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
typedef struct StorageListRenderCallbackData
{
    IoWrite *write;                                                 // Where to write output
    bool json;                                                      // Is this json output?
    bool first;                                                     // Is this the first item?
} StorageListRenderCallbackData;

void
storageListRenderCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM_P(VOID, data);
        FUNCTION_TEST_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(data != NULL);
    ASSERT(info != NULL);

    StorageListRenderCallbackData *listData = (StorageListRenderCallbackData *)data;

    // Skip . path if it is not first when json output
    if (info->type == storageTypePath && strEq(info->name, DOT_STR) && (!listData->first || !listData->json))
    {
        FUNCTION_TEST_RETURN_VOID();
        return;
    }

    // Add separator character
    if (!listData->first && listData->json)
        ioWrite(listData->write, COMMA_BUF);
    else
        listData->first = false;

    // Render in json
    if (listData->json)
    {
        ioWriteStr(listData->write, jsonFromStr(info->name));
        ioWrite(listData->write, BUFSTRDEF(":{\"type\":\""));

        switch (info->type)
        {
            case storageTypeFile:
                ioWrite(listData->write, BUFSTRDEF("file\""));
                break;

            case storageTypeLink:
                ioWrite(listData->write, BUFSTRDEF("link\""));
                break;

            case storageTypePath:
                ioWrite(listData->write, BUFSTRDEF("path\""));
                break;

            case storageTypeSpecial:
                ioWrite(listData->write, BUFSTRDEF("special\""));
                break;
        }

        if (info->type == storageTypeFile)
        {
            ioWriteStr(listData->write, strNewFmt(",\"size\":%" PRIu64, info->size));
            ioWriteStr(listData->write, strNewFmt(",\"time\":%" PRId64, (int64_t)info->timeModified));
        }

        if (info->type == storageTypeLink)
            ioWriteStr(listData->write, strNewFmt(",\"destination\":%s", strZ(jsonFromStr(info->linkDestination))));

        ioWrite(listData->write, BRACER_BUF);
    }
    // Render in text
    else
    {
        ioWrite(listData->write, BUFSTR(info->name));
        ioWrite(listData->write, LF_BUF);
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

    if (strLstSize(cfgCommandParam()) == 1)
        path = strLstGet(cfgCommandParam(), 0);
    else if (strLstSize(cfgCommandParam()) > 1)
        THROW(ParamInvalidError, "only one path may be specified");

    // Get options
    bool json = cfgOptionStrId(cfgOptOutput) == CFGOPTVAL_OUTPUT_JSON ? true : false;
    const String *expression = cfgOptionStrNull(cfgOptFilter);
    RegExp *regExp = expression == NULL ? NULL : regExpNew(expression);

    // Render the info list
    StorageListRenderCallbackData data =
    {
        .write = write,
        .json = json,
        .first = true,
    };

    ioWriteOpen(data.write);

    if (data.json)
        ioWrite(data.write, BRACEL_BUF);

    // Check if this is a file
    StorageInfo info = storageInfoP(storageRepo(), path, .ignoreMissing = true);

    if (info.exists && info.type == storageTypeFile)
    {
        if (regExp == NULL || regExpMatch(regExp, storagePathP(storageRepo(), path)))
        {
            info.name = DOT_STR;
            storageListRenderCallback(&data, &info);
        }
    }
    // Else try to list the path
    else
    {
        // The path will always be reported as existing so we don't get different results from storage that does not support paths
        if (data.json && (regExp == NULL || regExpMatch(regExp, DOT_STR)))
            storageListRenderCallback(&data, &(StorageInfo){.type = storageTypePath, .name = DOT_STR});

        // List content of the path
        storageInfoListP(
            storageRepo(), path, storageListRenderCallback, &data, .sortOrder = sortOrder, .expression = expression,
            .recurse = cfgOptionBool(cfgOptRecurse));
    }

    if (data.json)
    {
        ioWrite(data.write, BRACER_BUF);
        ioWrite(data.write, LF_BUF);
    }

    ioWriteClose(data.write);

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
