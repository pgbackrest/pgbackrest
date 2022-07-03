/***********************************************************************************************************************************
Storage List
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
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
        const String *name;                                         // Name of path/file/link
    } exists;

    // Mode is only provided at detail level but is included here to save space on 64-bit architectures
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
    } basic;

    // Set when info type >= storageInfoLevelDetail (undefined at lower levels)
    struct
    {
        const String *user;                                         // Name of user that owns the file
        const String *group;                                        // Name of group that owns the file
        uid_t userId;                                               // User that owns the file
        uid_t groupId;                                              // Group that owns the file
        const String *linkDestination;                              // Destination if this is a link
    } detail;
} StorageListInfo;

// Determine size of struct to store in the list. We only need to store the members that are used by the current level.
static size_t
storageLstInfoSize(const StorageInfoLevel level)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, level);
    FUNCTION_TEST_END();

    switch (level)
    {
        case storageInfoLevelExists:
            FUNCTION_TEST_RETURN(SIZE, offsetof(StorageListInfo, type));

        case storageInfoLevelType:
            FUNCTION_TEST_RETURN(SIZE, offsetof(StorageListInfo, basic));

        case storageInfoLevelBasic:
            FUNCTION_TEST_RETURN(SIZE, offsetof(StorageListInfo, detail));

        default:
            ASSERT(level == storageInfoLevelDetail);
            FUNCTION_TEST_RETURN(SIZE, sizeof(StorageListInfo));
    }
}

/**********************************************************************************************************************************/
StorageList *
storageLstNew(const StorageInfoLevel level)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(ENUM, level);
    FUNCTION_LOG_END();

    StorageList *this = NULL;

    OBJ_NEW_BEGIN(StorageList, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (StorageList)
        {
            .pub =
            {
                .list = lstNewP(storageLstInfoSize(level), .comparator =  lstComparatorStr),
                .level = level,
            },
            .ownerList = strLstNew(),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_LIST, this);
}

/**********************************************************************************************************************************/
void
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
        StorageListInfo listInfo = {.exists = {.name = strDup(info->name)}};

        if (storageLstLevel(this) >= storageInfoLevelType)
            listInfo.type.type = info->type;

        if (storageLstLevel(this) >= storageInfoLevelBasic)
        {
            listInfo.basic.size = info->size;
            listInfo.basic.timeModified = info->timeModified;
        }

        if (storageLstLevel(this) >= storageInfoLevelDetail)
        {
            listInfo.type.mode = info->mode;
            listInfo.detail.user = strLstAddIfMissing(this->ownerList, info->user);
            listInfo.detail.group = strLstAddIfMissing(this->ownerList, info->group);
            listInfo.detail.userId = info->userId;
            listInfo.detail.groupId = info->groupId;
            listInfo.detail.linkDestination = strDup(info->linkDestination);
        }

        lstInsert(this->pub.list, idx, &listInfo);
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN_VOID();
}

/**********************************************************************************************************************************/
StorageInfo
storageLstGet(const StorageList *const this, const unsigned int idx)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_LIST, this);
        FUNCTION_TEST_PARAM(UINT, idx);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    const StorageListInfo *const listInfo = lstGet(this->pub.list, idx);
    StorageInfo result = {.name = listInfo->exists.name};

    if (storageLstLevel(this) >= storageInfoLevelType)
        result.type = listInfo->type.type;

    if (storageLstLevel(this) >= storageInfoLevelBasic)
    {
        result.size = listInfo->basic.size;
        result.timeModified = listInfo->basic.timeModified;
    }

    if (storageLstLevel(this) >= storageInfoLevelDetail)
    {
        result.mode = listInfo->type.mode;
        result.user = listInfo->detail.user;
        result.group = listInfo->detail.group;
        result.userId = listInfo->detail.userId;
        result.groupId = listInfo->detail.groupId;
        result.linkDestination = listInfo->detail.linkDestination;
    }

    FUNCTION_TEST_RETURN(STORAGE_INFO, result);
}

/**********************************************************************************************************************************/
String *
storageLstToLog(const StorageList *const this)
{
    return strNewFmt("{size: %u}", lstSize(this->pub.list));
}
