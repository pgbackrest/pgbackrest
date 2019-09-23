/***********************************************************************************************************************************
Storage Test Harness
***********************************************************************************************************************************/
#include <inttypes.h>

#include "common/user.h"
#include "storage/storage.h"

#include "common/harnessStorage.h"

/**********************************************************************************************************************************/
void
hrnStorageInfoListCallback(void *callbackData, const StorageInfo *info)
{
    HarnessStorageInfoListCallbackData *data = callbackData;

    strCatFmt(data->content, "%s {", strPtr(info->name));

    switch (info->type)
    {
        case storageTypeFile:
        {
            strCat(data->content, "file");

            if (!data->sizeOmit)
                strCatFmt(data->content, ", s=%" PRIu64, info->size);

            break;
        }

        case storageTypeLink:
        {
            strCatFmt(data->content, "link, d=%s", strPtr(info->linkDestination));
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

    if (info->type != storageTypeSpecial)
    {
        if (info->type != storageTypeLink)
        {
            if (!data->modeOmit || (info->type == storageTypePath && data->modePath != info->mode) ||
                (info->type == storageTypeFile && data->modeFile != info->mode))
            {
                strCatFmt(data->content, ", m=%04o", info->mode);
            }
        }

        if (info->type == storageTypeFile)
        {
            if (!data->timestampOmit)
                strCatFmt(data->content, ", t=%" PRIu64, (uint64_t)info->timeModified);
        }

        if (!data->userOmit || userId() != info->userId)
        {
            if (info->user != NULL)
            {
                strCatFmt(data->content, ", u=%s", strPtr(info->user));
            }
            else
            {
                strCatFmt(data->content, ", u=%d", (int)info->userId);
            }
        }

        if (!data->groupOmit || groupId() != info->groupId)
        {
            if (info->group != NULL)
            {
                strCatFmt(data->content, ", g=%s", strPtr(info->group));
            }
            else
            {
                strCatFmt(data->content, ", g=%d", (int)info->groupId);
            }
        }
    }

    strCat(data->content, "}\n");
}
