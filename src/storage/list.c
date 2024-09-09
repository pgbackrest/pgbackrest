/***********************************************************************************************************************************
Storage List
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/type/blob.h"
#include "common/type/list.h"
#include "storage/list.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageList
{
    StorageListPub pub;                                             // Publicly accessible variables
    StringList *ownerList;                                          // List of users/groups
    Blob *blob;                                                     // Blob of info data
    String *name;                                                   // Current info name
    String *linkDestination;                                        // Current link destination
    String *versionId;                                              // Current version id
};

/***********************************************************************************************************************************
A more space efficient version of StorageInfo. Each level is contained in a struct to ensure alignment when only using part of the
struct to store info. This also keeps the size calculations accurate if members are added to a level.
***********************************************************************************************************************************/
typedef struct StorageListInfo
{
    // Set when info type >= storageInfoLevelExists
    struct
    {
        const char *name;                                           // Name of path/file/link
    } exists;

    // Set when info type >= storageInfoLevelType (undefined at lower levels). Mode is only provided at higher detail levels but
    // included here to save space on 64-bit architectures
    struct
    {
        // Set when info type >= storageInfoLevelType (undefined at lower levels)
        StorageType type;                                           // Type file/path/link)

        // Set when info type >= storageInfoLevelDetail (undefined at lower levels)
        mode_t mode;                                                // Mode of path/file/link
    } type;

    // Set when info type >= storageInfoLevelBasic (undefined at lower levels)
    struct
    {
        uint64_t size;                                              // Size (path/link is 0)
        time_t timeModified;                                        // Time file was last modified
        const char *versionId;                                      // Version id when versioning enabled
    } basic;

    // Set when info type >= storageInfoLevelDetail (undefined at lower levels)
    struct
    {
        const String *user;                                         // Name of user that owns the file
        const String *group;                                        // Name of group that owns the file
        uid_t userId;                                               // User that owns the file
        uid_t groupId;                                              // Group that owns the file
        const char *linkDestination;                                // Destination if this is a link
    } detail;
} StorageListInfo;

// Determine size of struct to store in the list. We only need to store the members that are used by the current level.
static const uint8_t storageLstInfoSize[storageInfoLevelDetail + 1] =
{
    0,                                                              // storageInfoLevelDefault (should not be used)
    offsetof(StorageListInfo, type),                                // storageInfoLevelExists
    offsetof(StorageListInfo, basic),                               // storageInfoLevelType
    offsetof(StorageListInfo, detail),                              // storageInfoLevelBasic
    sizeof(StorageListInfo),                                        // storageInfoLevelDetail
};

/**********************************************************************************************************************************/
FN_EXTERN StorageList *
storageLstNew(const StorageInfoLevel level)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, level);
    FUNCTION_TEST_END();

    ASSERT(level != storageInfoLevelDefault);

    OBJ_NEW_BEGIN(StorageList, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (StorageList)
        {
            .pub =
            {
                .list = lstNewP(storageLstInfoSize[level], .comparator = lstComparatorZ),
                .level = level,
            },
            .ownerList = strLstNew(),
            .blob = blbNew(),
            .name = strNew(),
            .linkDestination = strNew(),
            .versionId = strNew(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(STORAGE_LIST, this);
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageLstInsert(StorageList *const this, const unsigned int idx, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_LIST, this);
        FUNCTION_TEST_PARAM(UINT, idx);
        FUNCTION_TEST_PARAM_P(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(info != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this->pub.list)
    {
        StorageListInfo listInfo = {.exists = {.name = blbAdd(this->blob, strZ(info->name), strSize(info->name) + 1)}};

        switch (storageLstLevel(this))
        {
            case storageInfoLevelDetail:
            {
                listInfo.type.mode = info->mode;
                listInfo.detail.user = strLstAddIfMissing(this->ownerList, info->user);
                listInfo.detail.group = strLstAddIfMissing(this->ownerList, info->group);
                listInfo.detail.userId = info->userId;
                listInfo.detail.groupId = info->groupId;

                if (info->linkDestination != NULL)
                {
                    listInfo.detail.linkDestination = blbAdd(
                        this->blob, strZ(info->linkDestination), strSize(info->linkDestination) + 1);
                }
            }

            case storageInfoLevelBasic:
            {
                listInfo.basic.size = info->size;
                listInfo.basic.timeModified = info->timeModified;

                if (info->versionId != NULL)
                    listInfo.basic.versionId = blbAdd(this->blob, strZ(info->versionId), strSize(info->versionId) + 1);
            }

            case storageInfoLevelType:
                listInfo.type.type = info->type;

            default:
                break;
        }

        lstInsert(this->pub.list, idx, &listInfo);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN StorageInfo
storageLstGet(const StorageList *const this, const unsigned int idx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_LIST, this);
        FUNCTION_TEST_PARAM(UINT, idx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const StorageListInfo *const listInfo = lstGet(this->pub.list, idx);

    StorageInfo result =
    {
        .name = strCatZ(strTrunc(this->name), listInfo->exists.name),
        .exists = true,
        .level = storageLstLevel(this)
    };

    switch (result.level)
    {
        case storageInfoLevelDetail:
        {
            result.mode = listInfo->type.mode;
            result.user = listInfo->detail.user;
            result.group = listInfo->detail.group;
            result.userId = listInfo->detail.userId;
            result.groupId = listInfo->detail.groupId;

            if (listInfo->detail.linkDestination != NULL)
                result.linkDestination = strCatZ(strTrunc(this->linkDestination), listInfo->detail.linkDestination);
        }

        case storageInfoLevelBasic:
        {
            result.size = listInfo->basic.size;
            result.timeModified = listInfo->basic.timeModified;

            if (listInfo->basic.versionId != NULL)
                result.versionId = strCatZ(strTrunc(this->versionId), listInfo->basic.versionId);
        }

        case storageInfoLevelType:
            result.type = listInfo->type.type;

        default:
            break;
    }

    FUNCTION_TEST_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
FN_EXTERN StorageInfo
storageLstFind(const StorageList *const this, const String *const name)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_LIST, this);
        FUNCTION_TEST_PARAM(STRING, name);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(name != NULL);

    const char *const namePtr = strZ(name);
    const unsigned int listIdx = lstFindIdx(this->pub.list, &namePtr);

    if (listIdx == LIST_NOT_FOUND)
        FUNCTION_TEST_RETURN(STORAGE_INFO, (StorageInfo){.exists = false});

    FUNCTION_TEST_RETURN(STORAGE_INFO, storageLstGet(this, listIdx));
}

/**********************************************************************************************************************************/
FN_EXTERN void
storageLstToLog(const StorageList *const this, StringStatic *const debugLog)
{
    strStcFmt(debugLog, "{size: %u}", lstSize(this->pub.list));
}
