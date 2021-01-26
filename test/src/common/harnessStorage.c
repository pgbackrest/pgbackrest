/***********************************************************************************************************************************
Storage Test Harness
***********************************************************************************************************************************/
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/type/object.h"
#include "common/type/param.h"
#include "common/user.h"
#include "storage/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessStorage.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Dummy functions and interface for constructing test storage drivers
***********************************************************************************************************************************/
static StorageInfo
storageTestDummyInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    (void)thisVoid; (void)file; (void)level; (void)param; return (StorageInfo){.exists = false};
}

static bool
storageTestDummyInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    (void)thisVoid; (void)path; (void)level; (void)callback; (void)callbackData; (void)param; return false;
}

static StorageRead *
storageTestDummyNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    (void)thisVoid; (void)file; (void)ignoreMissing; (void)param; return NULL;
}

static StorageWrite *
storageTestDummyNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    (void)thisVoid; (void)file; (void)param; return NULL;
}

static bool
storageTestDummyPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    (void)thisVoid; (void)path; (void)recurse; (void)param; return false;
}

static void
storageTestDummyRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    (void)thisVoid; (void)file; (void)param;
}

const StorageInterface storageInterfaceTestDummy =
{
    .info = storageTestDummyInfo,
    .infoList = storageTestDummyInfoList,
    .newRead = storageTestDummyNewRead,
    .newWrite = storageTestDummyNewWrite,
    .pathRemove = storageTestDummyPathRemove,
    .remove = storageTestDummyRemove,
};

/**********************************************************************************************************************************/
void
hrnStorageInfoListCallback(void *callbackData, const StorageInfo *info)
{
    HarnessStorageInfoListCallbackData *data = callbackData;

    if (data->rootPathOmit && info->type == storageTypePath && strEq(info->name, DOT_STR))
        return;

    strCatFmt(data->content, "%s {", info->name == NULL ? NULL_Z : strZ(info->name));

    if (info->level > storageInfoLevelExists)
    {
        switch (info->type)
        {
            case storageTypeFile:
            {
                strCatZ(data->content, "file");

                if (info->level >= storageInfoLevelBasic && !data->sizeOmit)
                {
                    uint64_t size = info->size;

                    // If the file is compressed then decompress to get the real size.  Note that only gz is used in unit tests since
                    // it is the only compression type guaranteed to be present.
                    if (data->fileCompressed)
                    {
                        ASSERT(data->storage != NULL);

                        StorageRead *read = storageNewReadP(
                            data->storage,
                            data->path != NULL ? strNewFmt("%s/%s", strZ(data->path), strZ(info->name)) : info->name);
                        ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), decompressFilter(compressTypeGz));
                        size = bufUsed(storageGetP(read));
                    }

                    strCatFmt(data->content, ", s=%" PRIu64, size);
                }

                break;
            }

            case storageTypeLink:
                strCatFmt(data->content, "link, d=%s", strZ(info->linkDestination));
                break;

            case storageTypePath:
                strCatZ(data->content, "path");
                break;

            case storageTypeSpecial:
                strCatZ(data->content, "special");
                break;
        }

        if (info->type != storageTypeSpecial)
        {
            if (info->type != storageTypeLink)
            {
                if (info->level >= storageInfoLevelDetail &&
                    (!data->modeOmit || (info->type == storageTypePath && data->modePath != info->mode) ||
                    (info->type == storageTypeFile && data->modeFile != info->mode)))
                {
                    strCatFmt(data->content, ", m=%04o", info->mode);
                }
            }

            if (info->type == storageTypeFile && info->level >= storageInfoLevelBasic)
            {
                if (!data->timestampOmit)
                    strCatFmt(data->content, ", t=%" PRIu64, (uint64_t)info->timeModified);
            }

            if (info->level >= storageInfoLevelDetail && (!data->userOmit || userId() != info->userId))
            {
                if (info->user != NULL)
                {
                    strCatFmt(data->content, ", u=%s", strZ(info->user));
                }
                else
                {
                    strCatFmt(data->content, ", u=%d", (int)info->userId);
                }
            }

            if (info->level >= storageInfoLevelDetail && (!data->groupOmit || groupId() != info->groupId))
            {
                if (info->group != NULL)
                {
                    strCatFmt(data->content, ", g=%s", strZ(info->group));
                }
                else
                {
                    strCatFmt(data->content, ", g=%d", (int)info->groupId);
                }
            }
        }
    }

    strCatZ(data->content, "}\n");
}

/**********************************************************************************************************************************/
StringList *
hrnStorageList(const Storage *storage, const char *path, HrnStorageListParam param)
{
    StringList *list = strLstSort(storageListP(storage, STR(path)), sortOrderAsc);

    // Remove files if requested
    if (param.remove)
    {
        for (unsigned int listIdx = 0; listIdx < strLstSize(list); listIdx++)
            storageRemoveP(storage, strNewFmt("%s/%s", path, strZ(strLstGet(list, listIdx))), .errorOnMissing = true);
    }

    // Return list for comparison
    return list;
}

const char *
hrnStorageListLog(const Storage *storage, const char *path, HrnStorageListParam param)
{
    StringList *list = strLstSort(storageListP(storage, STR(path)), sortOrderAsc);

    return strZ(
        strNewFmt(
            "list%s %u file%s in '%s'", param.remove ? "/remove": "", strLstSize(list), strLstSize(list) == 1 ? "" : "s",
            strZ(storagePathP(storage, STR(path)))));
}

/**********************************************************************************************************************************/
void
hrnStoragePut(const Storage *storage, const char *file, const Buffer *buffer, HrnStoragePutParam param)
{
    // Add compression extension to file name
    String *fileStr = strNew(file);
    compressExtCat(fileStr, param.compressType);

    // Create file
    StorageWrite *destination = storageNewWriteP(storage, fileStr);
    IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(destination));

    // Add compression filter
    if (param.compressType != compressTypeNone)
    {
        ASSERT(param.compressType == compressTypeGz || param.compressType == compressTypeBz2);
        ioFilterGroupAdd(filterGroup, compressFilter(param.compressType, 1));
    }

    // Add encrypted filter
    if (param.cipherType != cipherTypeNone)
    {
        // Default to main cipher pass
        if (param.cipherPass == NULL)
            param.cipherPass = TEST_CIPHER_PASS;

        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, param.cipherType, BUFSTRZ(param.cipherPass), NULL));
    }

    // Put file
    storagePutP(destination, buffer);
}

const char *
hrnStoragePutLog(const Storage *storage, const char *file, const Buffer *buffer, HrnStoragePutParam param)
{
    // Empty if buffer is NULL
    String *log = strNew(buffer == NULL || bufUsed(buffer) == 0 ? "(empty) " : "");

    // Add compression detail
    if (param.compressType != compressTypeNone)
        strCatFmt(log, "cmp[%s]", strZ(compressTypeStr(param.compressType)));

    // Add encryption detail
    if (param.cipherType != cipherTypeNone)
    {
        if (param.cipherPass == NULL)
            param.cipherPass = TEST_CIPHER_PASS;

        if (param.compressType != compressTypeNone)
            strCatZ(log, "/");

        strCatFmt(log, "enc[%s,%s]", strZ(cipherTypeName(param.cipherType)), param.cipherPass);
    }

    // Add a space if compression/encryption defined
    if (param.compressType != compressTypeNone || param.cipherType != cipherTypeNone)
        strCatZ(log, " ");

    // Add file name
    strCatFmt(log, "'%s%s'", strZ(storagePathP(storage, STR(file))), strZ(compressExtStr(param.compressType)));

    return strZ(log);
}
