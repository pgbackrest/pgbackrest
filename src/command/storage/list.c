/***********************************************************************************************************************************
Storage List Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "common/debug.h"
#include "common/io/handleWrite.h"
#include "common/memContext.h"
#include "common/log.h"
#include "common/memContext.h"
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

    if (listData->json)
    {
        ioWrite(listData->write, BUFSTRDEF("{\"name\":"));
        ioWriteStr(listData->write, jsonFromStr(info->name));
        ioWrite(listData->write, BUFSTRDEF(",\"type\":\""));

        switch (info->type)
        {
            case storageTypeFile:
            {
                ioWrite(listData->write, BUFSTRDEF("file\""));
                break;
            }

            case storageTypeLink:
            {
                ioWrite(listData->write, BUFSTRDEF("link\""));
                break;
            }

            case storageTypePath:
            {
                ioWrite(listData->write, BUFSTRDEF("path\""));
                break;
            }

            case storageTypeSpecial:
            {
                ioWrite(listData->write, BUFSTRDEF("special\""));
                break;
            }
        }

        if (info->type == storageTypeFile)
            ioWriteStr(listData->write, strNewFmt(",\"size\":%" PRIu64, info->size));

        if (info->mode != 0)
            ioWriteStr(listData->write, strNewFmt(",\"mode\":%04o", info->mode));

        if (info->user != NULL)
        {
            ioWrite(listData->write, BUFSTRDEF(",\"user\":"));
            ioWriteStr(listData->write, jsonFromStr(info->user));
        }

        if (info->group != NULL)
        {
            ioWrite(listData->write, BUFSTRDEF(",\"group\":"));
            ioWriteStr(listData->write, jsonFromStr(info->group));
        }

        ioWrite(listData->write, BRACER_BUF);
    }
    else
    {
        ioWriteLine(listData->write, BUFSTR(info->name));
    }

    FUNCTION_TEST_RETURN_VOID();
}

static void
storageListRenderParam(IoWrite *write, const String *path, bool json, SortOrder sortOrder, const String *filter)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_WRITE, write);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(BOOL, json);
        FUNCTION_LOG_PARAM(ENUM, sortOrder);
        FUNCTION_LOG_PARAM(STRING, filter);
    FUNCTION_LOG_END();

    (void)filter;

    // Get the list
    StorageListRenderCallbackData data =
    {
        .write = write,
        .json = json,
    };

    ioWriteOpen(data.write);

    if (data.json)
        ioWrite(data.write, BRACKETL_BUF);

    storageInfoListP(storageRepo(), path, storageListRenderCallback, &data, .sortOrder = sortOrder);

    if (data.json)
        ioWrite(data.write, BRACKETR_BUF);

    ioWriteClose(data.write);

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Call render with parameters passed by the user
***********************************************************************************************************************************/
static void
storageListRender(IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug)
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    storageListRenderParam(
        write, strLstSize(cfgCommandParam()) == 1 ? strLstGet(cfgCommandParam(), 0) : NULL,
        strEqZ(cfgOptionStr(cfgOptOutput), "json") ? true : false,
        strEqZ(cfgOptionStr(cfgOptSort), "asc") ? sortOrderAsc : sortOrderDesc, cfgOptionStr(cfgOptFilter));

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Render storage list and output to stdout
***********************************************************************************************************************************/
void
cmdStorageList(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        storageListRender(ioHandleWriteNew(STRDEF("stdout"), STDOUT_FILENO));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
