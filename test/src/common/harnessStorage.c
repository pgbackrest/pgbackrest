/***********************************************************************************************************************************
Storage Test Harness
***********************************************************************************************************************************/
#include <inttypes.h>

#include "common/debug.h"
#include "common/compress/helper.h"
#include "common/type/object.h"
#include "common/user.h"
#include "storage/storage.h"

#include "common/harnessStorage.h"

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

    strCatFmt(data->content, "%s {", strZ(info->name));

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
            {
                strCatFmt(data->content, "link, d=%s", strZ(info->linkDestination));
                break;
            }

            case storageTypePath:
            {
                strCatZ(data->content, "path");
                break;
            }

            case storageTypeSpecial:
            {
                strCatZ(data->content, "special");
                break;
            }
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
