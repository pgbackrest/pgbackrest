/***********************************************************************************************************************************
Storage Test Harness
***********************************************************************************************************************************/
#include "storage/storage.h"

#include "common/harnessStorage.h"

/**********************************************************************************************************************************/
void
hrnStorageInfoListCallback(void *callbackData, const StorageInfo *info)
{
    HarnessStorageInfoListCallbackData *data = callbackData;

    // Add LF for after the first item
    if (strSize(data->content) != 0)
        strCat(data->content, "\n");

    strCatFmt(data->content, "%s {", strPtr(info->name));

    switch (info->type)
    {
        case storageTypeFile:
        {
            strCat(data->content, "file");
            break;
        }

        case storageTypeLink:
        {
            strCat(data->content, "link");
            break;
        }

        case storageTypePath:
        {
            strCat(data->content, "path");
            break;
        }

        case storageTypeSpecial:
        {
            strCat(data->content, "special");
            break;
        }
    }

    if (!data->modeOmit)
    {
        strCatFmt(data->content, ", %04o", info->mode);
    }
}
