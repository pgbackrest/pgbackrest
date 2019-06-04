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
    const Storage *storage;
    const String *pathRoot;
    const String *path;
    String *json;
//    RegExp *filter;
} StorageManifestXsCallbackData;

void
storageManifestXsCallback(void *callbackData, const StorageInfo *info)
{
    StorageManifestXsCallbackData *data = (StorageManifestXsCallbackData *)callbackData;

    if (data->path == NULL || !strEqZ(info->name, "."))
    {
        if (strSize(data->json) != 1)
            strCat(data->json, ",");

        strCatFmt(
            data->json, "%s:{\"group\":%s,\"user\":%s,\"type\":\"",
            strPtr(jsonFromStr(data->path == NULL ? info->name : strNewFmt("%s/%s", strPtr(data->path), strPtr(info->name)))),
            strPtr(jsonFromStr(info->group)), strPtr(jsonFromStr(info->user)));

        switch (info->type)
        {
            case storageTypeFile:
            {
                strCatFmt(
                    data->json, "f\",\"mode\":\"%04o\",\"modification_time\":%" PRIu64 ",\"size\":%" PRIu64 "}", info->mode,
                    info->timeModified, info->size);
                break;
            }

            case storageTypeLink:
            {
                strCatFmt(data->json, "l\",\"link_destination\":%s}", strPtr(jsonFromStr(info->linkDestination)));
                break;
            }

            case storageTypePath:
            {
                strCatFmt(data->json, "d\",\"mode\":\"%04o\"}", info->mode);

                if (!strEqZ(info->name, "."))
                {
                    StorageManifestXsCallbackData dataSub =
                    {
                        .storage = data->storage,
                        .json = data->json,
                        .pathRoot = data->pathRoot,
                        .path = data->path == NULL ? info->name : strNewFmt("%s/%s", strPtr(data->path), strPtr(info->name)),
                    };

                    storageInfoListP(
                        dataSub.storage, strNewFmt("%s/%s", strPtr(dataSub.pathRoot), strPtr(dataSub.path)),
                        storageManifestXsCallback, &dataSub);
                }

                break;
            }
        }
    }
}

/***********************************************************************************************************************************
Add IO filter
***********************************************************************************************************************************/
void
storageFilterXsAdd(IoFilterGroup *filterGroup, const String *filter, const String *paramJson)
{
    VariantList *paramList = jsonToVarLst(paramJson);

    if (strEqZ(filter, "pgBackRest::Storage::Filter::CipherBlock"))
    {
        ioFilterGroupAdd(
            filterGroup,
            cipherBlockNew(
                varUInt64(varLstGet(paramList, 0)) ? cipherModeEncrypt : cipherModeDecrypt,
                cipherType(varStr(varLstGet(paramList, 1))), BUFSTR(varStr(varLstGet(paramList, 2))), NULL));
    }
    else
        THROW_FMT(AssertError, "unknown filter '%s'", strPtr(filter));
}
