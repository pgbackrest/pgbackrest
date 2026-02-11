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
STGRNGLST1_EXTERN(DEFAULT_STGRNGLST, 0, UINT64_MAX);

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
storageRangeListAdd(StorageRangeList *const this, const uint64_t offset, const uint64_t limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_RANGE_LIST, this);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(UINT64, limit);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // Check if new range can be combined with prior range
    StorageRange *const rangePrior = storageRangeListEmpty(this) ? NULL : storageRangeListGet(this, storageRangeListSize(this) - 1);

    if (rangePrior != NULL)
    {
        CHECK(AssertError, storageRangeLimit(rangePrior), "cannot add range after range with no limit");
        CHECK(AssertError, limit != STORAGE_RANGE_NO_LIMIT, "range with no limit must be first");

        // If new range continues the prior range then combine them
        if (offset == rangePrior->offset + rangePrior->limit)
        {
            rangePrior->limit = rangePrior->limit + limit;

            FUNCTION_TEST_RETURN(STORAGE_RANGE, rangePrior);
        }

        // Check that the new range is after the prior range
        CHECK_FMT(
            AssertError, offset > rangePrior->offset + rangePrior->limit,
            "new range offset %" PRIu64 " must be after prior range (%" PRIu64 "/%" PRIu64 ")", offset, rangePrior->offset,
            rangePrior->limit);
    }

    FUNCTION_TEST_RETURN(STORAGE_RANGE, lstAdd((List *)this, &(StorageRange){.offset = offset, .limit = limit}));
}
