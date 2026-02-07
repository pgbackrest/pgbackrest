/***********************************************************************************************************************************
Storage Range
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "storage/range.h"

/**********************************************************************************************************************************/
FN_EXTERN StorageRangeList *
storageRangeListNewOne(const uint64_t offset, const Variant *const limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_TEST_END();

    StorageRangeList *const result = storageRangeListNew();
    storageRangeListAdd(result, offset, limit);

    FUNCTION_TEST_RETURN(STORAGE_RANGE_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN StorageRangeList *
storageRangeListDup(const StorageRangeList *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_RANGE_LIST, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StorageRangeList *const result = storageRangeListNew();

    for (unsigned int rangeIdx = 0; rangeIdx < storageRangeListSize(this); rangeIdx++)
    {
        const StorageRange *const range = storageRangeListGet(this, rangeIdx);
        storageRangeListAdd(result, range->offset, range->limit);
    }

    FUNCTION_TEST_RETURN(STORAGE_RANGE_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN StorageRange *
storageRangeListAdd(StorageRangeList *const this, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_RANGE_LIST, this);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    StorageRange *const rangePrior = storageRangeListEmpty(this) ? NULL : storageRangeListGet(this, storageRangeListSize(this) - 1);

    if (rangePrior != NULL)
    {
        CHECK(AssertError, rangePrior->limit != NULL, "cannot add range after range with NULL limit");
        const uint64_t rangePriorLimit = varUInt64(rangePrior->limit);

        if (offset == rangePrior->offset + rangePriorLimit)
        {
            if (limit != NULL)
            {
                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    rangePrior->limit = varNewUInt64(rangePrior->offset + rangePriorLimit);
                }
                MEM_CONTEXT_OBJ_END();
            }
            else
                rangePrior->limit = NULL;

            FUNCTION_TEST_RETURN(STORAGE_RANGE, rangePrior);
        }

        CHECK(AssertError, offset > rangePrior->offset + rangePriorLimit, "new range must be after prior range");
    }

    StorageRange range = {.offset = offset};

    if (limit != NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            range.limit = varDup(limit);
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN(STORAGE_RANGE, lstAdd((List *)this, &range));
}
