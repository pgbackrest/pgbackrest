/***********************************************************************************************************************************
Storage XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "storage/helper.h"

typedef Storage *pgBackRest__LibC__Storage;

/***********************************************************************************************************************************
Manifest callback
***********************************************************************************************************************************/
typedef struct StorageManifestXsCallbackData
{
    const String *path;
    String *json;
//    RegExp *filter;
} StorageManifestXsCallbackData;

void
storageManifestXsCallback(void *callbackData, const StorageInfo *info)
{
    StorageManifestXsCallbackData *data = (StorageManifestXsCallbackData *)callbackData;

    if (strSize(data->json) != 1)
        strCat(data->json, ",");

    strCatFmt(
        data->json, "%s:{\"group\":%s,\"user\":%s,\"mode\":\"%04o\",\"type\":\"", strPtr(jsonFromStr(info->name)),
        strPtr(jsonFromStr(info->group)), strPtr(jsonFromStr(info->user)), info->mode);

    switch (info->type)
    {
        case storageTypeFile:
        {
            strCatFmt(data->json, "d\",\"modification_time\":%" PRIu64 ",\"size\":%" PRIu64, info->timeModified, info->size);
            break;
        }

        case storageTypeLink:
        {
            strCatFmt(data->json, "l\",\"link_destination\":%s", strPtr(jsonFromStr(info->linkDestination)));
            break;
        }

        case storageTypePath:
        {
            strCat(data->json, "d\"");
            break;
        }
    }

    strCat(data->json, "}");
}
