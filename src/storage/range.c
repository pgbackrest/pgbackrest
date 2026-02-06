/***********************************************************************************************************************************
Storage Range
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "storage/range.h"

/**********************************************************************************************************************************/
FN_EXTERN StorageRange *
storageRangeListAdd(StorageRangeList *const this, const uint64_t offset, const Variant *const limit)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_RANGE_LIST, this);
        FUNCTION_LOG_PARAM(UINT64, offset);
        FUNCTION_LOG_PARAM(VARIANT, limit);
    FUNCTION_TEST_END();

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
