/***********************************************************************************************************************************
Storage XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/compress/gz/compress.h"
#include "common/compress/gz/decompress.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/filter/size.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "postgres/interface.h"
#include "storage/helper.h"
#include "storage/s3/storage.intern.h"
#include "storage/storage.intern.h"

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
    const String *filter;
} StorageManifestXsCallbackData;

String *
storageManifestXsInfo(const String *path, const StorageInfo *info)
{
    String *json = strNew("");

    if (info->name != NULL)
    {
        strCatFmt(
            json, "%s:", strPtr(jsonFromStr(path == NULL ? info->name : strNewFmt("%s/%s", strPtr(path), strPtr(info->name)))));
    }

    strCatFmt(json, "{\"group\":%s,\"user\":%s,\"type\":\"", strPtr(jsonFromStr(info->group)), strPtr(jsonFromStr(info->user)));

    switch (info->type)
    {
        case storageTypeFile:
        {
            strCatFmt(
                json, "f\",\"mode\":\"%04o\",\"modification_time\":%" PRId64 ",\"size\":%" PRIu64 "}", info->mode,
                (int64_t)info->timeModified, info->size);
            break;
        }

        case storageTypeLink:
        {
            strCatFmt(json, "l\",\"link_destination\":%s}", strPtr(jsonFromStr(info->linkDestination)));
            break;
        }

        case storageTypePath:
        {
            strCatFmt(json, "d\",\"mode\":\"%04o\"}", info->mode);
            break;
        }

        case storageTypeSpecial:
        {
            strCatFmt(json, "s\",\"mode\":\"%04o\"}", info->mode);
            break;
        }
    }

    return json;
}

void
storageManifestXsCallback(void *callbackData, const StorageInfo *info)
{
    StorageManifestXsCallbackData *data = (StorageManifestXsCallbackData *)callbackData;

    if (data->path == NULL || !strEqZ(info->name, "."))
    {
        if (!strEqZ(info->name, ".") && data->filter && !strEq(data->filter, info->name))
            return;

        if (strSize(data->json) != 1)
            strCat(data->json, ",");

        strCat(data->json, strPtr(storageManifestXsInfo(data->path, info)));

        if (info->type == storageTypePath)
        {
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
                    dataSub.storage, strNewFmt("%s/%s", strPtr(dataSub.pathRoot), strPtr(dataSub.path)), storageManifestXsCallback,
                    &dataSub);
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
    VariantList *paramList = paramJson ? jsonToVarLst(paramJson) : NULL;

    if (strEqZ(filter, "pgBackRest::Storage::Filter::CipherBlock"))
    {
        ioFilterGroupAdd(
            filterGroup,
            cipherBlockNew(
                varUInt64Force(varLstGet(paramList, 0)) ? cipherModeEncrypt : cipherModeDecrypt,
                cipherType(varStr(varLstGet(paramList, 1))), BUFSTR(varStr(varLstGet(paramList, 2))), NULL));
    }
    else
        THROW_FMT(AssertError, "unable to add invalid filter '%s'", strPtr(filter));
}
