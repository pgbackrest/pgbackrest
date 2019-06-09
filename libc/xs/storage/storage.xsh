/***********************************************************************************************************************************
Storage XS Header
***********************************************************************************************************************************/
#include "command/backup/pageChecksum.h"
#include "common/assert.h"
#include "common/compress/gzip/compress.h"
#include "common/compress/gzip/decompress.h"
#include "common/io/filter/size.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "postgres/interface.h"
#include "storage/helper.h"
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
    String *json = strNewFmt(
        "%s:{\"group\":%s,\"user\":%s,\"type\":\"",
        strPtr(jsonFromStr(path == NULL ? info->name : strNewFmt("%s/%s", strPtr(path), strPtr(info->name)))),
        strPtr(jsonFromStr(info->group)), strPtr(jsonFromStr(info->user)));

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

                storageInfoListNP(
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
    else if (strEqZ(filter, "pgBackRest::Storage::Filter::Sha"))
    {
        ioFilterGroupAdd(filterGroup, cryptoHashNew(HASH_TYPE_SHA1_STR));
    }
    else if (strEqZ(filter, "pgBackRest::Common::Io::Handle"))
    {
        ioFilterGroupAdd(filterGroup, ioSizeNew());
    }
    else if (strEqZ(filter, "pgBackRest::Backup::Filter::PageChecksum"))
    {
        ioFilterGroupAdd(
            filterGroup,
            pageChecksumNew(
                varUInt64Force(varLstGet(paramList, 0)), PG_SEGMENT_PAGE_DEFAULT, PG_PAGE_SIZE_DEFAULT,
                (uint64_t)varUIntForce(varLstGet(paramList, 1)) << 32 | varUIntForce(varLstGet(paramList, 2))));
    }
    else if (strEqZ(filter, "pgBackRest::Storage::Filter::Gzip"))
    {
        if (strEqZ(varStr(varLstGet(paramList, 0)), "compress"))
        {
            ioFilterGroupAdd(
                filterGroup, gzipCompressNew(varUIntForce(varLstGet(paramList, 2)), varBoolForce(varLstGet(paramList, 1))));
        }
        else
        {
            ioFilterGroupAdd(filterGroup, gzipDecompressNew(varBoolForce(varLstGet(paramList, 1))));
        }
    }
    else
        THROW_FMT(AssertError, "unable to add invalid filter '%s'", strPtr(filter));
}

/***********************************************************************************************************************************
Get result from IO filter
***********************************************************************************************************************************/
String *
storageFilterXsResult(const IoFilterGroup *filterGroup, const String *filter)
{
    const Variant *result;

    if (strEqZ(filter, "pgBackRest::Storage::Filter::Sha"))
    {
        result = ioFilterGroupResult(filterGroup, CRYPTO_HASH_FILTER_TYPE_STR);
    }
    else if (strEqZ(filter, "pgBackRest::Common::Io::Handle"))
    {
        result = ioFilterGroupResult(filterGroup, SIZE_FILTER_TYPE_STR);
    }
    else if (strEqZ(filter, "pgBackRest::Backup::Filter::PageChecksum"))
    {
        result = ioFilterGroupResult(filterGroup, PAGE_CHECKSUM_FILTER_TYPE_STR);
    }
    else
        THROW_FMT(AssertError, "unable to get result for invalid filter '%s'", strPtr(filter));

    if (result == NULL)
        THROW_FMT(AssertError, "unable to find result for filter '%s'", strPtr(filter));

    return jsonFromVar(result, 0);
}
