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
};

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
                .list = lstNewP(sizeof(StorageInfo), .comparator =  lstComparatorStr),
                .level = level,
            },
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(STORAGE_LIST, this);
}

/**********************************************************************************************************************************/
void
storageLstAdd(StorageList *const this, const StorageInfo *const info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_LIST, this);
        FUNCTION_TEST_PARAM_P(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(info != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this->pub.list)
    {
        StorageInfo infoCopy = *info;
        infoCopy.name = strDup(infoCopy.name);
        infoCopy.user = strDup(infoCopy.user);
        infoCopy.group = strDup(infoCopy.group);
        infoCopy.linkDestination = strDup(infoCopy.linkDestination);

        lstAdd(this->pub.list, &infoCopy);
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

    FUNCTION_TEST_RETURN(STORAGE_INFO, *(StorageInfo *)lstGet(this->pub.list, idx));
}

/**********************************************************************************************************************************/
String *
storageLstToLog(const StorageList *const this)
{
    return strNewFmt("{size: %u}", lstSize(this->pub.list));
}
