/***********************************************************************************************************************************
Storage Range
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "storage/range.h"

/***********************************************************************************************************************************
Constant range lists that are generally useful
***********************************************************************************************************************************/
STGRNGLST_EXTERN(DEFAULT_STGRNGLST, 0, NULL);

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
                    rangePrior->limit = varNewUInt64(rangePriorLimit + varUInt64(limit));
                }
                MEM_CONTEXT_OBJ_END();
            }
            else
                rangePrior->limit = NULL;

            FUNCTION_TEST_RETURN(STORAGE_RANGE, rangePrior);
        }

        CHECK_FMT(
            AssertError, offset > rangePrior->offset + rangePriorLimit,
            "new range offset %" PRIu64 " must be after prior range (%" PRIu64 "/%" PRIu64 ")", offset, rangePrior->offset,
            rangePriorLimit);
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
